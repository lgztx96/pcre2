#pragma once
// This was essentially copied wholesale from the regex-automata crate. Such
// things are quite dubious to do, but it didn't seem appropriate to depend
// on regex-automata here. And the code below is not worth putting into a
// micro-crate IMO. We could depend on the `thread_local` crate (and we did
// at one point), but its memory usage scales with the total number of active
// threads that have ever run a regex search, where as the pool below should
// only scale with the number of simultaneous regex searches. In practice, it
// has been observed that `thread_local` leads to enormous memory usage in
// managed runtimes.

/*!
A thread safe memory pool.

The principal type in this module is a [`Pool`]. It main use case is for
holding a thread safe collection of mutable scratch spaces (usually called
`Cache` in this crate) that PCRE2 needs to execute a search. This avoids
needing to re-create the scratch space for every search, which could wind up
being quite expensive.
*/

/// A thread safe pool.
///
/// Getting a value out comes with a guard. When that guard is dropped, the
/// value is automatically put back in the pool. The guard provides both a
/// `Deref` and a `DerefMut` implementation for easy access to an underlying
/// `T`.
///
/// A `Pool` impls `Sync` when `T` is `Send` (even if `T` is not `Sync`). This
/// is possible because a pool is guaranteed to provide a value to exactly one
/// thread at any time.
///
/// Currently, a pool never contracts in size. Its size is proportional to the
/// maximum number of simultaneous uses. This may change in the future.
///
/// A `Pool` is a particularly useful data structure for this crate because
/// PCRE2 requires a mutable "cache" in order to execute a search. Since
/// regexes themselves tend to be global, the problem is then: how do you get a
/// mutable cache to execute a search? You could:
///
/// 1. Use a `thread_local!`, which requires the standard library and requires
/// that the regex pattern be statically known.
/// 2. Use a `Pool`.
/// 3. Make the cache an explicit dependency in your code and pass it around.
/// 4. Put the cache state in a `Mutex`, but this means only one search can
/// execute at a time.
/// 5. Create a new cache for every search.
///
/// A `thread_local!` is perhaps the best choice if it works for your use case.
/// Putting the cache in a mutex or creating a new cache for every search are
/// perhaps the worst choices. Of the remaining two choices, whether you use
/// this `Pool` or thread through a cache explicitly in your code is a matter
/// of taste and depends on your code architecture.
#include <atomic>
#include <vector>
#include <optional>
#include <memory>
#include <mutex>
#include <expected>
#include <functional>

namespace inner
{
	/// An atomic counter used to allocate thread IDs.
	///
	/// We specifically start our counter at 3 so that we can use the values
	/// less than it as sentinels.
	static std::atomic<size_t> COUNTER = std::atomic<size_t>(3);

	/// A thread ID indicating that there is no owner. This is the initial
	/// state of a pool. Once a pool has an owner, there is no way to change
	/// it.
	static size_t THREAD_ID_UNOWNED = 0;

	/// A thread ID indicating that the special owner value is in use and not
	/// available. This state is useful for avoiding a case where the owner
	/// of a pool calls `get` before putting the result of a previous `get`
	/// call back into the pool.
	static constexpr size_t THREAD_ID_INUSE = 1;

	/// This sentinel is used to indicate that a guard has already been dropped
	/// and should not be re-dropped. We use this because our drop code can be
	/// called outside of Drop and thus there could be a bug in the internal
	/// implementation that results in trying to put the same guard back into
	/// the same pool multiple times, and *that* could result in UB if we
	/// didn't mark the guard as already having been put back in the pool.
	///
	/// So this isn't strictly necessary, but this let's us define some
	/// routines as safe (like PoolGuard::put_imp) that we couldn't otherwise
	/// do.
	static constexpr size_t THREAD_ID_DROPPED = 2;

	/// The number of stacks we use inside of the pool. These are only used for
	/// non-owners. That is, these represent the "slow" path.
	///
	/// In the original implementation of this pool, we only used a single
	/// stack. While this might be okay for a couple threads, the prevalence of
	/// 32, 64 and even 128 core CPUs has made it untenable. The contention
	/// such an environment introduces when threads are doing a lot of searches
	/// on short haystacks (a not uncommon use case) is palpable and leads to
	/// huge slowdowns.
	///
	/// This constant reflects a change from using one stack to the number of
	/// stacks that this constant is set to. The stack for a particular thread
	/// is simply chosen by `thread_id % MAX_POOL_STACKS`. The idea behind
	/// this setup is that there should be a good chance that accesses to the
	/// pool will be distributed over several stacks instead of all of them
	/// converging to one.
	///
	/// This is not a particularly smart or dynamic strategy. Fixing this to a
	/// specific number has at least two downsides. First is that it will help,
	/// say, an 8 core CPU more than it will a 128 core CPU. (But, crucially,
	/// it will still help the 128 core case.) Second is that this may wind
	/// up being a little wasteful with respect to memory usage. Namely, if a
	/// regex is used on one thread and then moved to another thread, then it
	/// could result in creating a new copy of the data in the pool even though
	/// only one is actually needed.
	///
	/// And that memory usage bit is why this is set to 8 and not, say, 64.
	/// Keeping it at 8 limits, to an extent, how much unnecessary memory can
	/// be allocated.
	///
	/// In an ideal world, we'd be able to have something like this:
	///
	/// * Grow the number of stacks as the number of concurrent callers
	/// increases. I spent a little time trying this, but even just adding an
	/// atomic addition/subtraction for each pop/push for tracking concurrent
	/// callers led to a big perf hit. Since even more work would seemingly be
	/// required than just an addition/subtraction, I abandoned this approach.
	/// * The maximum amount of memory used should scale with respect to the
	/// number of concurrent callers and *not* the total number of existing
	/// threads. This is primarily why the `thread_local` crate isn't used, as
	/// as some environments spin up a lot of threads. This led to multiple
	/// reports of extremely high memory usage (often described as memory
	/// leaks).
	/// * Even more ideally, the pool should contract in size. That is, it
	/// should grow with bursts and then shrink. But this is a pretty thorny
	/// issue to tackle and it might be better to just not.
	/// * It would be nice to explore the use of, say, a lock-free stack
	/// instead of using a mutex to guard a `Vec` that is ultimately just
	/// treated as a stack. The main thing preventing me from exploring this
	/// is the ABA problem. The `crossbeam` crate has tools for dealing with
	/// this sort of problem (via its epoch based memory reclamation strategy),
	/// but I can't justify bringing in all of `crossbeam` as a dependency of
	/// `regex` for this.
	///
	/// See this issue for more context and discussion:
	/// https://github.com/rust-lang/regex/issues/934
	const constexpr size_t MAX_POOL_STACKS = 8;

	//thread_local!(
		/// A thread local used to assign an ID to a thread.
	thread_local size_t THREAD_ID = [] {
		auto next = COUNTER.fetch_add(1, std::memory_order::relaxed);
		// SAFETY: We cannot permit the reuse of thread IDs since reusing a
		// thread ID might result in more than one thread "owning" a pool,
		// and thus, permit accessing a mutable value from multiple threads
		// simultaneously without synchronization. The intent of this panic
		// is to be a sanity check. It is not expected that the thread ID
		// space will actually be exhausted in practice. Even on a 32-bit
		// system, it would require spawning 2^32 threads (although they
		// wouldn't all need to run simultaneously, so it is in theory
		// possible).
		//
		// This checks that the counter never wraps around, since atomic
		// addition wraps around on overflow.
		if (next == 0) {
			throw std::exception("regex: thread ID allocation space exhausted");
		}
		return next;
		}();
	//);

		/// This puts each stack in the pool below into its own cache line. This is
		/// an absolutely critical optimization that tends to have the most impact
		/// in high contention workloads. Without forcing each mutex protected
		/// into its own cache line, high contention exacerbates the performance
		/// problem by causing "false sharing." By putting each mutex in its own
		/// cache-line, we avoid the false sharing problem and the affects of
		/// contention are greatly reduced.


	template<typename T>
	struct __declspec(align(64)) CacheLine
	{
		T value;
	};

	template<typename T, typename M = std::mutex>
	class Mutex
	{
	public:

		Mutex() : contents(new T()), m(new M()) {}

		Mutex(const Mutex& other) = delete;
		Mutex operator=(const Mutex& other) = delete;

		Mutex(Mutex&& other) : contents(other.contents), m(other.m)
		{
			other.contents = nullptr;
			other.m = nullptr;
		}

		Mutex operator=(Mutex&& other)
		{
			return std::move(other);
		}

		//template <typename... Args>
		//Mutex(Args&&... args) : contents(new T(std::forward<Args>(args)...)), m(new M()) {}
		~Mutex()
		{
			delete m;
			delete contents;
		}

		struct Locker
		{
			T* operator->() { return contents; }
			std::unique_lock<M> lock;
			T* contents;
		};

		Locker Acquire() { return { std::unique_lock(*m), contents }; }

		bool try_lock() const
		{
			return m->try_lock();
		}

		void unlock() const
		{
			m->unlock();
		}

		Locker operator->() { return Acquire(); }
		//T Copy() { return Acquire().contents; }
		T* Ref() { return contents; }

	private:
		T* contents;
		M* m;
	};

	/// A thread safe pool utilizing std-only features.
	///
	/// The main difference between this and the simplistic alloc-only pool is
	/// the use of std::sync::Mutex and an "owner thread" optimization that
	/// makes accesses by the owner of a pool faster than all other threads.
	/// This makes the common case of running a regex within a single thread
	/// faster by avoiding mutex unlocking.
	template<typename T, typename F>
	struct Pool {
		/// A function to create more T values when stack is empty and a caller
		/// has requested a T.
		F create;
		/// Multiple stacks of T values to hand out. These are used when a Pool
		/// is accessed by a thread that didn't create it.
		///
		/// Conceptually this is `Mutex<Vec<Box<T>>>`, but sharded out to make
		/// it scale better under high contention work-loads. We index into
		/// this sequence via `thread_id % stacks.len()`.
		std::vector<CacheLine<Mutex<std::vector<std::unique_ptr<T>>>>> stacks;
		/// The ID of the thread that owns this pool. The owner is the thread
		/// that makes the first call to 'get'. When the owner calls 'get', it
		/// gets 'owner_val' directly instead of returning a T from 'stack'.
		/// See comments elsewhere for details, but this is intended to be an
		/// optimization for the common case that makes getting a T faster.
		///
		/// It is initialized to a value of zero (an impossible thread ID) as a
		/// sentinel to indicate that it is unowned.
		std::atomic<size_t> owner;
		/// A value to return when the caller is in the same thread that
		/// first called `Pool::get`.
		///
		/// This is set to None when a Pool is first created, and set to Some
		/// once the first thread calls Pool::get.
		std::optional<std::unique_ptr<T>> owner_val;// : UnsafeCell<Option<T>>,

		struct PoolGuard
		{
			/// The pool that this guard is attached to.
			Pool<T, F>& pool;
			/// This is Err when the guard represents the special "owned" value.
			/// In which case, the value is retrieved from 'pool.owner_val'. And
			/// in the special case of `Err(THREAD_ID_DROPPED)`, it means the
			/// guard has been put back into the pool and should no longer be used.
			std::expected<std::unique_ptr<T>, size_t> v;
			/// When true, the value should be discarded instead of being pushed
			/// back into the pool. We tend to use this under high contention, and
			/// this allows us to avoid inflating the size of the pool. (Because
			/// under contention, we tend to create more values instead of waiting
			/// for access to a stack of existing values.)
			bool discard;

			auto value(this const PoolGuard& self) -> const T&
			{
				if (self.v)
				{
					return *self.v->get();
				}

				auto id = self.v.error();

				return self.pool.owner_val.value();

				//match self.value{
				//	Ok(ref v) = > &**v,
				//	// SAFETY: This is safe because the only way a PoolGuard gets
				//	// created for self.value=Err is when the current thread
				//	// corresponds to the owning thread, of which there can only
				//	// be one. Thus, we are guaranteed to be providing exclusive
				//	// access here which makes this safe.
				//	//
				//	// Also, since 'owner_val' is guaranteed to be initialized
				//	// before an owned PoolGuard is created, the unchecked unwrap
				//	// is safe.
				//	Err(id) = > unsafe {
				//		// This assert is *not* necessary for safety, since we
				//		// should never be here if the guard had been put back into
				//		// the pool. This is a sanity check to make sure we didn't
				//		// break an internal invariant.
				//		debug_assert_ne!(THREAD_ID_DROPPED, id);
				//		(*self.pool.owner_val.get()).as_ref().unwrap_unchecked()
				//	},
				//}
			}

			auto value_mut(this const PoolGuard& self) -> T&
			{
				if (self.v)
				{
					return *self.v->get();
				}

				auto id = self.v.error();

				return *self.pool.owner_val.value();

				//match self.value{
				//	Ok(ref mut v) = > &mut * *v,
				//	// SAFETY: This is safe because the only way a PoolGuard gets
				//	// created for self.value=None is when the current thread
				//	// corresponds to the owning thread, of which there can only
				//	// be one. Thus, we are guaranteed to be providing exclusive
				//	// access here which makes this safe.
				//	//
				//	// Also, since 'owner_val' is guaranteed to be initialized
				//	// before an owned PoolGuard is created, the unwrap_unchecked
				//	// is safe.
				//	Err(id) = > unsafe {
				//		// This assert is *not* necessary for safety, since we
				//		// should never be here if the guard had been put back into
				//		// the pool. This is a sanity check to make sure we didn't
				//		// break an internal invariant.
				//		debug_assert_ne!(THREAD_ID_DROPPED, id);
				//		(*self.pool.owner_val.get()).as_mut().unwrap_unchecked()
				//	},
				//}
			}

			static void put(PoolGuard& value)
			{
				// Since this is effectively consuming the guard and putting the
				// value back into the pool, there's no reason to run its Drop
				// impl after doing this. I don't believe there is a correctness
				// problem with doing so, but there's definitely a perf problem
				// by redoing this work. So we avoid it.
				//auto v = core::mem::ManuallyDrop::new(value);
				value.put_imp();
			}

			inline void put_imp(this PoolGuard& self)
			{
				if (auto& v = self.v)
				{
					if (self.discard)
					{
						return;
					}
					self.pool.put_value(std::move(*v));
					self.v = std::unexpected(THREAD_ID_DROPPED);
				}
				else {
					auto owner = self.v.error();
					self.pool.owner.store(owner, std::memory_order::release);
				}

				//match core::mem::replace(&self.v, Err(THREAD_ID_DROPPED)) {
				//	Ok(value) = > {
				//		// If we were told to discard this value then don't bother
				//		// trying to put it back into the pool. This occurs when
				//		// the pop operation failed to acquire a lock and we
				//		// decided to create a new value in lieu of contending for
				//		// the lock.
				//		if self.discard{
				//			return;
				//		}
				//		self.pool.put_value(value);
				//	}
				//	// If this guard has a value "owned" by the thread, then
				//	// the Pool guarantees that this is the ONLY such guard.
				//	// Therefore, in order to place it back into the pool and make
				//	// it available, we need to change the owner back to the owning
				//	// thread's ID. But note that we use the ID that was stored in
				//	// the guard, since a guard can be moved to another thread and
				//	// dropped. (A previous iteration of this code read from the
				//	// THREAD_ID thread local, which uses the ID of the current
				//	// thread which may not be the ID of the owning thread! This
				//	// also avoids the TLS access, which is likely a hair faster.)
				//	Err(owner) = > {
				//		// If we hit this point, it implies 'put_imp' has been
				//		// called multiple times for the same guard which in turn
				//		// corresponds to a bug in this implementation.
				//		assert_ne!(THREAD_ID_DROPPED, owner);
				//		self.pool.owner.store(owner, std::memory_order::release);
				//	}
			}

			~PoolGuard()
			{
				put_imp();
			}
		};

		Pool(F create) : owner(THREAD_ID_UNOWNED)
		{
			// MSRV(1.63): Mark this function as 'const'. I've arranged the
			// code such that it should "just work." Then mark the public
			// 'Pool::new' method as 'const' too. (The alloc-only Pool::new
			// is already 'const', so that should "just work" too.) The only
			// thing we're waiting for is Mutex::new to be const.

			stacks.reserve(MAX_POOL_STACKS);

			for (size_t i = 0; i < stacks.capacity(); i++)
			{
				stacks.emplace_back(Mutex<std::vector<std::unique_ptr<T>>>());
			}

			this->create = create;
		}

		//static auto new1(F create) -> Pool<T, F>
		//{
		//	// MSRV(1.63): Mark this function as 'const'. I've arranged the
		//	// code such that it should "just work." Then mark the public
		//	// 'Pool::new' method as 'const' too. (The alloc-only Pool::new
		//	// is already 'const', so that should "just work" too.) The only
		//	// thing we're waiting for is Mutex::new to be const.
		//	std::vector<CacheLine<Mutex<std::vector<std::unique_ptr<T>>>>> stacks;
		//	stacks.reserve(MAX_POOL_STACKS);

		//	for (size_t i = 0; i < stacks.capacity(); i++)
		//	{
		//		stacks.push_back(CacheLine(Mutex(std::vector<std::unique_ptr<T>>())));
		//	}

		//	auto owner = std::atomic<size_t>(THREAD_ID_UNOWNED);
		//	auto owner_val = std::nullopt;// UnsafeCell::new(None); // init'd on first access
		//	return Pool{ create, stacks, THREAD_ID_UNOWNED, /*owner,*/ owner_val };
		//}

		auto get(this Pool& self) -> PoolGuard
		{
			// Our fast path checks if the caller is the thread that "owns"
			// this pool. Or stated differently, whether it is the first thread
			// that tried to extract a value from the pool. If it is, then we
			// can return a T to the caller without going through a mutex.
			//
			// SAFETY: We must guarantee that only one thread gets access
			// to this value. Since a thread is uniquely identified by the
			// THREAD_ID thread local, it follows that if the caller's thread
			// ID is equal to the owner, then only one thread may receive this
			// value. This is also why we can get away with what looks like a
			// racy load and a store. We know that if 'owner == caller', then
			// only one thread can be here, so we don't need to worry about any
			// other thread setting the owner to something else.
			auto caller = THREAD_ID;// .with(| id | *id);
			auto owner = self.owner.load(std::memory_order::acquire);
			if (caller == owner) {
				// N.B. We could also do a CAS here instead of a load/store,
				// but ad hoc benchmarking suggests it is slower. And a lot
				// slower in the case where `get_slow` is common.
				self.owner.store(THREAD_ID_INUSE, std::memory_order::release);
				return self.guard_owned(caller);
			}
			return self.get_slow(caller, owner);
		}

		auto get_slow(
			this Pool& self,
			size_t caller,
			size_t owner
		) -> PoolGuard {
			if (owner == THREAD_ID_UNOWNED) {
				// This sentinel means this pool is not yet owned. We try to
				// atomically set the owner. If we do, then this thread becomes
				// the owner and we can return a guard that represents the
				// special T for the owner.
				//
				// Note that we set the owner to a different sentinel that
				// indicates that the owned value is in use. The owner ID will
				// get updated to the actual ID of this thread once the guard
				// returned by this function is put back into the pool.
				auto res = self.owner.compare_exchange_strong(
					THREAD_ID_UNOWNED,
					THREAD_ID_INUSE,
					std::memory_order::acq_rel,
					std::memory_order::acquire
				);
				if (res) {
					// SAFETY: A successful CAS above implies this thread is
					// the owner and that this is the only such thread that
					// can reach here. Thus, there is no data race.

					self.owner_val = std::move(std::unique_ptr<T>(self.create()));// std::invoke(self.create); self.create();
					return self.guard_owned(caller);
				}
			}
			auto stack_id = caller % self.stacks.size();
			// We try to acquire exclusive access to this thread's stack, and
			// if so, grab a value from it if we can. We put this in a loop so
			// that it's easy to tweak and experiment with a different number
			// of tries. In the end, I couldn't see anything obviously better
			// than one attempt in ad hoc testing.

			for (int i = 0; i < 10; i++)
			{
				auto& stack = self.stacks[stack_id].value;
				/*	{
						Err(_) = > continue,
							Ok(stack) = > stack,
					};*/
				if (stack.try_lock()) {
					if (auto v = stack.Ref(); !v->empty()) {
						std::unique_ptr s(std::move(v->back()));
						v->pop_back();
						stack.unlock();
						return self.guard_stack(std::move(s));
					}
				}

				// Unlock the mutex guarding the stack before creating a fresh
				// value since we no longer need the stack.
				stack.unlock();
				auto value = std::unique_ptr<T>
					(
						self.create()
						//(self.create)()
					);
				return self.guard_stack(std::move(value));
			}
			// We're only here if we could get access to our stack, so just
			// create a new value. This seems like it could be wasteful, but
			// waiting for exclusive access to a stack when there's high
			// contention is brutal for perf.
			return self.guard_stack_transient(std::unique_ptr<T>(
				self.create()
				//(self.create)()
			));
		}

		/// Puts a value back into the pool. Callers don't need to call this.
		/// Once the guard that's returned by 'get' is dropped, it is put back
		/// into the pool automatically.

		void put_value(this Pool& self, std::unique_ptr<T> value) {
			auto caller = THREAD_ID;// .with(| id | *id);
			auto stack_id = caller % self.stacks.size();
			// As with trying to pop a value from this thread's stack, we
			// merely attempt to get access to push this value back on the
			// stack. If there's too much contention, we just give up and throw
			// the value away.
			//
			// Interestingly, in ad hoc benchmarking, it is beneficial to
			// attempt to push the value back more than once, unlike when
			// popping the value. I don't have a good theory for why this is.
			// I guess if we drop too many values then that winds up forcing
			// the pop operation to create new fresh values and thus leads to
			// less reuse. There's definitely a balancing act here.
			for (int i = 0; i < 10; i++)
			{
				auto& s = self.stacks[stack_id];
				auto& stack = s.value;
				if (stack.try_lock())
				{
					stack.Ref()->push_back(std::move(value));
					stack.unlock();
					return;
				}
			}
		}

		/// Create a guard that represents the special owned T.

		auto guard_owned(this Pool& self, size_t caller) -> PoolGuard
		{
			return PoolGuard{ .pool = self, .v = std::unexpected(caller), .discard = false };
		}

		/// Create a guard that contains a value from the pool's stack.

		auto guard_stack(this Pool& self, std::unique_ptr<T> value) -> PoolGuard
		{
			return PoolGuard{ .pool = self, .v = std::move(value), .discard = false };
		}

		/// Create a guard that contains a value from the pool's stack with an
		/// instruction to throw away the value instead of putting it back
		/// into the pool.

		auto guard_stack_transient(this Pool& self, std::unique_ptr<T> value) -> PoolGuard
		{
			return PoolGuard{ .pool = self, .v = std::move(value), .discard = true };
		}
	};
}

template <class T, class F>
struct Pool
{
	std::unique_ptr<inner::Pool<T, F>> pool;

	Pool() = default;

	Pool(Pool&& other) noexcept
	{
		pool = std::move(other.pool);
	}

	Pool(std::unique_ptr<inner::Pool<T, F>> pool) : pool(std::move(pool)) {}

	struct PoolGuard
	{
		inner::Pool<T, F>::PoolGuard value;

		static void put(PoolGuard& value)
		{
			inner::Pool<T, F>::PoolGuard::put(value.value);
		}

		auto operator*() const -> T& {
			return value.value_mut();
		}

		auto operator->() const -> T* {
			return &value.value_mut();
		}

		auto deref(this const PoolGuard& self) -> const T& {
			return self.value.value();
		}

		auto deref_mut(this PoolGuard& self) -> T& {
			return self.value.value_mut();
		}
	};

	static auto create(F create) -> Pool<T, F>
	{
		return Pool(std::move(std::make_unique<inner::Pool<T, F>>(create)));
	}

	auto get(this const Pool& self) -> PoolGuard
	{
		return PoolGuard(self.pool->get());
	}

	auto operator() (this const Pool& self) -> PoolGuard {
		return PoolGuard(self.pool->get());
	}
};



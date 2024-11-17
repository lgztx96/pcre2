#pragma once

template<typename T>
struct Mutex {
    std::atomic<bool> locked;
    UnsafeCell<T> data;
}

// SAFETY: Since a Mutex guarantees exclusive access, as long as we can
// send it across threads, it must also be Sync.
unsafe impl<T: Send> Sync for Mutex<T> {}

impl<T> Mutex<T> {
    /// Create a new mutex for protecting access to the given value across
    /// multiple threads simultaneously.
    const fn new(value: T)->Mutex<T>{
        Mutex {
            locked: AtomicBool::new(false),
            data : UnsafeCell::new(value),
        }
    }

        /// Lock this mutex and return a guard providing exclusive access to
        /// `T`. This blocks if some other thread has already locked this
        /// mutex.
#[inline]
        fn lock(&self)->MutexGuard < '_, T> {
        while self
            .locked
            .compare_exchange(
                false,
                true,
                Ordering::AcqRel,
                Ordering::Acquire,
                )
            .is_err()
        {
            core::hint::spin_loop();
        }
    // SAFETY: The only way we're here is if we successfully set
    // 'locked' to true, which implies we must be the only thread here
    // and thus have exclusive access to 'data'.
    let data = unsafe{ &mut * self.data.get() };
    MutexGuard{ locked: &self.locked, data }
}
    }

    /// A guard that derefs to &T and &mut T. When it's dropped, the lock is
    /// released.
#[derive(Debug)]
    struct MutexGuard < 'a, T> {
        locked : &'a AtomicBool,
        data : &'a mut T,
    }

    impl<'a, T> core::ops::Deref for MutexGuard<'a, T> {
        type Target = T;

#[inline]
        fn deref(&self) ->& T{
            self.data
        }
    }

    impl<'a, T> core::ops::DerefMut for MutexGuard<'a, T> {
#[inline]
        fn deref_mut(&mut self) ->& mut T{
            self.data
        }
    }

    impl<'a, T> Drop for MutexGuard<'a, T> {
#[inline]
        fn drop(&mut self) {
            // Drop means 'data' is no longer accessible, so we can unlock
            // the mutex.
            self.locked.store(false, Ordering::Release);
        }
    }
}

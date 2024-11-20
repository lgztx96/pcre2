#pragma once
#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 0
#include "capture_locations.h"
#include "regex_builder.h"
#include "captures.h"
#include "code.h"
#include "config.h"
#include "match_data.h"
#include <map>
#include <pcre2.h>
#include "pool.h"

namespace pcre2 {

	/// The type of the closure we use to create new caches. We need to spell out
	/// all of the marker traits or else we risk leaking !MARKER impls.
	using MatchDataPoolFn = std::function<MatchData* ()>;
	//using MatchDataPoolFn = decltype([]() ->std::unique_ptr<MatchData> {});

	using MatchDataPool = Pool<MatchData, MatchDataPoolFn>;

	/// Same as above, but for the guard returned by a pool.
	using MatchDataPoolGuard = Pool<MatchData, MatchDataPoolFn>::PoolGuard;

	bool is_jit_available() {
		uint32_t rc = 0;
		auto error_code = pcre2_config_16(PCRE2_CONFIG_JIT, &rc);
		if (error_code < 0) {
			// If PCRE2_CONFIG_JIT is a bad option, then there's a bug somewhere.
			//panic!("BUG: {}", Error::jit(error_code));
		}
		return rc == 1;
	}

	template<typename T> struct traits;

	template<> struct traits<wchar_t>
	{
		typedef ::pcre2_code_16* handle;
		typedef const void* extra;

		typedef wchar_t* char_ptr;
		typedef const wchar_t* const_char_ptr;

		typedef std::wstring string;

		static handle compile(const_char_ptr pattern, int options,
			int* error, const char** help, int* offset,
			const unsigned char* table)
		{
			// return (::pcre16_compile2 (to(pattern), options, error, help, offset, table));
		}

		static void release(handle pattern)
		{
			::pcre2_code_free_16(pattern);
		}

		static int query(handle pattern, extra extra, int what, void* where)
		{
			// return (::pcre16_fullinfo(pattern, extra, what, where));
		}

		static int string_number(handle pattern, const_char_ptr name)
		{
			// return (::pcre16_get_stringnumber(pattern, to(name)));
		}

		static int execute(handle pattern, extra extra,
			const_char_ptr data, int size, int base,
			int options, int* results, int count)
		{
			// return (::pcre16_exec(pattern, extra, to(data), size, base, options, results, count));
		}

		static int table_offset()
		{
			return (1);
		}

	private:
		static const_char_ptr from(PCRE2_SPTR16 pointer)
		{
			return (reinterpret_cast<const_char_ptr>(pointer));
		}

		static PCRE2_SPTR16 to(const_char_ptr pointer)
		{
			return (reinterpret_cast<PCRE2_SPTR16>(pointer));
		}
	};

	std::wstring escape(std::wstring_view pattern) {
		auto is_meta_character = [](char c) -> bool {
			switch (c)
			{
			case '\\':
			case '.':
			case '+':
			case '*':
			case '?':
			case '(':
			case ')':
			case '|':
			case '[':
			case ']':
			case '{':
			case '}':
			case '^':
			case '$':
			case '#':
			case '-':
				return true;
			default:
				return false;
			}
			};

		// Is it really true that PCRE2 doesn't have an API routine to
		// escape a pattern so that it matches literally? Wow. I couldn't
		// find one. It does of course have \Q...\E, but, umm, what if the
		// literal contains a \E?
		std::wstring quoted;
		quoted.reserve(pattern.size());
		for (auto c : pattern) {
			if (is_meta_character(c)) {
				quoted.append(1, '\\');
			}
			quoted.append(1, c);
		}
		return quoted;
	}

	class wregex
	{

	private:
		/// The configuration used to build the regex.
		Config config;
		/// The original pattern string.
		std::wstring pattern;
		/// The underlying compiled PCRE2 object.
		std::unique_ptr<Code> code;
		/// The capture group names for this regex.
		std::unique_ptr<std::vector<std::wstring>> capture_names;
		/// A map from capture group name to capture group index.
		std::unique_ptr<std::map<std::wstring, size_t, std::less<void>>> capture_names_idx;
		/// A pool of mutable scratch data used by PCRE2 during matching.
		   // MatchDataPool match_data;
		MatchDataPool match_data;

		wregex(Config config,
			std::wstring_view pattern,
			std::unique_ptr<Code> code,
			std::unique_ptr<std::vector<std::wstring>> capture_names,
			std::unique_ptr<std::map<std::wstring, size_t, std::less<void>>> capture_names_idx,
			MatchDataPool data
		) noexcept
			: config(config)
			, pattern(pattern)
			, code(std::move(code))
			, capture_names(std::move(capture_names))
			, capture_names_idx(std::move(capture_names_idx))
			, match_data(std::move(data))

		{
		}

	public:

		wregex(wregex&& regex) noexcept : match_data(std::move(regex.match_data))
		{
			config = regex.config;
			pattern = regex.pattern;
			code = std::move(regex.code);
			capture_names = std::move(regex.capture_names);
			capture_names_idx = std::move(regex.capture_names_idx);
		}

		wregex(const wregex& rhs) = delete;
		wregex operator=(const wregex& rhs) = delete;

		auto find_at_with_match_data(
			this const wregex& self,
			const MatchDataPoolGuard& match_data,
			std::wstring_view subject,
			size_t start
		) -> std::expected<std::optional<Match>, Error> {
			assert(
				start <= subject.size(),
				"start ({}) must be <= subject.len() ({})",
				start,
				subject.size()
			);

			uint32_t options = 0;
			// SAFETY: We don't use any dangerous PCRE2 options.
			return match_data->find(
				self.code.get(),
				subject,
				start,
				options)
				.transform([&](bool b) -> std::optional<Match>
					{
						if (b) {
							auto ovector = match_data->ovector();
							return std::make_optional<Match>(subject.data(), ovector[0], ovector[1]);
						}
						return std::nullopt;
					});
		}

		/// Returns the same as `captures_read`, but starts the search at the given
		/// offset and populates the capture locations given.
		///
		/// The significance of the starting point is that it takes the surrounding
		/// context into consideration. For example, the `\A` anchor can only
		/// match when `start == 0`.
		auto captures_read_at(
			this const wregex& self,
			CaptureLocations& locs,
			std::wstring_view subject,
			size_t start
		) -> std::expected<std::optional<Match>, Error> {
			assert(
				start <= subject.size(),
				"start ({}) must be <= subject.len() ({})",
				start,
				subject.size()
			);

			uint32_t options = 0;
			// SAFETY: We don't use any dangerous PCRE2 options.
			return locs.data->find(
				self.code.get(),
				subject,
				start,
				options)
				.transform([&](bool b) -> std::optional<Match>
					{
						if (b) {
							auto ovector = locs.data->ovector();
							return std::make_optional<Match>(subject.data(), ovector[0], ovector[1]);
						}
						return std::nullopt;
					});
		}

		static inline auto jit_compile(std::wstring_view pattern) -> std::expected<wregex, Error>
		{
			auto options = RegexOptions{};
			options.jit(true);
			return jit_compile(pattern, options);
		}

		static auto jit_compile(std::wstring_view pattern, RegexOptions& s) -> std::expected<wregex, Error>
		{
			uint32_t options = 0;
			Config config = s.config;
			if (config.caseless) {
				options |= PCRE2_CASELESS;
			}
			if (config.dotall) {
				options |= PCRE2_DOTALL;
			}
			if (config.extended) {
				options |= PCRE2_EXTENDED;
			}
			if (config.multi_line) {
				options |= PCRE2_MULTILINE;
			}
			if (config.ucp) {
				options |= PCRE2_UCP;
				options |= PCRE2_UTF;
				options |= PCRE2_MATCH_INVALID_UTF;
			}
			if (config.utf) {
				options |= PCRE2_UTF;
			}

			auto ctx = std::make_unique<CompileContext>();
			if (config.crlf) {
				ctx->set_newline(PCRE2_NEWLINE_ANYCRLF);
				//.expect("PCRE2_NEWLINE_ANYCRLF is a legal value");
			}

			auto code = *Code::make(pattern, options, std::move(ctx));
			switch (config.jit)
			{
			case JITChoice::Never:
				break;
			case JITChoice::Always:
				code->jit_compile();
				break;
			case JITChoice::Attempt:
				if (auto rc = code->jit_compile(); !rc) {
					//log::debug!("JIT compilation failed: {}", err);
				}
				break;
			}

			auto capture_names = code->capture_names();
			auto idx = std::make_unique<std::map<std::wstring, size_t, std::less<void>>>();
			for (size_t i = 0; i < capture_names.size(); i++)
			{
				if (auto name = capture_names[i]; !name.empty())
				{
					idx->emplace(name, i);
				}
			}

			auto match_data = MatchDataPool::create([v = code.get(), c = config.match_config]()
				{
					return new MatchData(c, v);
				});

			return wregex(config, pattern, std::move(code),
				std::make_unique<std::vector<std::wstring>>(std::move(capture_names)),
				std::move(idx),
				std::move(match_data)
			);
		}

		inline auto as_str(this const wregex& self) -> std::wstring_view
		{
			self.pattern;
		}

		auto captures_len(this const wregex& self) -> size_t
		{
			return self.code->capture_count().value();// .expect("a valid capture count from PCRE2")
		}

		/// Returns an empty set of capture locations that can be reused in
		/// multiple calls to `captures_read` or `captures_read_at`.
		inline auto capture_locations(this const wregex& self) -> CaptureLocations
		{
			return CaptureLocations{ self.code.get(), self.new_match_data() };
		}

		inline auto new_match_data(this const wregex& self) -> std::unique_ptr<MatchData>
		{
			return std::make_unique<MatchData>(self.config.match_config, self.code.get());
		}

		// 检查给定的字符串是否匹配正则表达式
		auto is_match_at(
			this const wregex& self,
			std::wstring_view subject,
			size_t start
		) -> std::expected<bool, Error> {
			assert(
				start <= subject.size(),
				"start ({}) must be <= subject.len() ({})",
				start,
				subject.size()
			);

			uint32_t options = 0;
			auto match_data = self.match_data();
			// SAFETY: We don't use any dangerous PCRE2 options.
			auto res =
				match_data->find(self.code.get(), subject, start, options);
			MatchDataPoolGuard::put(match_data);
			return res;
		}

		struct Matches {
			const wregex& re;
			MatchDataPoolGuard match_data;
			std::wstring_view subject;
			size_t last_end;
			std::optional<size_t> last_match;

			struct iterator
			{
				using difference_type = std::ptrdiff_t;
				using element_type = std::expected<Match, Error>;
				using pointer = element_type*;
				using reference = const element_type&;

				reference& operator*(this const iterator& self)
				{
					if (!self.current)
					{
						throw "at the end";
					}
					return self.current.value();
				}

				iterator& operator++()
				{
					if (!current)
					{
						throw "at the end";
					}

					if (const auto& v = matches->next()) {
						current = *v;
					}
					else {
						current = std::nullopt;
					}

					++index;

					return *this;
				}

				void operator++(int) { ++*this; }

				bool operator==(const iterator& iter)
				{
					if (!iter.current && !current) return true;
					return iter.matches == matches && iter.index == index;
				}

				bool operator!=(const iterator& iter)
				{
					if (!iter.current && !current) {
						return false;
					}
					return iter.matches != matches || iter.index != index;
				}

				iterator(Matches* matches,
					std::optional<std::expected<Match, Error>>&& start) noexcept : matches(matches), index(0)
				{
					if (const auto& v = start)
					{
						current = *v;
					}
				}

				iterator(Matches* matches) noexcept : matches(matches), index(-1) {}

				iterator(const iterator& iter) noexcept : matches(iter.matches), index(iter.index)
				{
					if (const auto& v = iter.current)
					{
						current = *v;
					}
				}

				~iterator() = default;

			private:
				Matches* matches;
				std::optional<std::expected<Match, Error>> current;
				int index;
			};

			static_assert(std::input_iterator<iterator>);

			auto begin() { return iterator(this, next()); };

			auto end() { return iterator(this); };

			auto next(this Matches& self) -> std::optional<std::expected<Match, Error>>
			{
				if (self.last_end > self.subject.size())
				{
					return std::nullopt;
				}

				auto res = self.re.find_at_with_match_data(
					self.match_data,
					self.subject,
					self.last_end
				);

				if (!res) {
					return std::unexpected(Error(res.error()));
				}
				else if (!*res) {
					return std::nullopt;
				}

				auto& m = **res;

				if (m.start == m.end) {
					// This is an empty match. To ensure we make progress, start
					// the next search at the smallest possible starting position
					// of the next match following this one.
					self.last_end = m.end + 1;
					// Don't accept empty matches immediately following a match.
					// Just move on to the next match.
					if (self.last_match && m.end == self.last_match) {
						return self.next();
					}
				}
				else {
					self.last_end = m.end;
				}
				self.last_match = m.end;
				return m;
			}
		};

		struct CaptureMatches
		{
			const wregex& re;
			std::wstring_view subject;
			size_t last_end;
			std::optional<size_t> last_match;

			struct iterator
			{
				using difference_type = std::ptrdiff_t;
				using element_type = std::expected<Captures, Error>;
				using pointer = element_type*;
				using reference = element_type&;

				const element_type& operator*(this const iterator& self)
				{
					if (!self.current)
					{
						throw "at the end";
					}
					return self.current.value();
				}

				iterator& operator++(this iterator& self)
				{
					if (!self.current)
					{
						throw "at the end";
					}

					if (auto v = self.matches->next()) {
						self.current = std::move(v);
					}
					else {
						self.current = std::nullopt;
					}

					++self.index;

					return self;
				}

				bool operator==(const iterator& iter)
				{
					if (!iter.current && !current) return true;
					return iter.matches == matches && iter.index == index;
				}

				bool operator!=(const iterator& iter)
				{
					if (!iter.current && !current) {
						return false;
					}
					return iter.matches != matches || iter.index != index;
				}

				iterator(CaptureMatches* matches,
					std::optional<std::expected<Captures, Error>>&& start)
					noexcept : matches(matches), current(std::move(start)), index(0)
				{

				}

				iterator(CaptureMatches* matches) noexcept : matches(matches), index(-1) {}

				~iterator() = default;

			private:
				CaptureMatches* matches;
				std::optional<std::expected<Captures, Error>> current;
				int index;
			};

			auto begin() { return iterator(this, next()); };

			auto end() { return iterator(this); };

			auto next(this CaptureMatches& self) -> std::optional<std::expected<Captures, Error>>
			{
				if (self.last_end > self.subject.size()) {
					return std::nullopt;
				}
				auto locs = self.re.capture_locations();
				auto res =
					self.re.captures_read_at(locs, self.subject, self.last_end);

				if (!res) {
					return std::unexpected(Error(res.error()));
				}
				else if (!*res) {
					return std::nullopt;
				}

				auto& m = **res;
				if (m.start == m.end) {
					// This is an empty match. To ensure we make progress, start
					// the next search at the smallest possible starting position
					// of the next match following this one.
					self.last_end = m.end + 1;
					// Don't accept empty matches immediately following a match.
					// Just move on to the next match.
					if (self.last_match && *self.last_match == m.end) {
						return self.next();
					}
				}
				else {
					self.last_end = m.end;
				}
				self.last_match = m.end;
				return Captures{
					self.subject.data(),
					std::move(locs),
					self.re.capture_names_idx.get()
				};
			}
		};

		auto find_at(this const wregex& self,
			std::wstring_view subject,
			size_t start
		) -> std::expected<std::optional<Match>, Error> {
			auto match_data = self.match_data.get();
			auto res =
				self.find_at_with_match_data(match_data, subject, start);
			MatchDataPoolGuard::put(match_data);
			return res;
		}

		inline auto captures_read(
			this const wregex& self,
			CaptureLocations& locs,
			std::wstring_view subject
		) -> std::expected<std::optional<Match>, Error>
		{
			return self.captures_read_at(locs, subject, 0);
		}

		inline auto is_match(this const wregex& self, std::wstring_view subject) -> std::expected<bool, Error>
		{
			return self.is_match_at(subject, 0);
		}

		inline auto find(
			this const wregex& self,
			std::wstring_view subject
		) -> std::expected<std::optional<Match>, Error> {
			return self.find_at(subject, 0);
		}

		inline auto find_iter(this const wregex& self, std::wstring_view subject) -> Matches
		{
			return Matches{
				 .re = self,
				 .match_data = self.match_data.get(),
				 .subject = subject,
				 .last_end = 0,
				 .last_match = std::nullopt,
			};
		}

		auto captures(
			this const wregex& self,
			std::wstring_view subject
		) -> std::expected<std::optional<Captures>, Error> {
			auto locs = self.capture_locations();
			return self.captures_read(locs, subject)->transform(
				[&](auto&)
				{
					return Captures(subject.data(), std::move(locs), self.capture_names_idx.get());
				});
		}

		auto captures_iter(
			this const wregex& self,
			std::wstring_view subject
		) -> CaptureMatches {
			return CaptureMatches{ .re = self, .subject = subject, .last_end = 0, .last_match = std::nullopt };
		}

		auto substitute_with_options(
			this const wregex& self,
			std::wstring_view subject,
			std::wstring_view replacement,
			uint32_t options,
			std::wstring& output) noexcept -> bool {
			PCRE2_SPTR16 subject_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(subject.data());
			PCRE2_SPTR16 replacement_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(replacement.data());
			//pcre2_callout_enumerate_16
			if (output.size() < subject.size()) output.resize(subject.size() + 1);
			size_t outlen = output.size();

			auto match_data = self.new_match_data();

			auto c = match_data->find(self.code.get(), subject, 0, 0);
			if (!c || !*c) return false;

			int rc = pcre2_substitute_16(
				self.code->as_ptr(),
				subject_ptr,
				subject.size(),
				0,
				options | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH,
				match_data->as_mut_ptr(),
				nullptr,
				replacement_ptr,
				replacement.size(),
				reinterpret_cast<PCRE2_UCHAR16*>(output.data()),
				&outlen);
			if (rc >= 0) {
				output.resize(outlen);
				return true;
			}
			else if (rc == PCRE2_ERROR_NOMEMORY)
			{
				output.resize(outlen);

				rc = pcre2_substitute_16(self.code->as_ptr(),
					subject_ptr,
					subject.size(),
					0,
					options,        
					match_data->as_mut_ptr(),
					nullptr,
					replacement_ptr,  
					replacement.size(),
					reinterpret_cast<PCRE2_UCHAR16*>(output.data()),
					&outlen);

				if (rc >= 0) {
					output.resize(outlen);
					return true;
				}
			}

			return false;
		}

		static constexpr uint32_t SUBSTITUTE_MATCHED =
			PCRE2_SUBSTITUTE_MATCHED
			| PCRE2_SUBSTITUTE_EXTENDED
			| PCRE2_SUBSTITUTE_UNKNOWN_UNSET
			| PCRE2_SUBSTITUTE_UNSET_EMPTY;

		auto substitute(this const wregex& self,
			std::wstring_view subject,
			std::wstring_view replacement, 
			std::wstring& output) -> bool {
			return self.substitute_with_options(
				subject, 
				replacement, 
				SUBSTITUTE_MATCHED, 
				output);
		}

		auto substitute_all(this const wregex& self,
			std::wstring_view subject,
			std::wstring_view replacement,
			std::wstring& output) -> bool {

			return self.substitute_with_options(
				subject,
				replacement, 
				SUBSTITUTE_MATCHED | PCRE2_SUBSTITUTE_GLOBAL,
				output);
		}

		struct Split {
			Matches finder;
			size_t last;

			struct iterator
			{
				using difference_type = std::ptrdiff_t;
				using element_type = std::expected<std::wstring_view, Error>;
				using pointer = element_type*;
				using reference = element_type&;

				const element_type& operator*(this const iterator& self)
				{
					if (!self.current)
					{
						throw "at the end";
					}
					return self.current.value();
				}

				iterator& operator++(this iterator& self)
				{
					if (!self.current)
					{
						throw "at the end";
					}

					if (auto v = self.split->next()) {
						self.current = std::move(v);
					}
					else {
						self.current = std::nullopt;
					}

					++self.index;

					return self;
				}

				bool operator==(const iterator& iter) noexcept
				{
					if (!iter.current && !current) return true;
					return iter.split == split && iter.index == index;
				}

				bool operator!=(const iterator& iter) noexcept
				{
					if (!iter.current && !current) {
						return false;
					}
					return iter.split != split || iter.index != index;
				}

				iterator(Split* split,
					std::optional<std::expected<std::wstring_view, Error>> start)
					noexcept : split(split), current(start), index(0)
				{

				}

				iterator(Split* split) noexcept : split(split), index(-1) {}

				~iterator() = default;

			private:
				Split* split;
				std::optional<std::expected<std::wstring_view, Error>> current;
				int index;
			};

			auto begin() { return iterator(this, next()); };

			auto end() { return iterator(this); };

			auto next(this Split& self) -> std::optional<std::expected<std::wstring_view, Error>> {
				auto text = self.finder.subject;
				auto v = self.finder.next();
				if (!v)
				{
					if (self.last > text.size()) {
						return std::nullopt;
					}
					else {
						auto s = text.substr(self.last);
						self.last = text.size() + 1; // Next call will return None
						return std::make_optional(s);
					}
				}
				else {
					if (*v) {
						auto matched = text.substr(self.last, (*v)->start - self.last);
						self.last = (*v)->end;
						return std::make_optional(matched);
					}
					else {
						return std::unexpected(v->error());
					}
				}
			}
		};

		inline auto split(this const wregex& self, std::wstring_view haystack) noexcept -> Split
		{
			return Split{ .finder = self.find_iter(haystack), .last = 0 };
		}

		struct SplitN {
			Split splits;
			size_t limit;

			struct iterator
			{
				using difference_type = std::ptrdiff_t;
				using element_type = std::expected<std::wstring_view, Error>;
				using pointer = element_type*;
				using reference = element_type&;

				const element_type& operator*(this const iterator& self)
				{
					if (!self.current)
					{
						throw "at the end";
					}
					return self.current.value();
				}

				iterator& operator++(this iterator& self)
				{
					if (!self.current)
					{
						throw "at the end";
					}

					if (auto v = self.split->next()) {
						self.current = std::move(v);
					}
					else {
						self.current = std::nullopt;
					}

					++self.index;

					return self;
				}

				bool operator==(const iterator& iter) noexcept
				{
					if (!iter.current && !current) return true;
					return iter.split == split && iter.index == index;
				}

				bool operator!=(const iterator& iter) noexcept
				{
					if (!iter.current && !current) {
						return false;
					}
					return iter.split != split || iter.index != index;
				}

				iterator(SplitN* split,
					std::optional<std::expected<std::wstring_view, Error>> start)
					noexcept : split(split), current(start), index(0)
				{

				}

				iterator(SplitN* split) noexcept : split(split), index(-1) {}

				~iterator() = default;

			private:
				SplitN* split;
				std::optional<std::expected<std::wstring_view, Error>> current;
				int index;
			};

			auto begin() noexcept { return iterator(this, next()); };

			auto end() noexcept { return iterator(this); };

			auto next(this SplitN& self) -> std::optional<std::expected<std::wstring_view, Error>>
			{
				if (self.limit == 0) {
					return std::nullopt;
				}

				self.limit -= 1;
				if (self.limit > 0) {
					return self.splits.next();
				}

				auto len = self.splits.finder.subject.size();
				if (self.splits.last > len) {
					// We've already returned all substrings.
					return std::nullopt;
				}
				else {
					auto text = self.splits.finder.subject;
					auto matched = text.substr(self.splits.last);
					return std::make_optional(matched);
				}
			}

			auto size_hint(this const SplitN& self) -> std::tuple<size_t, std::optional<size_t>>
			{
				return std::make_tuple(0, self.limit);
			}
		};

		inline auto splitn(
			this const wregex& self,
			std::wstring_view haystack,
			size_t limit
		) noexcept -> SplitN
		{
			return SplitN{ Split{.finder = self.find_iter(haystack), .last = 0 }, limit };
		}
	};
}
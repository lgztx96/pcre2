#pragma once
#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 0
#include "capture_locations.h"
#include "captures.h"
#include "code.h"
#include "config.h"
#include "match_data.h"
#include <map>
#include <pcre2.h>

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

class wregex {
	friend struct RegexBuilder;
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
	MatchData match_data;

public:
	wregex() {}

	wregex(wregex&& regex) noexcept {
		config = regex.config;
		pattern = regex.pattern;
		code = std::move(regex.code);
		capture_names = std::move(regex.capture_names);
		capture_names_idx = std::move(regex.capture_names_idx);
		match_data = MatchData::get(regex.config.match_config, *code);
	}

	auto find_at_with_match_data(
		this const wregex& self,
		MatchData& match_data,
		//match_data: &mut MatchDataPoolGuard < '_>,
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
		if (!*match_data.find(self.code.get(), subject, start, options)) {
			return std::nullopt;
		}
		auto ovector = match_data.ovector();
		return Match(subject, ovector[0], ovector[1]);
	}

	/// Returns the same as `captures_read`, but starts the search at the given
	/// offset and populates the capture locations given.
	///
	/// The significance of the starting point is that it takes the surrounding
	/// context into consideration. For example, the `\A` anchor can only
	/// match when `start == 0`.
	auto captures_read_at(
		this const wregex& self,
		CaptureLocations locs,
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
		if (!*locs.data.find(self.code.get(), subject, start, options)) {
			return std::nullopt;
		}
		auto ovector = locs.data.ovector();

		return Match(subject, ovector[0], ovector[1]);
	}

	//static std::expected<wregex, Error> compile(std::wstring_view pattern) {
	//     auto builder = RegexBuilder();
	//     return builder.build(pattern);
	// }

	inline std::wstring_view as_str(this const wregex& self)
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
		return CaptureLocations{ .code = self.code.get(), .data = self.new_match_data() };
	}

	inline auto new_match_data(this const wregex& self) -> MatchData
	{
		return MatchData::get(self.config.match_config, *self.code.get());
	}

	// 构造函数，接收正则表达式模式
	//explicit wregex(const std::wstring& pattern) : pattern(pattern) {
	//    // 编译正则表达式
	//    size_t erroroffset;
	//    int errorcode = 0;
	//    re = pcre2_compile_16(
	//        reinterpret_cast<const PCRE2_UCHAR16*>(pattern.c_str()),  // 正则表达式模式
	//        pattern.size(),  // 模式的字节长度
	//        0,  // 标志
	//        &errorcode,  // 错误码
	//        &erroroffset,  // 错误位置
	//        nullptr);  // 编译时的额外选项
	//    auto k = pcre2_jit_compile_16(re, PCRE2_JIT_COMPLETE);

	//    if (!re || k) {
	//        std::wstring error_msg = L"Regex compilation failed at position " + std::to_wstring(erroroffset);
	//        throw std::runtime_error(std::string(error_msg.begin(), error_msg.end()));
	//    }
	//}

	// 析构函数，释放正则表达式
	~wregex() {
		//if (re) {
		   // pcre2_code_free_16(re);
		//}
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
		auto match_data = self.match_data;
		// SAFETY: We don't use any dangerous PCRE2 options.
		auto res =
			match_data.find(self.code.get(), subject, start, options);
		// PoolGuard::put(match_data);
		return res;
	}
	
	struct Matches {
		const wregex& re;
		MatchData match_data;
		//MatchData match_data : MatchDataPoolGuard < 'r>,
		std::wstring_view subject;
		size_t last_end;
		std::optional<size_t> last_match;

		struct matches_iterator
		{
			using difference_type = std::ptrdiff_t;
			using element_type = std::expected<Match, Error>;
			using pointer = element_type*;
			using reference = const element_type&;

			reference& operator*(this const matches_iterator& self)
			{
				if (!self.current)
				{
					throw "at the end";
				}
				return self.current.value();
			}

			matches_iterator& operator++()
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

			bool operator==(const matches_iterator& iter)
			{ 
				if (!iter.current && !current) return true;
				return iter.matches == matches && iter.index == index;
			}
			
			bool operator!=(const matches_iterator& iter) 
			{
				if (!iter.current && !current) {
					return false;
				}
				return iter.matches != matches || iter.index != index;
			}

			matches_iterator(Matches* matches,
				std::optional<std::expected<Match, Error>>&& start) noexcept : matches(matches), index(0)
			{
				if (const auto& v = start)
				{
					current = *v;
				}
			}

			matches_iterator(Matches* matches) noexcept : matches(matches), index(-1){}

			matches_iterator(const matches_iterator& iter) noexcept : matches(iter.matches), index(iter.index)
			{
				if (const auto& v = iter.current)
				{
					current = *v;
				}
			}

			~matches_iterator() = default;
			

		private:
			Matches* matches;
			std::optional<std::expected<Match, Error>> current;
			int index;
		};

		static_assert(std::input_iterator<matches_iterator>);

		auto begin() { return matches_iterator(this, next()); };

		auto end() { return matches_iterator(this); };

		auto next(this Matches& self) -> std::optional<std::expected<Match, Error>> {
			if (self.last_end > self.subject.size()) {
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

	struct CaptureMatches {

		const wregex& re;
		std::wstring_view subject;
		size_t last_end;
		std::optional<size_t> last_match;

		struct capture_matches_iterator
		{
			using difference_type = std::ptrdiff_t;
			using element_type = std::expected<Captures, Error>;
			using pointer = element_type*;
			using reference = const element_type&;

			reference& operator*(this const capture_matches_iterator& self)
			{
				if (!self.current)
				{
					throw "at the end";
				}
				return self.current.value();
			}

			capture_matches_iterator& operator++()
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

			bool operator==(const capture_matches_iterator& iter)
			{
				if (!iter.current && !current) return true;
				return iter.matches == matches && iter.index == index;
			}

			bool operator!=(const capture_matches_iterator& iter)
			{
				if (!iter.current && !current) {
					return false;
				}
				return iter.matches != matches || iter.index != index;
			}

			capture_matches_iterator(CaptureMatches* matches,
				std::optional<std::expected<Captures, Error>>&& start) noexcept : matches(matches), index(0)
			{
				if (const auto& v = start)
				{
					current = *v;
				}
			}

			capture_matches_iterator(CaptureMatches* matches) noexcept : matches(matches), index(-1) {}

			capture_matches_iterator(const capture_matches_iterator& iter) noexcept : matches(iter.matches), index(iter.index)
			{
				if (const auto& v = iter.current)
				{
					current = *v;
				}
			}

			~capture_matches_iterator() = default;


		private:
			CaptureMatches* matches;
			std::optional<std::expected<Captures, Error>> current;
			int index;
		};

		auto begin() { return capture_matches_iterator(this, next()); };

		auto end() { return capture_matches_iterator(this); };

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
				.subject = self.subject,
				.locs = locs,
				.idx = self.re.capture_names_idx.get()
			};
		}
	};

	//inline std::optional<Match> find(std::wstring_view subject) const { return find_at(subject, 0); }

	//using match_data_ptr = std::unique_ptr<pcre2_match_data_16, decltype(&pcre2_match_data_free_16)>;

	//// 查找第一个匹配项
	//std::optional<Match> find_at(std::wstring_view subject, size_t offset) const {
	//    PCRE2_SPTR16 subject_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(subject.data());

	//    auto match_data = match_data_ptr(pcre2_match_data_create_from_pattern_16(code->as_ptr(), nullptr), &pcre2_match_data_free_16);

	//    int result = pcre2_jit_match_16(
	//        code->as_ptr(),
	//        subject_ptr,      
	//        subject.size(),
	//        offset,  
	//        0,            
	//        match_data.get(), 
	//        nullptr);

	//    if (result >= 0) {
	//        uint32_t size = pcre2_get_ovector_count_16(match_data.get());
	//        size_t* ovector = pcre2_get_ovector_pointer_16(match_data.get());
	//       // for (uint32_t i = 0; i < size; i++)
	//        {
	//            // result->data.write[i].start = ovector[i * 2];
	//            // result->data.write[i].end = ovector[i * 2 + 1];
	//        }
	//        
	//        return Match{ .start = ovector[0], .end = ovector[1] };
	//    }

	//    return std::nullopt;
	//}

	auto find_at(this const wregex& self,
		std::wstring_view subject,
		size_t start
	) -> std::expected<std::optional<Match>, Error> {
		auto match_data = self.match_data;
		auto res =
			self.find_at_with_match_data(match_data, subject, start);
		//PoolGuard::put(match_data);
		return res;
	}

	inline auto captures_read(
		this const wregex& self,
		CaptureLocations& locs,
		std::wstring_view subject
	) -> std::expected<std::optional<Match>, Error> {
		return self.captures_read_at(locs, subject, 0);
	}

	inline auto is_match(this const wregex& self, std::wstring_view subject) -> std::expected<bool, Error> {
		return self.is_match_at(subject, 0);
	}

	inline auto find(
		this const wregex& self,
		std::wstring_view subject
	) -> std::expected<std::optional<Match>, Error> {
		return self.find_at(subject, 0);
	}

	inline auto find_iter(this const wregex& self, std::wstring_view subject) -> Matches {
		return Matches{
			 .re = self,
			 .match_data = self.match_data,
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
				return Captures{
					.subject = subject,
					.locs = locs,
					.idx = self.capture_names_idx.get(),
				};
			});
	}

	auto captures_iter(
		this const wregex& self,
		std::wstring_view subject
	) -> CaptureMatches {
		return CaptureMatches{ .re = self, .subject = subject, .last_end = 0, .last_match = std::nullopt };
	}

	// 替换第一个匹配项
	//bool replace(std::wstring_view subject, std::wstring_view replacement, std::wstring& output) const {
	//    PCRE2_SPTR16 subject_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(subject.data());
	//    PCRE2_SPTR16 replacement_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(replacement.data());

	//    output.resize(subject.size() + 1);
	//    size_t outlen = output.size();

	//    pcre2_match_data_16* match_data = pcre2_match_data_create_from_pattern_16(re, nullptr);
	//    int rc = pcre2_substitute_16(
	//        re,                      // 编译后的正则表达式
	//        subject_ptr,             // 要替换的文本
	//        subject.size(), // 文本长度
	//        0,                       // 匹配起始位置
	//        0,     
	//        match_data, // 标志
	//        nullptr,
	//        replacement_ptr,         // 替换字符串
	//        replacement.size(),  // 替换字符串长度
	//        reinterpret_cast<PCRE2_UCHAR16*>(output.data()),                 // 额外选项
	//        &outlen);                // 结果输出

	//    while (rc == PCRE2_ERROR_NOMEMORY) {
	//       output.resize(output.size() + subject.size());
	//       // o = (PCRE2_UCHAR16*)output.ptrw();
	//        rc = pcre2_substitute_16(re,                      // 编译后的正则表达式
	//            subject_ptr,             // 要替换的文本
	//            subject.size(), // 文本长度
	//            0,                       // 匹配起始位置
	//            0,
	//            match_data, // 标志
	//            nullptr,
	//            replacement_ptr,         // 替换字符串
	//            replacement.size(),  // 替换字符串长度
	//            reinterpret_cast<PCRE2_UCHAR16*>(output.data()),                 // 额外选项
	//            &outlen);
	//    }
	//    output.resize(outlen);
	//    // 将结果放入返回的字符串
	//   // output.assign(reinterpret_cast<const wchar_t*>(output), outlen);

	//    pcre2_match_data_free_16(match_data);
	//    return true;
	//}
};
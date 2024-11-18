#pragma once
#include <optional>
#include <string_view>

enum class JITChoice 
{
	/// Never do JIT compilation.
	Never,
	/// Always do JIT compilation and return an error if it fails.
	Always,
	/// Attempt to do JIT compilation but silently fall back to non-JIT.
	Attempt,
};

struct MatchConfig
{
	/// When set, a custom JIT stack will be created with the given maximum
	/// size.
	std::optional<size_t> max_jit_stack_size;
};

struct Config {
	/// PCRE2_CASELESS
	bool caseless;
	/// PCRE2_DOTALL
	bool dotall;
	/// PCRE2_EXTENDED
	bool extended;
	/// PCRE2_MULTILINE
	bool multi_line;
	/// PCRE2_NEWLINE_ANYCRLF
	bool crlf;
	/// PCRE2_UCP
	bool ucp;
	/// PCRE2_UTF
	bool utf;
	/// use pcre2_jit_compile
	JITChoice jit;
	/// Match-time specific configuration knobs.
	MatchConfig match_config;

	Config() noexcept
		: caseless(false)
		, dotall(false)
		, extended(false)
		, multi_line(false)
		, crlf(false)
		, ucp(false)
		, utf(false)
		, jit(JITChoice::Never) {

	}
};

struct Match
{
	const wchar_t* subject;
	//std::wstring_view subject;
	size_t start;
	size_t end;

	auto as_view(this const Match& self) noexcept -> std::wstring_view
	{
		return std::wstring_view(self.subject + self.start, self.end - self.start);
	}
	
	auto suffix(this const Match& self) noexcept -> std::wstring_view
	{
		return std::wstring_view(self.subject, self.start);
	}

	auto prefix(this const Match& self) noexcept -> std::wstring_view
	{
		return std::wstring_view(self.subject + self.end);
	}
};
//namespace meta {
//
//
//struct Split {
//	std::wstring_view haystack;
//	meta::Split it;
//
//inline auto next(this Split& self)-> std::optional<std::wstring_view> {
//	return self.it.next().map(| span | &self.haystack[span]);
//}
//};
//
//struct SplitN {
//	std::wstring_view haystack;
//	meta::SplitN it;
//
//	inline auto next(this SplitN& self) -> std::optional<std::wstring_view> {
//		return self.it.next().map(| span | &self.haystack[span]);
//	}
//
//inline auto size_hint(this SplitN& self) -> std::tuple<size_t, std::optional<size_t>> {
//	return self.it.size_hint();
//	}
//};
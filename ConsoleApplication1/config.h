#pragma once
#include <optional>
#include <string_view>

enum class JITChoice {
	/// Never do JIT compilation.
	Never,
	/// Always do JIT compilation and return an error if it fails.
	Always,
	/// Attempt to do JIT compilation but silently fall back to non-JIT.
	Attempt,
};

struct MatchConfig {
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

	Config()
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
	std::wstring_view subject;
	size_t start;
	size_t end;

	auto as_bytes(this const Match& self) -> std::wstring_view
	{
		return self.subject.substr(self.start, self.end - self.start);
	}
};
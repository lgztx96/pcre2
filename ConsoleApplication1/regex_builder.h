#pragma once

#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 0
#include "code.h"
#include "compile_context.h"
#include "config.h"
#include "error.h"
#include "match_data.h"
#include "regex.h"
#include <expected>
#include <map>
#include <pcre2.h>

struct RegexBuilder
{
private:
	Config config;
public:
	/// Compile the given pattern into a PCRE regex using the current
	/// configuration.
	///
	/// If there was a problem compiling the pattern, then an error is
	/// returned.
	std::expected<wregex, Error> build(this RegexBuilder& self, std::wstring_view pattern) {
		uint32_t options = 0;
		if (self.config.caseless) {
			options |= PCRE2_CASELESS;
		}
		if (self.config.dotall) {
			options |= PCRE2_DOTALL;
		}
		if (self.config.extended) {
			options |= PCRE2_EXTENDED;
		}
		if (self.config.multi_line) {
			options |= PCRE2_MULTILINE;
		}
		if (self.config.ucp) {
			options |= PCRE2_UCP;
			options |= PCRE2_UTF;
			options |= PCRE2_MATCH_INVALID_UTF;
		}
		if (self.config.utf) {
			options |= PCRE2_UTF;
		}

		auto ctx = std::make_unique<CompileContext>();
		if (self.config.crlf) {
			ctx->set_newline(PCRE2_NEWLINE_ANYCRLF);
			//.expect("PCRE2_NEWLINE_ANYCRLF is a legal value");
		}

		auto code = *Code::make(pattern, options, std::move(ctx));
		switch (self.config.jit)
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
		for (size_t i = 0; i < capture_names.size(); i++) {
			if (auto name = capture_names[i]; !name.empty()) {
				idx->emplace(name, i);
			}
		}

		/* auto match_data = {
			 auto config = self.config.match_config;*/

			 //auto create : MatchDataPoolFn =
			 //        Box::new(move || MatchData::new(config.clone(), &code));
			 //    Pool::new(create)
			 //};

		wregex re;
		re.config = self.config;
		re.pattern = pattern;
		re.code = std::move(code);
		re.capture_names = std::make_unique<std::vector<std::wstring>>(std::move(capture_names));
		re.capture_names_idx = std::move(idx);
		re.match_data = MatchData::get(re.config.match_config, *re.code);
		return re;
	}

	/// Enables case insensitive matching.
	///
	/// If the `utf` option is also set, then Unicode case folding is used
	/// to determine case insensitivity. When the `utf` option is not set,
	/// then only standard ASCII case insensitivity is considered.
	///
	/// This option corresponds to the `i` flag.
	RegexBuilder& caseless(this auto& self, bool yes)
	{
		self.config.caseless = yes;
		return self;
	}

	/// Enables "dot all" matching.
	///
	/// When enabled, the `.` metacharacter in the pattern matches any
	/// character, include `\n`. When disabled (the default), `.` will match
	/// any character except for `\n`.
	///
	/// This option corresponds to the `s` flag.
	RegexBuilder& dotall(this auto& self, bool yes)
	{
		self.config.dotall = yes;
		return self;
	}

	/// Enable "extended" mode in the pattern, where whitespace is ignored.
	///
	/// This option corresponds to the `x` flag.
	RegexBuilder& extended(this auto& self, bool yes)
	{
		self.config.extended = yes;
		return self;
	}

	/// Enable multiline matching mode.
	///
	/// When enabled, the `^` and `$` anchors will match both at the beginning
	/// and end of a subject string, in addition to matching at the start of
	/// a line and the end of a line. When disabled, the `^` and `$` anchors
	/// will only match at the beginning and end of a subject string.
	///
	/// This option corresponds to the `m` flag.
	RegexBuilder& multi_line(this auto& self, bool yes)
	{
		self.config.multi_line = yes;
		return self;
	}

	/// Enable matching of CRLF as a line terminator.
	///
	/// When enabled, anchors such as `^` and `$` will match any of the
	/// following as a line terminator: `\r`, `\n` or `\r\n`.
	///
	/// This is disabled by default, in which case, only `\n` is recognized as
	/// a line terminator.
	RegexBuilder& crlf(this auto& self, bool yes)
	{
		self.config.crlf = yes;
		return self;
	}

	/// Enable Unicode matching mode.
	///
	/// When enabled, the following patterns become Unicode aware: `\b`, `\B`,
	/// `\d`, `\D`, `\s`, `\S`, `\w`, `\W`.
	///
	/// When set, this implies UTF matching mode. It is not possible to enable
	/// Unicode matching mode without enabling UTF matching mode.
	///
	/// This is disabled by default.
	RegexBuilder& ucp(this auto& self, bool yes)
	{
		self.config.ucp = yes;
		return self;
	}

	/// Enable UTF matching mode.
	///
	/// When enabled, characters are treated as sequences of code units that
	/// make up a single codepoint instead of as single bytes. For example,
	/// this will cause `.` to match any single UTF-8 encoded codepoint, where
	/// as when this is disabled, `.` will any single byte (except for `\n` in
	/// both cases, unless "dot all" mode is enabled).
	///
	/// This is disabled by default.
	RegexBuilder& utf(this auto& self, bool yes)
	{
		self.config.utf = yes;
		return self;
	}

	/// Enable PCRE2's JIT and return an error if it's not available.
	///
	/// This generally speeds up matching quite a bit. The downside is that it
	/// can increase the time it takes to compile a pattern.
	///
	/// If the JIT isn't available or if JIT compilation returns an error, then
	/// regex compilation will fail with the corresponding error.
	///
	/// This is disabled by default, and always overrides `jit_if_available`.
	RegexBuilder& jit(this auto& self, bool yes)
	{
		if (yes) {
			self.config.jit = JITChoice::Always;
		}
		else {
			self.config.jit = JITChoice::Never;
		}
		return self;
	}

	/// Enable PCRE2's JIT if it's available.
	///
	/// This generally speeds up matching quite a bit. The downside is that it
	/// can increase the time it takes to compile a pattern.
	///
	/// If the JIT isn't available or if JIT compilation returns an error,
	/// then a debug message with the error will be emitted and the regex will
	/// otherwise silently fall back to non-JIT matching.
	///
	/// This is disabled by default, and always overrides `jit`.
	RegexBuilder& jit_if_available(this auto& self, bool yes)
	{
		if (yes) {
			self.config.jit = JITChoice::Attempt;
		}
		else {
			self.config.jit = JITChoice::Never;
		}
		return self;
	}

	/// Set the maximum size of PCRE2's JIT stack, in bytes. If the JIT is
	/// not enabled, then this has no effect.
	///
	/// When `None` is given, no custom JIT stack will be created, and instead,
	/// the default JIT stack is used. When the default is used, its maximum
	/// size is 32 KB.
	///
	/// When this is set, then a new JIT stack will be created with the given
	/// maximum size as its limit.
	///
	/// Increasing the stack size can be useful for larger regular expressions.
	///
	/// By default, this is set to `None`.
	RegexBuilder& max_jit_stack_size(
		this RegexBuilder& self,
		std::optional<size_t> bytes
	) {
		self.config.match_config.max_jit_stack_size = bytes;
		return self;
	}
};

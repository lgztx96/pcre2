#pragma once

#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 0
#include "code.h"
#include "compile_context.h"
#include "config.h"
#include "error.h"
#include "match_data.h"
#include <expected>
#include <map>
#include <pcre2.h>

struct RegexOptions
{
//private:
	//Config config;
public:
	Config config;
	//std::expected<wregex, Error> build(this RegexOptions& self, std::wstring_view pattern) {
	//	uint32_t options = 0;
	//	if (self.config.caseless) {
	//		options |= PCRE2_CASELESS;
	//	}
	//	if (self.config.dotall) {
	//		options |= PCRE2_DOTALL;
	//	}
	//	if (self.config.extended) {
	//		options |= PCRE2_EXTENDED;
	//	}
	//	if (self.config.multi_line) {
	//		options |= PCRE2_MULTILINE;
	//	}
	//	if (self.config.ucp) {
	//		options |= PCRE2_UCP;
	//		options |= PCRE2_UTF;
	//		options |= PCRE2_MATCH_INVALID_UTF;
	//	}
	//	if (self.config.utf) {
	//		options |= PCRE2_UTF;
	//	}

	//	auto ctx = std::make_unique<CompileContext>();
	//	if (self.config.crlf) {
	//		ctx->set_newline(PCRE2_NEWLINE_ANYCRLF);
	//		//.expect("PCRE2_NEWLINE_ANYCRLF is a legal value");
	//	}

	//	auto code = *Code::make(pattern, options, std::move(ctx));
	//	switch (self.config.jit)
	//	{
	//	case JITChoice::Never:
	//		break;
	//	case JITChoice::Always:
	//		code->jit_compile();
	//		break;
	//	case JITChoice::Attempt:
	//		if (auto rc = code->jit_compile(); !rc) {
	//			//log::debug!("JIT compilation failed: {}", err);
	//		}
	//		break;
	//	}

	//	auto capture_names = code->capture_names();
	//	auto idx = std::make_unique<std::map<std::wstring, size_t, std::less<void>>>();
	//	for (size_t i = 0; i < capture_names.size(); i++) {
	//		if (auto name = capture_names[i]; !name.empty()) {
	//			idx->emplace(name, i);
	//		}
	//	}

	//	/* auto match_data = {
	//		 auto config = self.config.match_config;*/

	//		 //auto create : MatchDataPoolFn =
	//		 //        Box::new(move || MatchData::new(config.clone(), &code));
	//		 //    Pool::new(create)
	//		 //};

	//	wregex re;
	//	/*re.config = self.config;
	//	re.pattern = pattern;
	//	re.code = std::move(code);
	//	re.capture_names = std::make_unique<std::vector<std::wstring>>(std::move(capture_names));
	//	re.capture_names_idx = std::move(idx);
	//	re.match_data = MatchData::create(re.config.match_config, re.code.get());*/
	//	return re;
	//}

	RegexOptions& caseless(this auto& self, bool yes)
	{
		self.config.caseless = yes;
		return self;
	}

	RegexOptions& dotall(this auto& self, bool yes)
	{
		self.config.dotall = yes;
		return self;
	}

	RegexOptions& extended(this auto& self, bool yes)
	{
		self.config.extended = yes;
		return self;
	}

	RegexOptions& multi_line(this auto& self, bool yes)
	{
		self.config.multi_line = yes;
		return self;
	}

	RegexOptions& crlf(this auto& self, bool yes)
	{
		self.config.crlf = yes;
		return self;
	}

	RegexOptions& ucp(this auto& self, bool yes)
	{
		self.config.ucp = yes;
		return self;
	}

	RegexOptions& utf(this auto& self, bool yes)
	{
		self.config.utf = yes;
		return self;
	}

	RegexOptions& jit(this auto& self, bool yes)
	{
		if (yes) {
			self.config.jit = JITChoice::Always;
		}
		else {
			self.config.jit = JITChoice::Never;
		}
		return self;
	}

	RegexOptions& jit_if_available(this auto& self, bool yes)
	{
		if (yes) {
			self.config.jit = JITChoice::Attempt;
		}
		else {
			self.config.jit = JITChoice::Never;
		}
		return self;
	}

	RegexOptions& max_jit_stack_size(
		this RegexOptions& self,
		std::optional<size_t> bytes
	) {
		self.config.match_config.max_jit_stack_size = bytes;
		return self;
	}
};

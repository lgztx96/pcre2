#pragma once

#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>
#include "code.h"
#include "compile_context.h"
#include "config.h"
#include "error.h"
#include "match_data.h"
import <expected>;
import <map>;

struct RegexOptions
{
public:
	Config config;

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

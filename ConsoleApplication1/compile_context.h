#pragma once
#include <cassert>
#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>
#include <expected>
#include "error.h"

namespace pcre2 {

	struct CompileContext
	{
		pcre2_compile_context_16* context;

		CompileContext()
		{
			auto ctx = pcre2_compile_context_create_16(nullptr);
			assert(ctx, "could not allocate compile context");
			context = ctx;
		}

		CompileContext(const CompileContext& rhs) = delete;

		~CompileContext()
		{
			pcre2_compile_context_free_16(context);
		}

		explicit operator pcre2_compile_context_16* (this const CompileContext& self) noexcept
		{
			return self.context;
		}

		inline auto as_mut_ptr(this const CompileContext& self) noexcept -> pcre2_compile_context_16*
		{
			return self.context;
		}

		auto set_newline(this const CompileContext& self, uint32_t value) -> std::expected<void, Error>
		{
			auto rc = pcre2_set_newline_16(self.context, value);
			if (rc == 0) {
				return {};
			}
			else {
				return std::unexpected(Error::option(rc));
			}
		}
	};
}
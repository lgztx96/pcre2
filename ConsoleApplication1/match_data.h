#pragma once

#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 0
#include "config.h"
#include "error.h"
#include <expected>
#include <optional>
#include <pcre2.h>
#include <span>

namespace pcre2 {

	struct MatchData
	{
		MatchConfig config;
		pcre2_match_context_16* match_context;
		pcre2_match_data_16* match_data;
		std::optional<pcre2_jit_stack_16*> jit_stack;
		const size_t* ovector_ptr;
		uint32_t ovector_count;

		MatchData(const MatchData&) = delete;
		MatchData operator=(const MatchData&) = delete;

		MatchData(MatchConfig config, const Code* code) : config(config)
		{
			match_context = pcre2_match_context_create_16(nullptr);
			assert(match_context, "failed to allocate match context");

			match_data = pcre2_match_data_create_from_pattern_16(
				code->as_ptr(),
				nullptr);
			assert(match_data, "failed to allocate match data block");

			jit_stack = [&]() -> std::optional<pcre2_jit_stack_16*> {
				if (!code->compiled_jit) {
					return std::nullopt;
				}
				if (const auto& max = config.max_jit_stack_size) {
					auto stack = pcre2_jit_stack_create_16(
						std::min<size_t>(*max, static_cast<size_t>(32 * 1) << 10),
						*max,
						nullptr
					);
					assert(!stack, "failed to allocate JIT stack");

					pcre2_jit_stack_assign_16(
						match_context,
						nullptr,
						stack
					);
					return stack;
				}

				return std::nullopt;
				}();

			ovector_ptr = pcre2_get_ovector_pointer_16(match_data);
			assert(ovector_ptr, "got NULL ovector pointer");
			ovector_count = pcre2_get_ovector_count_16(match_data);
		}

		~MatchData()
		{
			if (auto& stack = jit_stack)
			{
				pcre2_jit_stack_free_16(*stack);
			}
			pcre2_match_data_free_16(match_data);
			pcre2_match_context_free_16(match_context);
		}

		auto find(
			this const MatchData& self,
			const Code* code,
			std::wstring_view subject,
			size_t start,
			uint32_t options
		) -> std::expected<bool, Error> {

			auto rc = pcre2_match_16(
				code->as_ptr(),
				std::bit_cast<PCRE2_SPTR16>(subject.data()),
				subject.size(),
				start,
				options,
				self.as_mut_ptr(),
				self.match_context
			);
			if (rc == PCRE2_ERROR_NOMATCH) {
				return false;
			}
			else if (rc > 0) {
				return true;
			}
			else {
				assert(rc != 0, "ovector should never be too small");
				return std::unexpected(Error::matching(rc));
			}
		}

		inline auto as_mut_ptr(this const MatchData& self) noexcept -> pcre2_match_data_16*
		{
			return self.match_data;
		}

		explicit operator pcre2_match_data_16* (this const MatchData& self) noexcept
		{
			return self.match_data;
		}

		inline auto ovector(this const MatchData& self) noexcept -> std::span<const size_t>
		{
			return std::span<const size_t>(self.ovector_ptr, self.ovector_count * 2);
		}
	};
}
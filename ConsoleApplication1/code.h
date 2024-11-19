#pragma once
#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 0
#include "compile_context.h"
#include "error.h"
#include <bit>
#include <expected>
#include <memory>
#include <pcre2.h>
#include <span>
#include <vector>

struct Code {
	pcre2_code_16* code;
	bool compiled_jit;
	std::unique_ptr<CompileContext> ctx;

	~Code() {
		pcre2_code_free_16(code);
	}

	static auto make(
		std::wstring_view pattern,
		uint32_t options,
		std::unique_ptr<CompileContext> ctx
	) -> std::expected<std::unique_ptr<Code>, Error> {
		int error_code = 0;
		size_t error_offset = 0;
		auto code =
			pcre2_compile_16(
				std::bit_cast<PCRE2_SPTR16>(pattern.data()),
				pattern.size(),
				options,
				&error_code,
				&error_offset,
				ctx->as_mut_ptr()
			);
		if (code == nullptr) {
			return std::unexpected(Error::compile(error_code, error_offset));
		}
		else {
			return std::make_unique<Code>(code, false, std::move(ctx));
		}
	}

	auto jit_compile(this Code& self) -> std::expected<void, Error>
	{
		auto error_code = pcre2_jit_compile_16(self.code, PCRE2_JIT_COMPLETE);
		if (error_code == 0) {
			self.compiled_jit = true;
			return {};
		}
		else {
			return std::unexpected(Error::jit(error_code));
		}
	}

	auto capture_names(this const Code& self) -> std::vector<std::wstring>
	{
		// This is an object lesson in why C sucks. All we need is a map from
		// a name to a number, but we need to go through all sorts of
		// shenanigans to get it. In order to verify this code, see
		// https://www.pcre.org/current/doc/html/pcre2api.html
		// and search for PCRE2_INFO_NAMETABLE.

		auto name_count = *self.name_count();
		auto size = *self.name_entry_size();
		std::span<const wchar_t> table(std::bit_cast<const wchar_t*>(*self.raw_name_table()), name_count * size);

		auto names = std::vector<std::wstring>();
		names.resize(*self.capture_count());
		for (size_t i = 0; i < name_count; i++) {
			auto entry = table.subspan(i * size, (i + 1) * size - i * size);
			auto name = entry.subspan(1);
			auto nulat = std::distance(name.cbegin(), std::find(name.cbegin(), name.cend(), 0));
			/* auto nulat = name
				 .iter()
				 .position(| &b | b == 0)
				 .expect("a NUL in name table entry");*/
				 // auto index = (static_cast<size_t>(entry[0]) << 8 | static_cast<size_t>(entry[1]));
			auto index = static_cast<uint32_t>(entry[0]);
			names[index] = std::wstring(std::bit_cast<const wchar_t*>(name.data()), nulat);
			//.map(Some)
			// We require our pattern to be valid UTF-8, so all capture
			// names should also be valid UTF-8.
			//.expect("valid UTF-8 for capture name");
		}

		return names;
	}

	inline auto as_ptr(this const Code& self) noexcept -> const pcre2_code_16*
	{
		return self.code;
	}

	explicit operator pcre2_code_16* (this const Code& self) noexcept
	{
		return self.code;
	}

	auto raw_name_table(this const Code& self) -> std::expected<const uint8_t*, Error>
	{
		const uint8_t* bytes = nullptr;
		auto rc = pcre2_pattern_info_16(
			self.as_ptr(),
			PCRE2_INFO_NAMETABLE,
			&bytes);
		if (rc != 0) {
			return std::unexpected(Error::info(rc));
		}
		else {
			return bytes;
		}
	}

	auto name_count(this const Code& self) -> std::expected<size_t, Error>
	{
		uint32_t count = 0;
		auto rc =
			pcre2_pattern_info_16(
				self.as_ptr(),
				PCRE2_INFO_NAMECOUNT,
				&count);

		if (rc != 0) {
			return std::unexpected(Error::info(rc));
		}
		else {
			return count;
		}
	}

	auto name_entry_size(this const Code& self) -> std::expected<size_t, Error>
	{
		uint32_t size = 0;
		auto rc =
			pcre2_pattern_info_16(
				self.as_ptr(),
				PCRE2_INFO_NAMEENTRYSIZE,
				&size);

		if (rc != 0) {
			return std::unexpected(Error::info(rc));
		}
		else {
			return size;
		}
	}

	auto capture_count(this const Code& self) -> std::expected<size_t, Error>
	{
		uint32_t count = 0;
		auto rc =
			pcre2_pattern_info_16(
				self.as_ptr(),
				PCRE2_INFO_CAPTURECOUNT,
				&count
			);

		if (rc != 0) {
			return std::unexpected(Error::info(rc));
		}
		else {
			return 1 + static_cast<size_t>(count);
		}
	}
};

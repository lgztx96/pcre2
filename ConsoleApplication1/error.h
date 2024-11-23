#pragma once
#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 0
#include <array>
#include <cassert>
#include <optional>
#include <pcre2.h>
#include <string>

namespace pcre2 {

	enum class ErrorKind
	{
		/// An error occurred during compilation of a regex.
		Compile,
		/// An error occurred during JIT compilation of a regex.
		JIT,
		/// An error occurred while matching.
		Match,
		/// An error occurred while querying a compiled regex for info.
		Info,
		/// An error occurred while setting an option.
		Option,
	};

	struct Error
	{
		ErrorKind kind;
		int code;
		std::optional<size_t> offset;

		/// Create a new compilation error.
		static auto compile(int code, size_t offset) -> Error
		{
			return Error{ ErrorKind::Compile, code, offset };
		}

		/// Create a new JIT compilation error.
		static auto jit(int code) -> Error
		{
			return Error{ ErrorKind::JIT, code, std::nullopt };
		}

		/// Create a new matching error.
		static auto matching(int code) -> Error
		{
			return Error{ ErrorKind::Match, code, std::nullopt };
		}

		/// Create a new info error.
		static auto info(int code) -> Error
		{
			return Error{ ErrorKind::Info, code, std::nullopt };
		}

		/// Create a new option error.
		static auto option(int code) -> Error
		{
			return Error{ ErrorKind::Option, code, std::nullopt };
		}

		/// Returns the error message from PCRE2.
		auto error_message(this const Error& self) -> std::wstring
		{
			// PCRE2 docs say a buffer size of 120 bytes is enough, but we're
			// cautious and double it.
			std::array<uint16_t, 240> buf{};
			auto rc = pcre2_get_error_message_16(self.code, buf.data(), buf.size());
			// Errors are only ever constructed from codes reported by PCRE2, so
			// our code should always be valid.
			assert(rc != PCRE2_ERROR_BADDATA, "used an invalid error code");
			// PCRE2 docs claim 120 bytes is enough, and we use more, so...
			assert(rc != PCRE2_ERROR_NOMEMORY, "buffer size too small");
			// Sanity check that we do indeed have a non-negative result. 0 is OK.
			assert(rc >= 0, "expected non-negative but got {}", rc);
			return { reinterpret_cast<const wchar_t*>(buf.data()), static_cast<size_t>(rc) };
		}

		inline constexpr const wchar_t* description()
		{
			return L"pcre2 error";
		}
	};
}
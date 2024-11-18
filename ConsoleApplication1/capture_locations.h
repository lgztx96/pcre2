#pragma once
#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 0
#include "code.h"
#include "match_data.h"
#include <optional>
#include <pcre2.h>

struct CaptureLocations 
{
	Code* code;
	std::unique_ptr<MatchData> data;

	CaptureLocations(Code* code, std::unique_ptr<MatchData> data) noexcept : code(code), data(std::move(data)) {}

	CaptureLocations(const CaptureLocations& rhs) = delete;

	CaptureLocations(CaptureLocations&& rhs) noexcept : code(rhs.code), data(std::move(rhs.data)) {}

	CaptureLocations operator=(CaptureLocations&& rhs) noexcept
	{
		return CaptureLocations(std::move(rhs));
	}

	auto get(this const CaptureLocations& self, size_t i) noexcept -> std::optional<std::tuple<size_t, size_t>>
	{
		auto ovec = self.data->ovector();
		size_t index = i * 2;
		if (index < ovec.size()) 
		{
			if (auto s = ovec[index]; s != PCRE2_UNSET)
			{
				index = i * 2 + 1;
				if (index < ovec.size()) 
				{
					if (auto e = ovec[index]; e != PCRE2_UNSET)
					{
						return std::make_optional<std::tuple<size_t, size_t>>(s, e);
					}
				}
			}
		}

		return std::nullopt;
	}

	inline auto len(this const CaptureLocations& self) -> size_t
	{
		return self.data->ovector().size() / 2;
	}
};

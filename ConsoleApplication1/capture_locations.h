#pragma once
#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 0
#include "code.h"
#include "match_data.h"
#include <optional>
#include <pcre2.h>

struct CaptureLocations {
	Code* code;
	MatchData data;

	CaptureLocations clone(this const CaptureLocations& self) {
		return CaptureLocations{
			 .code = self.code,
			 .data = MatchData::get(self.data.config, *self.code)
		};
	}

	std::optional<std::tuple<size_t, size_t>> get(this const auto& self, size_t i)
	{
		auto ovec = self.data.ovector();
		size_t index = i * 2;
		if (index < ovec.size()) {
			if (auto s = ovec[index]; s != PCRE2_UNSET) {
				index = i * 2 + 1;
				if (index < ovec.size()) {
					if (auto e = ovec[index]; e != PCRE2_UNSET) {
						return std::make_tuple(s, e);
					}
				}
			}
		}

		return std::nullopt;
	}

	size_t len(this const CaptureLocations& self)
	{
		return self.data.ovector().size() / 2;
	}
};

﻿#pragma once
#include "capture_locations.h"
#include "config.h"
#include <map>
#include <string>
#include <string_view>

struct Captures
{
	const wchar_t* subject;
	CaptureLocations locs;
	const std::map<std::wstring, size_t, std::less<void>>* idx;

	Captures(
		const wchar_t* subject, 
		CaptureLocations locs, 
		const std::map<std::wstring, size_t, std::less<void>>* idx) noexcept
		: subject(subject), locs(std::move(locs)), idx(idx) {};

	Captures(const Captures& c) = delete;
	Captures(Captures&& c) noexcept : subject(c.subject), locs(std::move(c.locs)), idx(c.idx) {};
	Captures operator=(Captures&& c) noexcept { return Captures(std::move(c)); };

	auto get(this const Captures& self, size_t i) -> std::optional<Match>
	{
		return self.locs.get(i).transform([&](auto v) { auto& [s, e] = v; return Match(self.subject, s, e); });
	}

	auto name(this const Captures& self, std::wstring_view name) -> std::optional<Match>
	{
		if (auto iter = self.idx->find(name); iter != self.idx->end()) {
			return self.get(iter->second);
		}
		return std::nullopt;
	}

	auto operator[](this const Captures& self, int i) -> std::wstring_view
	{
		return self.get(i)
			.transform([](const auto& m) { return m.as_view(); }).value();
		//.unwrap_or_else(|| panic!("no group at index '{}'", i));
	}

	auto operator[](this const Captures& self, const wchar_t* name) -> std::wstring_view
	{
		return self.name(name)
			.transform([](const auto& m) { return m.as_view(); }).value();
		//.unwrap_or_else(|| panic!("no group at index '{}'", i));
	}

	inline auto len(this const Captures& self) -> size_t
	{
		return self.locs.len();
	}
};
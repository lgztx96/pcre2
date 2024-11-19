// ConsoleApplication1.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "regex_builder.h"
#include "regex.h"
#include <print>
#include <boost/regex.hpp>

// https://github.com/Homebrodot/Godot/blob/5eccbcefabba0f6ead2294877db3ff4a92ece068/modules/regex/regex.cpp#L374
int main() 
{
	try 
	{
		const auto regex = pcre2::wregex::jit_compile(L"(\\d+)-(\\d+)-(\\d+)");
		constexpr auto text = L"2024-05-23-2025-06-27--2025-06-27---2025-06-27----2025-06-27-----2025-06-27------2025-06-27-------2025-06-27";

		boost::wregex re(L"(\\d+)-(\\d+)-(\\d+)");

		auto start = std::chrono::high_resolution_clock::now();

		for (auto i = 0; i < 100000; i++)
		{
			auto rc = regex->is_match_at(text, 0).value();
		}

		auto end = std::chrono::high_resolution_clock::now();

		auto c = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

		std::println("pcre {} ms", c);
		start = std::chrono::high_resolution_clock::now();

		for (auto i = 0; i < 100000; i++)
		{
			auto k = boost::regex_search(text, re);
		}
		end = std::chrono::high_resolution_clock::now();
		c = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

		std::println("boost {}ms", c);
		for (const auto& v : regex->splitn(text, 5)) {
			if (v) 
			{ 
				std::wstring s(v->data(), v->size());
				std::wcout << s << std::endl;
			}
		}
		return 0;
		boost::wsmatch match;

		if (auto rc = regex->is_match(text); rc && *rc) {
			std::wcout << L"Pattern matches the text!" << std::endl;
		}
		else {
			std::wcout << L"Pattern does not match the text." << std::endl;
		}

		if (auto match = regex->find(text); match.has_value() && match->has_value()) {
			// std::wcout << L"First match: " << text.substr((*match)->start, (*match)->end - (*match)->start) << std::endl;
		}
		else {
			std::wcout << L"No match found!" << std::endl;
		}

		auto m = regex->captures_iter(text);

		for (const auto& v : m) {
			if (v) {
				auto k = (*v)[L"year"];
				auto k1 = (*v)[L"month"];
				auto k2 = (*v)[L"day"];
				//std::wcout << L"First match: " << v->subject.substr(v->start, v->end - v->start) << std::endl;
				std::wcout << L"First match: " << k << std::endl;
				std::wcout << L"First match: " << k1 << std::endl;
				std::wcout << L"First match: " << k2 << std::endl;
			}
		}

	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
	}

	return 0;
}

//int containsRegionSensitive(const char* str, const char* substr, const char* locale) {
//    UErrorCode status = U_ZERO_ERROR;
//
//    // Convert input C strings to Unicode (UChar)
//    UChar* u_str = malloc(strlen(str) * sizeof(UChar) + 2);   // +2 for null-terminator
//    UChar* u_substr = malloc(strlen(substr) * sizeof(UChar) + 2);
//
//    // Convert C string to UChar (UTF-16)
//    int32_t str_len = ucnv_toUChars(NULL, u_str, strlen(str) + 1, str, strlen(str), &status);
//    if (U_FAILURE(status)) {
//        fprintf(stderr, "Error converting string to UChar\n");
//        free(u_str);
//        free(u_substr);
//        return 0;
//    }
//
//    int32_t substr_len = ucnv_toUChars(NULL, u_substr, strlen(substr) + 1, substr, strlen(substr), &status);
//    if (U_FAILURE(status)) {
//        fprintf(stderr, "Error converting substring to UChar\n");
//        free(u_str);
//        free(u_substr);
//        return 0;
//    }
//
//    // Create a Collator for the given locale
//    UCollator* coll = ucol_open(locale, &status);
//    if (U_FAILURE(status)) {
//        fprintf(stderr, "Error creating collator for locale %s\n", locale);
//        free(u_str);
//        free(u_substr);
//        return 0;
//    }
//
//    // Use u_strstr to check if the substring exists in the main string (using Collator for region-sensitive compare)
//    UChar* found = u_strstr(u_str, u_substr);
//    while (found != NULL) {
//        // Compare the region-sensitive order of the substring with the main string
//       
//        if (auto result = ucol_greaterOrEqual(coll, u_str, -1, u_substr, -1)) {
//            fprintf(stderr, "Error comparing strings with collator\n");
//            ucol_close(coll);
//            free(u_str);
//            free(u_substr);
//            return 0;
//        }
//
//        // If result is EQUAL, it means substring is found, region-sensitive
//        if (result == UCOL_EQUAL) {
//            ucol_close(coll);
//            free(u_str);
//            free(u_substr);
//            return 1;
//        }
//
//        // Move to the next possible occurrence of the substring
//        found = u_strstr(found + 1, u_substr); // continue searching from the next position
//    }
//
//    // Clean up and return 0 if substring is not found
//    ucol_close(coll);
//    free(u_str);
//    free(u_substr);
//    return 0;
//}
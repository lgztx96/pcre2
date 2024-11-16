// ConsoleApplication1.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "regex_builder.h"
#include <icu.h>

// https://github.com/Homebrodot/Godot/blob/5eccbcefabba0f6ead2294877db3ff4a92ece068/modules/regex/regex.cpp#L374
int main() {
    try {

        // 使用UTF-16编码的字符串创建Regex对象
        auto builder = RegexBuilder{};
        builder.jit_if_available(true);
        const auto& regex = builder.build(L"(?P<year>\\d+)-(?P<month>\\d+)-(?P<day>\\d+)");  // 这是一个简单的匹配任意字母的正则表达式
        std::wstring text = L"2024-05-23 2025-06-27";

        // 检查是否匹配
        if (auto rc = regex->is_match(text); rc && *rc) {
            std::wcout << L"Pattern matches the text!" << std::endl;
        }
        else {
            std::wcout << L"Pattern does not match the text." << std::endl;
        }

        // 查找第一个匹配项

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

        //while (auto v = m.next()) {
        //    if (*v) {
        //        std::wcout << L"First match: " << (*v)->subject.substr((*v)->start, (*v)->end - (*v)->start) << std::endl;
        //    }
        //}

        // 替换第一个匹配项
        std::wstring replaced;
        //if (regex.replace(text, L"Replaced", replaced)) {
        //    std::wcout << L"Text after replace: " << replaced << std::endl;
        //}
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
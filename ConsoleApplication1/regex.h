#pragma once
#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>

class wregex {
public:
    // 构造函数，接收正则表达式模式
    explicit wregex(const std::wstring& pattern) : pattern(pattern) {
        // 编译正则表达式
        size_t erroroffset;
        int errorcode = 0;
        re = pcre2_compile_16(
            reinterpret_cast<const PCRE2_UCHAR16*>(pattern.c_str()),  // 正则表达式模式
            pattern.size(),  // 模式的字节长度
            0,  // 标志
            &errorcode,  // 错误码
            &erroroffset,  // 错误位置
            nullptr);  // 编译时的额外选项
        auto k = pcre2_jit_compile_16(re, PCRE2_JIT_COMPLETE);
        if (!re || k) {
            std::wstring error_msg = L"Regex compilation failed at position " + std::to_wstring(erroroffset);
            throw std::runtime_error(std::string(error_msg.begin(), error_msg.end()));
        }
    }

    // 析构函数，释放正则表达式
    ~wregex() {
        if (re) {
            pcre2_code_free_16(re);
        }
    }

    bool is_match(std::wstring_view subject) const { return is_match_at(subject, 0); }

    // 检查给定的字符串是否匹配正则表达式
    bool is_match_at(std::wstring_view subject, size_t offset) const {
        PCRE2_SPTR16 subject_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(subject.data());
        pcre2_match_data_16* match_data = pcre2_match_data_create_from_pattern_16(re, nullptr);

        int result = pcre2_jit_match_16(
            re,               // 编译后的正则表达式
            subject_ptr,      // 要匹配的文本
            subject.size() * sizeof(wchar_t),  // 文本长度
            offset,                // 匹配起始位置
            0,                // 标志
            match_data,       // 匹配数据
            nullptr);         // 额外选项

        pcre2_match_data_free_16(match_data);
        return result >= 0;  // 返回是否匹配成功
    }

    rsize_t find(std::wstring_view subject) const { return find_at(subject, 0); }

    // 查找第一个匹配项
    size_t find_at(std::wstring_view subject, size_t offset) const {
        PCRE2_SPTR16 subject_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(subject.data());
        pcre2_match_data_16* match_data = pcre2_match_data_create_from_pattern_16(re, nullptr);

        int result = pcre2_jit_match_16(
            re,               // 编译后的正则表达式
            subject_ptr,      // 要匹配的文本
            subject.size() * sizeof(wchar_t),  // 文本长度
            offset,                // 匹配起始位置
            0,                // 标志
            match_data,       // 匹配数据
            nullptr);         // 额外选项

        if (result >= 0) {
            PCRE2_SIZE* ovector = pcre2_get_ovector_pointer_16(match_data);
            
            auto offset = ovector[0];
            pcre2_match_data_free_16(match_data);
            return offset;
        }

        uint32_t size = pcre2_get_ovector_count_16(match_data);
        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer_16(match_data);

        result->data.resize(size);

        for (uint32_t i = 0; i < size; i++) {
            result->data.write[i].start = ovector[i * 2];
            result->data.write[i].end = ovector[i * 2 + 1];
        }

        return std::string::npos;
    }

    // 替换第一个匹配项
    bool replace(std::wstring_view subject, std::wstring_view replacement, std::wstring& result) const {
        PCRE2_SPTR16 subject_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(subject.data());
        PCRE2_SPTR16 replacement_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(replacement.data());

        PCRE2_UCHAR16 output[1024];
        PCRE2_SIZE outlen = sizeof(output);

        pcre2_match_data_16* match_data = pcre2_match_data_create_from_pattern_16(re, nullptr);
        pcre2_substitute_16(
            re,                      // 编译后的正则表达式
            subject_ptr,             // 要替换的文本
            subject.size(), // 文本长度
            0,                       // 匹配起始位置
            0,     
            match_data, // 标志
            nullptr,
            replacement_ptr,         // 替换字符串
            replacement.size(),  // 替换字符串长度
            output,                 // 额外选项
                        // 匹配数据
            &outlen);                // 结果输出

        // 将结果放入返回的字符串
        result.assign(reinterpret_cast<const wchar_t*>(output), outlen);

        pcre2_match_data_free_16(match_data);
        return true;
    }

private:
    pcre2_code_16* re;  // 编译后的正则表达式
    std::wstring pattern;
};
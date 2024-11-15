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
    // ���캯��������������ʽģʽ
    explicit wregex(const std::wstring& pattern) : pattern(pattern) {
        // ����������ʽ
        size_t erroroffset;
        int errorcode = 0;
        re = pcre2_compile_16(
            reinterpret_cast<const PCRE2_UCHAR16*>(pattern.c_str()),  // ������ʽģʽ
            pattern.size(),  // ģʽ���ֽڳ���
            0,  // ��־
            &errorcode,  // ������
            &erroroffset,  // ����λ��
            nullptr);  // ����ʱ�Ķ���ѡ��
        auto k = pcre2_jit_compile_16(re, PCRE2_JIT_COMPLETE);
        if (!re || k) {
            std::wstring error_msg = L"Regex compilation failed at position " + std::to_wstring(erroroffset);
            throw std::runtime_error(std::string(error_msg.begin(), error_msg.end()));
        }
    }

    // �����������ͷ�������ʽ
    ~wregex() {
        if (re) {
            pcre2_code_free_16(re);
        }
    }

    bool is_match(std::wstring_view subject) const { return is_match_at(subject, 0); }

    // ���������ַ����Ƿ�ƥ��������ʽ
    bool is_match_at(std::wstring_view subject, size_t offset) const {
        PCRE2_SPTR16 subject_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(subject.data());
        pcre2_match_data_16* match_data = pcre2_match_data_create_from_pattern_16(re, nullptr);

        int result = pcre2_jit_match_16(
            re,               // ������������ʽ
            subject_ptr,      // Ҫƥ����ı�
            subject.size() * sizeof(wchar_t),  // �ı�����
            offset,                // ƥ����ʼλ��
            0,                // ��־
            match_data,       // ƥ������
            nullptr);         // ����ѡ��

        pcre2_match_data_free_16(match_data);
        return result >= 0;  // �����Ƿ�ƥ��ɹ�
    }

    rsize_t find(std::wstring_view subject) const { return find_at(subject, 0); }

    // ���ҵ�һ��ƥ����
    size_t find_at(std::wstring_view subject, size_t offset) const {
        PCRE2_SPTR16 subject_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(subject.data());
        pcre2_match_data_16* match_data = pcre2_match_data_create_from_pattern_16(re, nullptr);

        int result = pcre2_jit_match_16(
            re,               // ������������ʽ
            subject_ptr,      // Ҫƥ����ı�
            subject.size() * sizeof(wchar_t),  // �ı�����
            offset,                // ƥ����ʼλ��
            0,                // ��־
            match_data,       // ƥ������
            nullptr);         // ����ѡ��

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

    // �滻��һ��ƥ����
    bool replace(std::wstring_view subject, std::wstring_view replacement, std::wstring& result) const {
        PCRE2_SPTR16 subject_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(subject.data());
        PCRE2_SPTR16 replacement_ptr = reinterpret_cast<const PCRE2_UCHAR16*>(replacement.data());

        PCRE2_UCHAR16 output[1024];
        PCRE2_SIZE outlen = sizeof(output);

        pcre2_match_data_16* match_data = pcre2_match_data_create_from_pattern_16(re, nullptr);
        pcre2_substitute_16(
            re,                      // ������������ʽ
            subject_ptr,             // Ҫ�滻���ı�
            subject.size(), // �ı�����
            0,                       // ƥ����ʼλ��
            0,     
            match_data, // ��־
            nullptr,
            replacement_ptr,         // �滻�ַ���
            replacement.size(),  // �滻�ַ�������
            output,                 // ����ѡ��
                        // ƥ������
            &outlen);                // ������

        // ��������뷵�ص��ַ���
        result.assign(reinterpret_cast<const wchar_t*>(output), outlen);

        pcre2_match_data_free_16(match_data);
        return true;
    }

private:
    pcre2_code_16* re;  // ������������ʽ
    std::wstring pattern;
};
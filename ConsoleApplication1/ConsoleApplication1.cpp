// ConsoleApplication1.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "regex_builder.h"
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

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件

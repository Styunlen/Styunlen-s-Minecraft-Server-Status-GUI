#pragma once
#ifndef _H_STRINGTOOLS
#define _H_STRINGTOOLS
#include <string>
#include <locale>         // std::wstring_convert
#include <codecvt>        // std::codecvt_utf8
#include <iostream>
#include <memory>
using namespace std;
#define _CRT_SECURE_NO_WARNINGS

std::string UnicodeToUTF8(const std::wstring & wstr);
std::wstring UTF8ToUnicode(const std::string & str);
std::string UnicodeToANSI(const std::wstring & wstr);
std::wstring ANSIToUnicode(const std::string & str);

#endif // !_H_STRINGTOOLS

#pragma once
/********************
以下代码取自百度云SDK
代码略作修改
*********************/
#ifndef __BASE64_H__
#define __BASE64_H__

#include <iostream>
#include <string>

extern const std::string base64_chars;

static inline bool is_base64(const char c)
{
	return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_encode(const char * bytes_to_encode, unsigned int in_len);

std::string base64_decode(std::string const & encoded_string);
#endif
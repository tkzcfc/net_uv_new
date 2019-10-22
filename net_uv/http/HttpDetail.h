#pragma once

#include "HttpCommon.h"

NS_NET_UV_BEGIN

class HttpDetail
{
public:

	static std::string encode_url(const std::string& s);

	static bool is_hex(char c, int& v);

	static bool from_hex_to_i(const std::string& s, size_t i, size_t cnt, int& val);

	static std::string from_i_to_hex(uint64_t n);

	static size_t to_utf8(int code, char* buff);

	static std::string decode_url(const std::string& s);

	static void parse_query_text(const std::string& s, std::map<std::string, std::string>& params);
	
	static std::map<std::string, std::string> parse_query_text(const std::string& s);

	template <class Fn>
	static void split(const char* b, const char* e, char d, Fn fn)
	{
		int i = 0;
		int beg = 0;

		while (e ? (b + i != e) : (b[i] != '\0')) {
			if (b[i] == d) {
				fn(&b[beg], &b[i]);
				beg = i + 1;
			}
			i++;
		}

		if (i) {
			fn(&b[beg], &b[i]);
		}
	}

};

NS_NET_UV_END
#include "HttpDetail.h"

NS_NET_UV_BEGIN



std::string HttpDetail::encode_url(const std::string& s)
{
	std::string result;

	for (auto i = 0; s[i]; i++) {
		switch (s[i]) {
		case ' ':  result += "%20"; break;
		case '+':  result += "%2B"; break;
		case '\r': result += "%0D"; break;
		case '\n': result += "%0A"; break;
		case '\'': result += "%27"; break;
		case ',':  result += "%2C"; break;
		case ':':  result += "%3A"; break;
		case ';':  result += "%3B"; break;
		default:
			auto c = static_cast<uint8_t>(s[i]);
			if (c >= 0x80) {
				result += '%';
				char hex[4];
				size_t len = snprintf(hex, sizeof(hex) - 1, "%02X", c);
				assert(len == 2);
				result.append(hex, len);
			}
			else {
				result += s[i];
			}
			break;
		}
	}

	return result;
}

bool HttpDetail::is_hex(char c, int& v)
{
	if (0x20 <= c && isdigit(c)) {
		v = c - '0';
		return true;
	}
	else if ('A' <= c && c <= 'F') {
		v = c - 'A' + 10;
		return true;
	}
	else if ('a' <= c && c <= 'f') {
		v = c - 'a' + 10;
		return true;
	}
	return false;
}

bool HttpDetail::from_hex_to_i(const std::string& s, size_t i, size_t cnt, int& val)
{
	if (i >= s.size()) {
		return false;
	}

	val = 0;
	for (; cnt; i++, cnt--) {
		if (!s[i]) {
			return false;
		}
		int v = 0;
		if (is_hex(s[i], v)) {
			val = val * 16 + v;
		}
		else {
			return false;
		}
	}
	return true;
}

std::string HttpDetail::from_i_to_hex(uint64_t n)
{
	const char *charset = "0123456789abcdef";
	std::string ret;
	do {
		ret = charset[n & 15] + ret;
		n >>= 4;
	} while (n > 0);
	return ret;
}

size_t HttpDetail::to_utf8(int code, char* buff)
{
	if (code < 0x0080) {
		buff[0] = (code & 0x7F);
		return 1;
	}
	else if (code < 0x0800) {
		buff[0] = (0xC0 | ((code >> 6) & 0x1F));
		buff[1] = (0x80 | (code & 0x3F));
		return 2;
	}
	else if (code < 0xD800) {
		buff[0] = (0xE0 | ((code >> 12) & 0xF));
		buff[1] = (0x80 | ((code >> 6) & 0x3F));
		buff[2] = (0x80 | (code & 0x3F));
		return 3;
	}
	else if (code < 0xE000) { // D800 - DFFF is invalid...
		return 0;
	}
	else if (code < 0x10000) {
		buff[0] = (0xE0 | ((code >> 12) & 0xF));
		buff[1] = (0x80 | ((code >> 6) & 0x3F));
		buff[2] = (0x80 | (code & 0x3F));
		return 3;
	}
	else if (code < 0x110000) {
		buff[0] = (0xF0 | ((code >> 18) & 0x7));
		buff[1] = (0x80 | ((code >> 12) & 0x3F));
		buff[2] = (0x80 | ((code >> 6) & 0x3F));
		buff[3] = (0x80 | (code & 0x3F));
		return 4;
	}

	// NOTREACHED
	return 0;
}

std::string HttpDetail::decode_url(const std::string& s)
{
	std::string result;

	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '%' && i + 1 < s.size()) {
			if (s[i + 1] == 'u') {
				int val = 0;
				if (from_hex_to_i(s, i + 2, 4, val)) {
					// 4 digits Unicode codes
					char buff[4];
					size_t len = to_utf8(val, buff);
					if (len > 0) {
						result.append(buff, len);
					}
					i += 5; // 'u0000'
				}
				else {
					result += s[i];
				}
			}
			else {
				int val = 0;
				if (from_hex_to_i(s, i + 1, 2, val)) {
					// 2 digits hex codes
					result += val;
					i += 2; // '00'
				}
				else {
					result += s[i];
				}
			}
		}
		else if (s[i] == '+') {
			result += ' ';
		}
		else {
			result += s[i];
		}
	}

	return result;
}

void HttpDetail::parse_query_text(const std::string& s, std::map<std::string, std::string>& params)
{
	split(&s[0], &s[s.size()], '&', [&](const char* b, const char* e) {
		std::string key;
		std::string val;
		split(b, e, '=', [&](const char* b, const char* e) {
			if (key.empty()) {
				key.assign(b, e);
			}
			else {
				val.assign(b, e);
			}
		});
		params.emplace(key, decode_url(val));
	});
}

std::map<std::string, std::string> HttpDetail::parse_query_text(const std::string& s)
{
	std::map<std::string, std::string> params;
	parse_query_text(s, params);
	return std::move(params);
}

NS_NET_UV_END

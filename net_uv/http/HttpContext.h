#pragma once

#include "HttpCommon.h"
#include "HttpRequest.h"

NS_NET_UV_BEGIN

class HttpContext
{
public:
	enum HttpRequestParseState
	{
		kExpectRequestLine,
		kExpectHeaders,
		kExpectBody,
		kGotAll,
	};

	HttpContext();

	bool parseRequest(char* data, uint32_t len);

	void reset();

public:

	inline bool gotAll() const;

	inline const HttpRequest& request() const;

	inline HttpRequest& request();

protected:

	bool processRequestLine(const char* begin, const char* end);

	const char* searchCrlf(const char* begin, const char* end);

	const char* searchStr(const char* sbegin, const char* send, const char* tbegin, const char* tend);

private:

	HttpRequestParseState m_state;
	HttpRequest m_request;

	static const char kCRLF[];
};


bool HttpContext::gotAll() const
{
	return m_state == kGotAll;
}

const HttpRequest& HttpContext::request() const
{
	return m_request;
}

HttpRequest& HttpContext::request()
{
	return m_request;
}

NS_NET_UV_END

#pragma once

#include "HttpCommon.h"

NS_NET_UV_BEGIN


struct Pairs
{
	std::string field;
	std::string value;
};

class HttpRequest
{
public:

	HttpRequest();

	bool parse(const char* data, size_t len);

	bool ok();

public:

	const char* methodString() const;

	std::string getHeader(const std::string& field) const;

	std::string getParam(const std::string& key, const std::string& defaultValue) const;

public:

	
	inline http_method method() const;

	inline  const std::string& path() const;

	inline  const std::string& query() const;

	inline const std::vector<Pairs>& headers() const;

	inline const std::map<std::string, std::string>& params() const;

	inline const http_parser parser() const;
	
private:
	void reset();
	void processRequestLine();
	void parseQuery();

	static int on_message_begin(http_parser* parser);
	static int on_url(http_parser* parser, const char* at, size_t length);
	static int on_headers_complete(http_parser* parser);
	static int on_message_complete(http_parser* parser);
	static int on_header_field(http_parser* parser, const char* at, size_t length);
	static int on_header_value(http_parser* parser, const char* at, size_t length);
	static int on_body(http_parser* parser, const char* at, size_t length);

private:
	// example: /a/b/c?name=xxx&lang=en
	std::string m_url;
	// example: a/b/c
	std::string m_path;
	// example: name=xxx&lang=en
	std::string m_query;

	bool m_ok;
	http_parser_settings m_settings;
	http_parser m_parser;

	int m_curHeaderIdx;
	std::vector<Pairs> m_headers;

	std::map<std::string, std::string> m_params;
};

http_method HttpRequest::method() const
{
	return (http_method)m_parser.method;
}

const std::string& HttpRequest::path() const
{
	return m_path;
}

const std::string& HttpRequest::query() const
{
	return m_query;
}

const std::vector<Pairs>& HttpRequest::headers() const
{
	return m_headers;
}

const std::map<std::string, std::string>& HttpRequest::params() const
{
	return m_params;
}

const http_parser HttpRequest::parser() const
{
	return m_parser;
}

NS_NET_UV_END
#pragma once

#include "HttpCommon.h"

NS_NET_UV_BEGIN

class HttpRequest
{
public:
	enum Method
	{
		kInvalid, kGet, kPost, kHead, kPut, kDelete
	};
	enum Version
	{
		kUnknown, kHttp10, kHttp11
	};

	HttpRequest();

	bool setMethod(const char* start, const char* end);

	const char* methodString() const;

	void addHeader(const char* start, const char* colon, const char* end);

	std::string getHeader(const std::string& field) const;

	std::string getParam(const std::string& key, const std::string& defaultValue) const;

	void swap(HttpRequest& that);

public:

	inline void setVersion(Version v);

	inline Version getVersion() const;

	inline Method method() const;

	inline void setPath(const char* start, const char* end);

	inline void setPath(const std::string& path);

	inline  const std::string& path() const;

	inline  void setQuery(const char* start, const char* end);

	inline  void setQuery(const std::string& query);

	inline  const std::string& query() const;

	inline const std::map<std::string, std::string>& headers() const;

	inline const std::map<std::string, std::string>& params() const;

private:

	void parseQuery();

private:
	Method m_method;
	Version m_version;
	std::string m_path;
	std::string m_query;
	std::map<std::string, std::string> m_headers;
	std::map<std::string, std::string> m_params;
};


void HttpRequest::setVersion(Version v)
{
	m_version = v;
}

HttpRequest::Version HttpRequest::getVersion() const
{
	return m_version;
}

HttpRequest::Method HttpRequest::method() const
{
	return m_method;
}

void HttpRequest::setPath(const char* start, const char* end)
{
	m_path.assign(start, end);
}

void HttpRequest::setPath(const std::string& path)
{
	m_path = std::move(path);
}

const std::string& HttpRequest::path() const
{
	return m_path;
}

void HttpRequest::setQuery(const char* start, const char* end)
{
	m_query.assign(start, end);
	parseQuery();
}

void HttpRequest::setQuery(const std::string& query)
{
	m_query = std::move(query);
	parseQuery();
}

const std::string& HttpRequest::query() const
{
	return m_query;
}

const std::map<std::string, std::string>& HttpRequest::headers() const
{
	return m_headers;
}

const std::map<std::string, std::string>& HttpRequest::params() const
{
	return m_params;
}

NS_NET_UV_END
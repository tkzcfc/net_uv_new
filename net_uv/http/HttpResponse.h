#pragma once

#include "HttpCommon.h"

NS_NET_UV_BEGIN

class HttpResponse
{
public:
	enum HttpStatusCode
	{
		kUnknown,
		k200Ok = 200,
		k301MovedPermanently = 301,
		k400BadRequest = 400,
		k404NotFound = 404,
	};

	HttpResponse();

	std::string toString() const;

public:

	inline void setStatusCode(HttpStatusCode code);

	inline void setStatusMessage(const std::string& message);

	inline void setCloseConnection(bool on);

	inline bool closeConnection() const;

	inline void setContentType(const std::string& contentType);

	inline void addHeader(const std::string& key, const std::string& value);

	inline void setBody(const std::string& body);

private:
	std::map<std::string, std::string> m_headers;
	HttpStatusCode m_statusCode;
	std::string m_statusMessage;
	bool m_closeConnection;
	std::string m_body;
};

void HttpResponse::setStatusCode(HttpStatusCode code)
{
	m_statusCode = code;
}

void HttpResponse::setStatusMessage(const std::string& message)
{
	m_statusMessage = message;
}

void HttpResponse::setCloseConnection(bool on)
{
	m_closeConnection = on;
}

bool HttpResponse::closeConnection() const
{
	return m_closeConnection;
}

void HttpResponse::setContentType(const std::string& contentType)
{
	addHeader("Content-Type", contentType);
}

void HttpResponse::addHeader(const std::string& key, const std::string& value)
{
	m_headers[key] = value;
}

void HttpResponse::setBody(const std::string& body)
{
	m_body = body;
}

NS_NET_UV_END

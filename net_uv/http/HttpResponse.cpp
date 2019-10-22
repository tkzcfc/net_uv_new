#include "HttpResponse.h"

NS_NET_UV_BEGIN

HttpResponse::HttpResponse()
	: m_statusCode(kUnknown),
	m_closeConnection(true)
{
}

std::string HttpResponse::toString() const
{
	std::string output;

	char buf[32];
	snprintf(buf, sizeof buf, "HTTP/1.1 %d ", m_statusCode);
	output.append(buf);
	output.append(m_statusMessage);
	output.append("\r\n");

	if (m_closeConnection)
	{
		output.append("Connection: close\r\n");
	}
	else
	{
		snprintf(buf, sizeof buf, "Content-Length: %zd\r\n", m_body.size());
		output.append(buf);
		output.append("Connection: Keep-Alive\r\n");
	}

	for (const auto& header : m_headers)
	{
		output.append(header.first);
		output.append(": ");
		output.append(header.second);
		output.append("\r\n");
	}

	output.append("\r\n");
	output.append(m_body);

	return output;
}


NS_NET_UV_END

#include "HttpRequest.h"

NS_NET_UV_BEGIN

HttpRequest::HttpRequest()
	: m_method(kInvalid),
	m_version(kUnknown)
{
}

bool HttpRequest::setMethod(const char* start, const char* end)
{
	assert(m_method == kInvalid);
	std::string m(start, end);
	if (m == "GET")
	{
		m_method = kGet;
	}
	else if (m == "POST")
	{
		m_method = kPost;
	}
	else if (m == "HEAD")
	{
		m_method = kHead;
	}
	else if (m == "PUT")
	{
		m_method = kPut;
	}
	else if (m == "DELETE")
	{
		m_method = kDelete;
	}
	else
	{
		m_method = kInvalid;
	}
	return m_method != kInvalid;
}

const char* HttpRequest::methodString() const
{
	const char* result = "UNKNOWN";
	switch (m_method)
	{
	case kGet:
		result = "GET";
		break;
	case kPost:
		result = "POST";
		break;
	case kHead:
		result = "HEAD";
		break;
	case kPut:
		result = "PUT";
		break;
	case kDelete:
		result = "DELETE";
		break;
	default:
		break;
	}
	return result;
}

void HttpRequest::addHeader(const char* start, const char* colon, const char* end)
{
	std::string field(start, colon);
	++colon;
	while (colon < end && isspace(*colon))
	{
		++colon;
	}
	std::string value(colon, end);
	while (!value.empty() && isspace(value[value.size() - 1]))
	{
		value.resize(value.size() - 1);
	}
	m_headers[field] = value;
}

std::string HttpRequest::getHeader(const std::string& field) const
{
	std::string result;
	std::map<std::string, std::string>::const_iterator it = m_headers.find(field);
	if (it != m_headers.end())
	{
		result = it->second;
	}
	return result;
}

void HttpRequest::swap(HttpRequest& that)
{
	std::swap(m_method, that.m_method);
	std::swap(m_version, that.m_version);
	m_path.swap(that.m_path);
	m_query.swap(that.m_query);
	m_headers.swap(that.m_headers);
}

template <class Fn>
void split(const char* b, const char* e, char d, Fn fn)
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

NS_NET_UV_END

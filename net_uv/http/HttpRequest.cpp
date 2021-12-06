#include "HttpRequest.h"

NS_NET_UV_BEGIN

/// string split
static void split(const std::string& str, const std::string& delimiter, std::vector<std::string>& result)
{
	std::string strTmp = str;
	size_t cutAt;
	while ((cutAt = strTmp.find_first_of(delimiter)) != strTmp.npos)
	{
		if (cutAt > 0)
		{
			result.push_back(strTmp.substr(0, cutAt));
		}
		strTmp = strTmp.substr(cutAt + 1);
	}

	if (!strTmp.empty())
	{
		result.push_back(strTmp);
	}
}

int HttpRequest::on_message_begin(http_parser* parser)
{
	HttpRequest* self = (HttpRequest*)parser->data;
	self->reset();
	return 0;
}

int HttpRequest::on_headers_complete(http_parser* parser)
{
	HttpRequest* self = (HttpRequest*)parser->data;
	return 0;
}

int HttpRequest::on_message_complete(http_parser* parser)
{
	HttpRequest* self = (HttpRequest*)parser->data;
	self->m_ok = true;
	self->m_curHeaderIdx = -1;
	self->processRequestLine();
	return 0;
}

int HttpRequest::on_url(http_parser* parser, const char* at, size_t length)
{
	HttpRequest* self = (HttpRequest*)parser->data;
	self->m_url.append(at, length);
	return 0;
}

int HttpRequest::on_header_field(http_parser* parser, const char* at, size_t length)
{
	HttpRequest* self = (HttpRequest*)parser->data;

	if (self->m_curHeaderIdx < 0)
	{
		self->m_curHeaderIdx = self->m_headers.size();

		Pairs p;
		self->m_headers.emplace_back(p);
	}

	self->m_headers[self->m_curHeaderIdx].field.append(at, length);

	return 0;
}

int HttpRequest::on_header_value(http_parser* parser, const char* at, size_t length)
{
	HttpRequest* self = (HttpRequest*)parser->data;

	self->m_curHeaderIdx = -1;
	if (self->m_headers.empty())
	{
		assert(0);
		return 1;
	}

	self->m_headers.back().value.append(at, length);

	return 0;
}

int HttpRequest::on_body(http_parser* parser, const char* at, size_t length)
{
	HttpRequest* self = (HttpRequest*)parser->data;
	return 0;
}


HttpRequest::HttpRequest()
	: m_ok(false)
	, m_curHeaderIdx(-1)
{
	memset(&m_settings, 0, sizeof(m_settings));
	m_settings.on_message_begin = on_message_begin;
	m_settings.on_url = on_url;
	m_settings.on_header_field = on_header_field;
	m_settings.on_header_value = on_header_value;
	m_settings.on_headers_complete = on_headers_complete;
	m_settings.on_body = on_body;
	m_settings.on_message_complete = on_message_complete;

	m_parser.data = this;
	http_parser_init(&m_parser, HTTP_REQUEST);
}

bool HttpRequest::parse(const char* data, size_t len)
{
	size_t nparsed = http_parser_execute(&m_parser, &m_settings, data, len);
	return nparsed == len;
}

bool HttpRequest::ok()
{
	return m_ok;
}

const char* HttpRequest::methodString() const
{
	const char* result = "UNKNOWN";
	switch (m_parser.method)
	{
	case http_method::HTTP_DELETE:
		result = "DELETE";
		break;
	case http_method::HTTP_GET:
		result = "GET";
		break;
	case http_method::HTTP_HEAD:
		result = "HEAD";
		break;
	case http_method::HTTP_POST:
		result = "POST";
		break;
	case http_method::HTTP_PUT:
		result = "PUT";
		break;
	default:
		break;
	}
	return result;
}

std::string HttpRequest::getHeader(const std::string& field) const
{
	for (auto& it : m_headers)
	{
		if (it.field == field)
			return it.value;
	}
	return "";
}

std::string HttpRequest::getParam(const std::string& key, const std::string& defaultValue) const
{
	std::map<std::string, std::string>::const_iterator it = m_params.find(key);
	if (it != m_params.end())
	{
		return it->second;
	}
	return defaultValue;
}

void HttpRequest::reset()
{
	m_ok = false;

	m_url.clear();
	m_path.clear();
	m_query.clear();

	m_curHeaderIdx = -1;
	m_headers.clear();

	m_params.clear();
}

void HttpRequest::processRequestLine()
{
	auto pos = m_url.find('?');
	if (pos == std::string::npos)
	{
		m_path = m_url;
	}
	else
	{
		m_path = m_url.substr(0, pos);
		m_query = m_url.substr(pos + 1);
	}

	parseQuery();
}

void HttpRequest::parseQuery()
{
	m_params.clear();

	if (m_query.empty())
		return;

	std::vector<std::string> arrParams;
	split(m_query, "&", arrParams);

	if (arrParams.empty())
		return;

	std::vector<std::string> tmp;
	for (auto& it : arrParams)
	{
		tmp.clear();
		split(it, "=", tmp);
		if (tmp.size() == 2)
		{
			m_params[tmp[0]] = tmp[1];
		}
	}
}

NS_NET_UV_END

#include "HttpContext.h"

NS_NET_UV_BEGIN

const char HttpContext::kCRLF[] = "\r\n";

HttpContext::HttpContext()
: m_state(kExpectRequestLine)
{
}

const char* HttpContext::searchStr(const char* sbegin, const char* send, const char* tbegin, const char* tend)
{
	if (sbegin == NULL || send == NULL || tbegin == NULL || tend == NULL)
		return NULL;

	if (sbegin == send || tbegin == tend)
		return NULL;

	const char* tb = NULL;
	for (const char* b = sbegin; b != send; b++)
	{
		if (*b == tbegin[0])
		{
			const char* s_tmp = b + 1;
			const char* t_tmp = tbegin + 1;
			while (t_tmp != tend && s_tmp != send)
			{
				if (*t_tmp != *s_tmp)
					break;
				s_tmp++;
				t_tmp++;
			}
			if (t_tmp == tend)
				return b;
		}
	}
	return NULL;
}

const char* HttpContext::searchCrlf(const char* begin, const char* end)
{
	const char* crlf = searchStr(begin, end, kCRLF, kCRLF + 2);
	return crlf == begin ? NULL : crlf;
}

bool HttpContext::parseRequest(char* data, uint32_t len)
{
	bool ok = true;
	bool hasMore = true;
	char* buf = data;

	while (hasMore)
	{
		if (m_state == kExpectRequestLine)
		{
			const char* crlf = searchCrlf(buf, data + len);
			if (crlf)
			{
				ok = processRequestLine(buf, crlf);
				if (ok)
				{
					buf = (char*)(crlf + 2);
					m_state = kExpectHeaders;
				}
				else
				{
					hasMore = false;
				}
			}
			else
			{
				hasMore = false;
			}
		}
		else if (m_state == kExpectHeaders)
		{
			const char* crlf = searchCrlf(buf, data + len);
			if (crlf)
			{
				const char* colon = std::find((const char*)buf, crlf, ':');
				if (colon != crlf)
				{
					m_request.addHeader((const char*)buf, colon, crlf);
				}
				else
				{
					m_state = kGotAll;
					hasMore = false;
				}
				buf = (char*)(crlf + 2);
			}
			else
			{
				m_state = kGotAll;
				hasMore = false;
			}
		}
		//else if (m_state == kExpectBody)
		//{
		//}
	}
	return ok;
}

bool HttpContext::processRequestLine(const char* begin, const char* end)
{
	bool succeed = false;
	const char* start = begin;
	const char* space = std::find(start, end, ' ');
	if (space != end && m_request.setMethod(start, space))
	{
		start = space + 1;
		space = std::find(start, end, ' ');
		if (space != end)
		{
			const char* question = std::find(start, space, '?');
			if (question != space)
			{
				m_request.setPath(start, question);
				m_request.setQuery(question + 1, space);
			}
			else
			{
				m_request.setPath(start, space);
			}
			start = space + 1;
			succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
			if (succeed)
			{
				if (*(end - 1) == '1')
				{
					m_request.setVersion(HttpRequest::kHttp11);
				}
				else if (*(end - 1) == '0')
				{
					m_request.setVersion(HttpRequest::kHttp10);
				}
				else
				{
					succeed = false;
				}
			}
		}
	}
	return succeed;
}

void HttpContext::reset()
{
	m_state = kExpectRequestLine;
	HttpRequest dummy;
	m_request.swap(dummy);
}

NS_NET_UV_END

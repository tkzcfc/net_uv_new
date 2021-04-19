#include "NetMsgUtils.h"
#include "../base/Misc.h"

NS_NET_UV_BEGIN

// ¼ÓÃÜKey
const uint32_t net_uv_encodeKey = 362955351U;
const static uint32_t tcp_uv_hashlen = sizeof(uint32_t);

inline uint32_t quick_len(uint32_t len)
{
	if (len > 20) return 20;
	return len;
}

// ¼ÓÃÜ
char* net_uv_encode(const char* data, uint32_t len, uint32_t reserveLen, uint32_t &outLen)
{
	if (len <= 0)
		return NULL;

	outLen = tcp_uv_hashlen + len;

	auto hashValue = net_uv_encodeKey | net_getBufHash(data, quick_len(len));

	char* r = (char*)fc_malloc(outLen + reserveLen);
	char* rr = r + reserveLen;
	memcpy(rr, &hashValue, tcp_uv_hashlen);
	memcpy(rr + tcp_uv_hashlen, data, len);

	return r;
}

// ½âÃÜ
char* net_uv_decode(const char* data, uint32_t len, uint32_t &outLen)
{
	outLen = 0;
	if (len <= tcp_uv_hashlen)
	{
		return NULL;
	}
	uint32_t datalen = len - tcp_uv_hashlen;

	auto hashValue = net_getBufHash(data + tcp_uv_hashlen, quick_len(datalen)) | net_uv_encodeKey;

	unsigned int valueLen = *((unsigned int*)data);

	if (hashValue == valueLen)
	{
		char* p = (char*)fc_malloc(datalen);
		if (p == NULL)
		{
			return NULL;
		}
		memcpy(p, data + tcp_uv_hashlen, datalen);
		outLen = datalen;
		return p;
	}
	return NULL;
}

NS_NET_UV_END
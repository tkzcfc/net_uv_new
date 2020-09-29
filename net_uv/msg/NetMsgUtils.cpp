#include "NetMsgUtils.h"
#include "../base/Misc.h"

NS_NET_UV_BEGIN

// º”√‹Key
const uint32_t net_uv_encodeKey = 362955351U;
const static uint32_t tcp_uv_hashlen = sizeof(uint32_t);

// º”√‹
char* net_uv_encode(const char* data, uint32_t len, uint32_t reserveLen, uint32_t &outLen)
{
	if (len <= 0)
		return NULL;

	outLen = tcp_uv_hashlen + len;

	auto hashValue = net_uv_encodeKey | net_getBufHash(data, len);

	char* r = (char*)fc_malloc(outLen + reserveLen);
	char* rr = r + reserveLen;
	memcpy(rr, &hashValue, tcp_uv_hashlen);
	memcpy(rr + tcp_uv_hashlen, data, len);

	return r;
}

// Ω‚√‹
char* net_uv_decode(const char* data, uint32_t len, uint32_t &outLen)
{
	outLen = 0;
	if (len <= tcp_uv_hashlen)
	{
		return NULL;
	}
	uint32_t datalen = len - tcp_uv_hashlen;

	auto hashValue = net_getBufHash(data + tcp_uv_hashlen, datalen) | net_uv_encodeKey;

	unsigned int valueLen = *((unsigned int*)data);

	if (hashValue == valueLen)
	{
		char* p = (char*)fc_malloc(datalen + 1);
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
#include "NetMsgUtils.h"
#include "../base/Misc.h"

NS_NET_UV_BEGIN

// 加密Key
const char net_uv_encodeKey[] = "encodeKey";
const uint32_t net_uv_encodeKeyLen = sizeof(net_uv_encodeKey);

const static uint32_t tcp_uv_hashlen = sizeof(unsigned int);


// 加密
// 加密前 ：|-DATA-|
// 加密后 ：|-（MD5（DATA+加密key））的hash值-|-DATA-|
char* net_uv_encode(MD5& M, const char* data, uint32_t len, uint32_t &outLen)
{
	outLen = tcp_uv_hashlen + len;

	char* r = (char*)fc_malloc(outLen + net_uv_encodeKeyLen);

	memcpy(r + tcp_uv_hashlen, data, len);
	memcpy(r + outLen, net_uv_encodeKey, net_uv_encodeKeyLen);

	M.reset();
	M.update(r + tcp_uv_hashlen, len + net_uv_encodeKeyLen);

	auto md5s = M.toString();

	auto hashvalue = net_getBufHash(md5s.c_str(), md5s.size());
	memcpy(r, &hashvalue, tcp_uv_hashlen);

	return r;
}

// 解密
char* net_uv_decode(MD5& M, const char* data, uint32_t len, uint32_t &outLen)
{
	outLen = 0;

	int32_t datalen = len - tcp_uv_hashlen;

	char* p = (char*)fc_malloc(datalen + net_uv_encodeKeyLen + 1);

	memcpy(p, data + tcp_uv_hashlen, datalen);
	memcpy(p + datalen, net_uv_encodeKey, net_uv_encodeKeyLen);

	M.reset();
	M.update(p, datalen + net_uv_encodeKeyLen);

	auto md5s = M.toString();

	uint32_t hashvalue = net_getBufHash(md5s.c_str(), (uint32_t)md5s.size());
	if (hashvalue == *(uint32_t*)(data))
	{
		outLen = datalen;
	}

	//该数据不合法
	if (outLen == 0)
	{
		fc_free(p);
		p = NULL;
	}
	else
	{
		p[datalen] = '\0';
	}
	return p;
}

NS_NET_UV_END
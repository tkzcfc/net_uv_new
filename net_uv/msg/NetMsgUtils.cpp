#include "NetMsgUtils.h"
#include "../base/Misc.h"

NS_NET_UV_BEGIN

// ����Key
const char net_uv_encodeKey[] = "encodeKey";
const uint32_t net_uv_encodeKeyLen = sizeof(net_uv_encodeKey);

const static uint32_t tcp_uv_hashlen = sizeof(unsigned int);


// ����
// ����ǰ ��|-DATA-|
// ���ܺ� ��|-��MD5��DATA+����key������hashֵ-|-DATA-|
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

// ����
char* net_uv_decode(MD5& M, const char* data, uint32_t len, uint32_t &outLen)
{
	outLen = 0;

	int32_t datalen = len - tcp_uv_hashlen;
	if (datalen <= 0)
	{
		return NULL;
	}

	char* p = (char*)fc_malloc(datalen + net_uv_encodeKeyLen + 1);
	if (p == NULL)
	{
		return NULL;
	}

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

	//�����ݲ��Ϸ�
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
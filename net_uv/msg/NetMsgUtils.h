#pragma once

#include "NetMsgMgr.h"

NS_NET_UV_BEGIN

// �Ƿ���������У��
#define UV_ENABLE_DATA_CHECK 1
// �����Ϣ����10mb
#define NET_UV_MAX_MSG_LEN (10485760)
// �������Ƭ��С
#define NET_UV_WRITE_MAX_LEN (1400)

struct NetMsgHead
{
	uint32_t len;// ��Ϣ���ȣ����������ṹ��
	uint32_t id; // ��ϢID
	enum NetMsgType
	{
		PING,
		PONG,
		MSG
	}type;
};

const static uint32_t NetMsgHeadLen = sizeof(NetMsgHead);

//����
char* net_uv_encode(const char* data, uint32_t len, uint32_t reserveLen, uint32_t &outLen);
//����
char* net_uv_decode(const char* data, uint32_t len, uint32_t &outLen);

NS_NET_UV_END
#pragma once

#include "NetMsgMgr.h"

NS_NET_UV_BEGIN

// 是否启用数据校验
#define UV_ENABLE_DATA_CHECK 1
// 最大消息长度10mb
#define NET_UV_MAX_MSG_LEN (10485760)
// 网络包分片大小
#define NET_UV_WRITE_MAX_LEN (1400)

struct NetMsgHead
{
	uint32_t len;// 消息长度，不包括本结构体
	uint32_t id; // 消息ID
	enum NetMsgType
	{
		PING,
		PONG,
		MSG
	}type;
};

const static uint32_t NetMsgHeadLen = sizeof(NetMsgHead);

//加密
char* net_uv_encode(const char* data, uint32_t len, uint32_t reserveLen, uint32_t &outLen);
//解密
char* net_uv_decode(const char* data, uint32_t len, uint32_t &outLen);

NS_NET_UV_END
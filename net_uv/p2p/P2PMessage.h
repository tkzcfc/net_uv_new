#pragma once

#include "P2PCommon.h"

NS_NET_UV_BEGIN

// 消息ID
enum P2PMessageID
{
	P2P_MSG_ID_BEGIN = 1000,

	P2P_MSG_ID_JSON_BEGIN,

	P2P_MSG_ID_PING,
	P2P_MSG_ID_PONG,

	P2P_MSG_ID_CREATE_KCP,					// KCP创建 1004
	P2P_MSG_ID_CREATE_KCP_RESULT,			// KCP创建结果 1005

	P2P_MSG_ID_C2C_DISCONNECT,				// 断开 1006
	
	P2P_MSG_ID_C2T_CLIENT_LOGIN,			// 客户端登录 1007
	P2P_MSG_ID_T2C_CLIENT_LOGIN_RESULT,		// 客户端登录结果 1008
	
	P2P_MSG_ID_T2C_START_BURROW,			// 开始打洞指令 1009

	P2P_MSG_ID_C2C_HELLO,					// 打洞消息 1010

	P2P_MSG_ID_C2T_CHECK_PEER,				// 向Turn查询某个客户端信息 1011
	P2P_MSG_ID_C2T_CHECK_PEER_RESULT,		// 1012
	
	P2P_MSG_ID_JSON_END,

	P2P_MSG_ID_KCP,							// KCP消息

	P2P_MSG_ID_END,
};

// 消息结构
struct P2PMessage
{
	uint32_t msgID;		// 消息ID
	uint32_t msgLen;	// 消息长度(不包括本结构体)
	uint64_t uniqueID;  // 该条消息唯一ID
};

// 地址信息
union AddrInfo
{
	uint64_t key;		// key : 前四字节为IP,后四字节为端口
	struct
	{
		uint32_t ip;	// IP
		uint32_t port;  // 端口
	};
};

// P2P节点信息
struct P2PNodeInfo
{
	// 公网地址信息
	AddrInfo addr;
};

// 本地网络信息
struct LocNetAddrInfo
{
	AddrInfo addr;
	uint32_t mask;
};

static const char* P2P_NULL_JSON = "{}";
static uint32_t P2P_NULL_JSON_LEN = 3;

NS_NET_UV_END

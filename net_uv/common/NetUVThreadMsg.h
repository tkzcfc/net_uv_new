#pragma once

#include "../base/Macros.h"

NS_NET_UV_BEGIN

//消息类型
enum class NetThreadMsgType : uint32_t
{
	CONNECT_FAIL,		//连接失败
	CONNECT_TIMOUT,		//连接超时
	CONNECT_SESSIONID_EXIST,//会话ID已存在，且新连接IP和端口和之前会话不一致
	CONNECT_ING,		//正在连接
	CONNECT,			//连接成功
	NEW_CONNECT,		//新连接
	DIS_CONNECT,		//断开连接
	EXIT_LOOP,			//退出loop
	RECV_DATA,			//收到消息
	REMOVE_SESSION,		//移除会话
};

class Session;

#pragma pack(4)
struct NetThreadMsg
{
	NetThreadMsgType msgType;
	Session* pSession;
	char* data;
	uint32_t dataLen;
};
#pragma pack()

NS_NET_UV_END

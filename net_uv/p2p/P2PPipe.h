#pragma once

#include "P2PCommon.h"
#include "P2PMessage.h"

NS_NET_UV_BEGIN

using P2PPipeRecvJsonCallback = std::function<void(P2PMessageID msgID, rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)>;
using P2PPipeRecvKcpCallback = std::function<void(char* data, uint32_t len, uint64_t key, const struct sockaddr* addr)>;
using P2PPipeNewSessionCallback = std::function<void(uint64_t key)>;
using P2PPipeNewKcpCreateCallback = std::function<void(uint64_t key, uint32_t tag)>;
using P2PPipeRemoveSessionCallback = std::function<void(uint64_t key)>;

class P2PPipe;
struct SessionData
{
	// 心跳无响应次数
	uint8_t noResponseCount;
	// 最后一次心跳检测时间
	uint32_t lastCheckTime;
	// 地址
	sockaddr_in send_addr;
	// 是否开始检测
	bool isStartCheck;
	// 延迟时间
	uint32_t delayTime;
	// kcp
	ikcpcb* kcp;
	//pipe
	P2PPipe* pipe;
};

class P2PPipe
{
public:

	P2PPipe();

	P2PPipe(const P2PPipe&) = delete;

	virtual ~P2PPipe();
	
	bool bind(const char* bindIP, uint32_t binPort, uv_loop_t* loop);

	bool kcpSend(char* data, uint32_t len, uint32_t toIP, uint32_t toPort);

	bool kcpSend(char* data, uint32_t len, uint64_t key);

	void send(P2PMessageID msgID, const char* data, int32_t len, uint32_t toIP, uint32_t toPort);

	void send(P2PMessageID msgID, const char* data, int32_t len, const struct sockaddr* addr);

	void disconnect(uint64_t key);

	void update(uint32_t updateTime);

	bool isContain(uint64_t key);

	void close();

	inline UDPSocket* getSocket();

	inline void setRecvJsonCallback(const P2PPipeRecvJsonCallback& call);

	inline void setRecvKcpCallback(const P2PPipeRecvKcpCallback& call);

	inline void setNewSessionCallback(const P2PPipeNewSessionCallback& call);

	inline void setNewKcpCreateCallback(const P2PPipeNewKcpCreateCallback& call);

	inline void setRemoveSessionCallback(const P2PPipeRemoveSessionCallback& call);

protected:

	void heartCheck(uint32_t interval);
	
	void shutdownSocket();

	virtual void on_udp_read(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags);

	void on_recv_kcpMsg(uint64_t key, char* data, uint32_t len, const struct sockaddr* addr);

	void on_recv_pong(uint64_t key, rapidjson::Document& document, const struct sockaddr* addr);

	void on_recv_createKcp(uint64_t key, rapidjson::Document& document, const struct sockaddr* addr);

	void on_recv_createKcpResult(uint64_t key, rapidjson::Document& document, const struct sockaddr* addr);

	void on_recv_disconnect(uint64_t key, rapidjson::Document& document, const struct sockaddr* addr);

	void createKcp(uint64_t key, uint32_t conv, uint32_t tag);

	void recvData(uint64_t key, const struct sockaddr* addr);

	void onSessionRemove(std::map<uint64_t, SessionData>::iterator& it);

protected:

	static int32_t udp_output(const char *buf, int32_t len, ikcpcb *kcp, void *user);

protected:

	UDPSocket* m_socket;

	std::map<uint64_t, SessionData> m_allSessionDataMap;
	// 更新间隔
	uint32_t m_updateInterval;
	// 更新时间
	uint32_t m_updateTime;

	char *m_recvBuf;

	P2PPipeRecvJsonCallback m_recvJsonCallback;
	P2PPipeRecvKcpCallback m_recvKcpCallback;
	P2PPipeNewSessionCallback m_newSessionCallback;
	P2PPipeNewKcpCreateCallback m_newKcpCreateCallback;
	P2PPipeRemoveSessionCallback m_removeSessionCallback;
};

UDPSocket* P2PPipe::getSocket()
{
	return m_socket;
}

void P2PPipe::setRecvJsonCallback(const P2PPipeRecvJsonCallback& call)
{
	m_recvJsonCallback = std::move(call);
}

void P2PPipe::setRecvKcpCallback(const P2PPipeRecvKcpCallback& call)
{
	m_recvKcpCallback = std::move(call);
}

void P2PPipe::setNewSessionCallback(const P2PPipeNewSessionCallback& call)
{
	m_newSessionCallback = std::move(call);
}

void P2PPipe::setNewKcpCreateCallback(const P2PPipeNewKcpCreateCallback& call)
{
	m_newKcpCreateCallback = std::move(call);
}

void P2PPipe::setRemoveSessionCallback(const P2PPipeRemoveSessionCallback& call)
{
	m_removeSessionCallback = std::move(call);
}

NS_NET_UV_END

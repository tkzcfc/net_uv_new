#pragma once

#include "KCPCommon.h"
#include "KCPUtils.h"
#include "KCPSocketManager.h"

NS_NET_UV_BEGIN

// 新连接过滤回调，用于过滤黑名单 返回false表示不接受该连接
using KCPSocketConnectFilterCall = std::function<bool(const struct sockaddr*)>;
using KCPSocketNewConnectionCall = std::function<void(Socket*)>;

class KCPSocket : public Socket
{
public:
	KCPSocket() = delete;

	KCPSocket(const KCPSocket&) = delete;

	KCPSocket(uv_loop_t* loop);

	virtual ~KCPSocket();

	virtual bool bind(const char* ip, uint32_t port)override;

	virtual bool bind6(const char* ip, uint32_t port)override;

	virtual bool listen(int32_t count)override;

	virtual bool connect(const char* ip, uint32_t port)override;

	virtual bool send(char* data, int32_t len)override;

	virtual void disconnect()override;

	bool accept(const struct sockaddr* addr, uint32_t synValue);

	inline void setNewConnectionCallback(const KCPSocketNewConnectionCall& call);
	inline void setConnectFilterCallback(const KCPSocketConnectFilterCall& call);

protected:
	inline uv_udp_t* getUdp();
	
	inline void setWeakRefSocketManager(KCPSocketManager* manager);

	void svr_connect(struct sockaddr* addr, IUINT32 conv, uint32_t synValue);

	void shutdownSocket();

	void setSocketAddr(struct sockaddr* addr);

	inline struct sockaddr* getSocketAddr();

	void udpSend(const char* data, int32_t len);

	void kcpInput(const char* data, long size);

	void initKcp(IUINT32 conv);

	void releaseKcp();

	void onUdpRead(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, uint32_t flags);

	void connectResult(int32_t status);
	
	inline void setConv(IUINT32 conv);

	inline IUINT32 getConv();

	void startIdle();

	inline void stopIdle();

	void idleLogic();

	void tryCloseSocket(uv_handle_t* handle);

	void acceptFailed();

	void setAutoSendData(const char* data, uint32_t len);

	void cancelCurAutoSend();

	void autoSendLogic();

	void resetCheckTimer();
	
protected:

	enum class State
	{
		LISTEN,
		STOP_LISTEN,

		SYN_SENT,	// client state: send syn
		SYN_RECV,	// svr state:send syn+ack
		ESTABLISHED,// client & svr state

		FIN_WAIT,	// client state: close wait
		CLOSE_WAIT,	// svr state: close wait
		CLOSED,		// client & svr state
	};

	enum KCPSOType
	{
		SVR_SO,
		CLI_SO,
	};

	KCPSOType m_soType;
	State m_kcpState;

	uv_udp_t* m_udp;
	UVIdle* m_idle;
	struct sockaddr* m_socketAddr;
	char* m_recvBuf;

	KCPSocketManager* m_socketMng;
	bool m_weakRefTag;

	char* m_autoSend_Data;
	uint32_t m_autoSend_DataLen;
	int32_t m_autoSendCount;

	ikcpcb* m_kcp;
	IUINT32 m_conv;
	uint64_t m_idleUpdateTime;
	uint64_t m_lastKcpRecvTime;

	uint32_t m_heartbeatLoseCount;

	int32_t m_randValue;

	KCPSocketNewConnectionCall m_newConnectionCall;
	KCPSocketConnectFilterCall m_connectFilterCall;

	friend class KCPSocketManager;
	friend class KCPServer;
	friend class KCPSession;
protected:
	static void uv_on_udp_send(uv_udp_send_t *req, int32_t status);
	static void uv_on_after_read(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags);
	static int32_t udp_output(const char *buf, int32_t len, ikcpcb *kcp, void *user);
};

uv_udp_t* KCPSocket::getUdp()
{
	return m_udp;
}

void KCPSocket::setWeakRefSocketManager(KCPSocketManager* manager)
{
	m_weakRefTag = true;
	m_socketMng = manager;
}

struct sockaddr* KCPSocket::getSocketAddr()
{
	return m_socketAddr;
}

void KCPSocket::setNewConnectionCallback(const KCPSocketNewConnectionCall& call)
{
	m_newConnectionCall = std::move(call);
}

void KCPSocket::setConnectFilterCallback(const KCPSocketConnectFilterCall& call)
{
	m_connectFilterCall = std::move(call);
}

void KCPSocket::setConv(IUINT32 conv)
{
	m_conv = conv;
}

IUINT32 KCPSocket::getConv()
{
	return m_conv;
}

NS_NET_UV_END


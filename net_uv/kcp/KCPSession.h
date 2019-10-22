#pragma once

#include "KCPCommon.h"
#include "KCPSocket.h"

NS_NET_UV_BEGIN

class KCPSession : public Session
{
public:
	KCPSession() = delete;
	KCPSession(const KCPSession&) = delete;
	virtual ~KCPSession();

	/// Session
	virtual uint32_t getPort()override;

	virtual std::string getIp()override;
	
protected:

	static KCPSession* createSession(SessionManager* sessionManager, KCPSocket* socket);

	KCPSession(SessionManager* sessionManager);

protected:
	
	/// Session
	virtual void executeSend(char* data, uint32_t len)override;

	virtual void executeDisconnect()override;

	virtual bool executeConnect(const char* ip, uint32_t port)override;

	/// KCPSession
	inline void setKCPSocket(KCPSocket* socket);

	inline KCPSocket* getKCPSocket();

	bool init(KCPSocket* socket);

	void on_socket_close(Socket* socket);

	void on_socket_recv(char* data, ssize_t len);

	friend class KCPClient;
	friend class KCPServer;

protected:

	KCPSocket* m_socket;
};

void KCPSession::setKCPSocket(KCPSocket* socket)
{
	m_socket = socket;
}

KCPSocket* KCPSession::getKCPSocket()
{
	return m_socket;
}


NS_NET_UV_END
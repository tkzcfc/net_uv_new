#pragma once

#include "TCPSocket.h"

NS_NET_UV_BEGIN


class TCPSession : public Session
{
public:
	TCPSession() = delete;
	TCPSession(const TCPSession&) = delete;
	virtual ~TCPSession();

	virtual uint32_t getPort()override;

	virtual std::string getIp()override;
	
protected:

	static TCPSession* createSession(SessionManager* sessionManager, TCPSocket* socket);

	TCPSession(SessionManager* sessionManager);

protected:

	virtual void executeSend(char* data, uint32_t len) override;

	virtual void executeDisconnect() override;

	virtual bool executeConnect(const char* ip, uint32_t port)override;

protected:

	bool init(TCPSocket* socket);
	
	inline TCPSocket* getTCPSocket();

protected:

	void on_socket_recv(char* data, ssize_t len);

	void on_socket_close(Socket* socket);

	friend class TCPServer;
	friend class TCPClient;

protected:	
	TCPSocket* m_socket;
};

TCPSocket* TCPSession::getTCPSocket()
{
	return m_socket;
}

NS_NET_UV_END

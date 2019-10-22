#pragma once

#include "Common.h"
#include "Session.h"
#include "SessionManager.h"
#include "Runnable.h"
#include "../common/NetUVThreadMsg.h"

NS_NET_UV_BEGIN

class Server;
using ServerCloseCall = std::function<void(Server* svr)>;
using ServerNewConnectCall = std::function<void(Server* svr, Session* session)>;
using ServerRecvCall = std::function<void(Server* svr, Session* session, char* data, uint32_t len)>;
using ServerDisconnectCall = std::function<void(Server* svr, Session* session)>;

enum class ServerStage
{
	RUN,
	WAIT_CLOSE_SERVER_SOCKET,// Wait for server socket to close
	CLEAR,		
	WAIT_SESSION_CLOSE,// Wait for session to close
	STOP
};

class Server : public Runnable, public SessionManager
{
public:
	Server();
	virtual ~Server();

	virtual bool startServer(const char* ip, uint32_t port, bool isIPV6, int32_t maxCount);

	virtual bool stopServer() = 0;

	// main thread polling
	virtual void updateFrame() = 0;

	inline void setCloseCallback(const ServerCloseCall& call);
	
	inline void setNewConnectCallback(const ServerNewConnectCall& call);

	inline void setRecvCallback(const ServerRecvCall& call);

	inline void setDisconnectCallback(const ServerDisconnectCall& call);

	virtual std::string getIP();
	
	virtual uint32_t getPort();

	virtual uint32_t getListenPort();

	virtual bool isIPV6();

	virtual bool isCloseFinish();

protected:

	inline void setListenPort(uint32_t port);

protected:
	ServerCloseCall m_closeCall;
	ServerNewConnectCall m_newConnectCall;
	ServerRecvCall m_recvCall;
	ServerDisconnectCall m_disconnectCall;
	
	std::string m_ip;
	uint32_t m_port;
	uint32_t m_listenPort;
	bool m_isIPV6;

	// svr stage
	ServerStage m_serverStage;
};

void Server::setCloseCallback(const ServerCloseCall& call)
{
	m_closeCall = std::move(call);
}

void Server::setNewConnectCallback(const ServerNewConnectCall& call)
{
	m_newConnectCall = std::move(call);
}

void Server::setRecvCallback(const ServerRecvCall& call)
{
	m_recvCall = std::move(call);
}

void Server::setDisconnectCallback(const ServerDisconnectCall& call)
{
	m_disconnectCall = std::move(call);
}

void Server::setListenPort(uint32_t port)
{
	m_listenPort = port;
}

NS_NET_UV_END

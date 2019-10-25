#pragma once


#include "KCPSocket.h"
#include "KCPSession.h"

NS_NET_UV_BEGIN

class KCPServer : public Server
{
	struct serverSessionData
	{
		serverSessionData()
		{
			isInvalid = false;
			session = NULL;
		}
		KCPSession* session;
		bool isInvalid;
	};

public:

	KCPServer();
	KCPServer(const KCPServer&) = delete;

	virtual ~KCPServer();

	/// Server
	virtual bool startServer(const char* ip, uint32_t port, bool isIPV6, int32_t maxCount)override;

	virtual bool stopServer()override;

	virtual void updateFrame()override;

	/// SessionManager
	virtual void send(uint32_t sessionID, char* data, uint32_t len)override;

	virtual void sendEx(uint32_t sessionID, char* data, uint32_t len)override;

	virtual void disconnect(uint32_t sessionID)override;

protected:

	/// Runnable
	virtual void run()override;

	/// SessionManager
	virtual void executeOperation()override;

	/// KCPServer
	void onNewConnect(Socket* socket);

	void onServerSocketClose(Socket* svr);

	bool onServerSocketConnectFilter(const struct sockaddr* addr);

	void onSessionRecvData(Session* session, char* data, uint32_t len);

	/// Server
	virtual void onIdleRun()override;

	virtual void onTimerUpdateRun() {};

protected:

	void startFailureLogic();

	void addNewSession(KCPSession* session);

	void onSessionClose(Session* session);

	void clearData();

protected:
	bool m_start;

	KCPSocket* m_server;

	std::map<uint32_t, serverSessionData> m_allSession;
};



NS_NET_UV_END

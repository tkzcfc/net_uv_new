#pragma once


#include "TCPSocket.h"
#include "TCPSession.h"

NS_NET_UV_BEGIN

class TCPServer : public Server
{
	struct serverSessionData
	{
		serverSessionData()
		{
			isInvalid = false;
			session = NULL;
		}
		TCPSession* session;
		bool isInvalid;
	};

public:

	TCPServer();
	TCPServer(const TCPServer&) = delete;

	virtual ~TCPServer();

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

	virtual void onTimerUpdateRun() {};

	/// SessionManager
	virtual void executeOperation()override;

	/// TCPServer
	void onNewConnect(uv_stream_t* server, int32_t status);

	void onServerSocketClose(Socket* svr);
	
	void onSessionRecvData(Session* session, char* data, uint32_t len);

	/// Server
	virtual void onIdleRun()override;
	
protected:

	void startFailureLogic();

	void addNewSession(TCPSession* session);

	void onSessionClose(Session* session);

	void clearData();

protected:
	bool m_start;

	TCPSocket* m_server;

	std::map<uint32_t, serverSessionData> m_allSession;

	uint32_t m_sessionID;
};



NS_NET_UV_END

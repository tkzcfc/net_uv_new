#pragma once

#include "TCPSocket.h"
#include "TCPSession.h"

NS_NET_UV_BEGIN

class TCPClient : public Client
{
protected:

	struct clientSessionData
	{
		clientSessionData() {}
		~clientSessionData() {}
		CONNECTSTATE connectState;
		bool removeTag;
		std::string ip;
		uint32_t port;
		TCPSession* session;
	};

public:

	TCPClient();
	TCPClient(const TCPClient&) = delete;
	virtual ~TCPClient();

	/// Client
	virtual void connect(const char* ip, uint32_t port, uint32_t sessionId)override;

	virtual void closeClient()override;
	
	virtual void updateFrame()override;

	virtual void removeSession(uint32_t sessionId)override;

	/// SessionManager
	virtual void disconnect(uint32_t sessionId)override;

	virtual void send(uint32_t sessionId, char* data, uint32_t len)override;

	virtual void sendEx(uint32_t sessionId, char* data, uint32_t len)override;

	bool setSocketNoDelay(bool enable);

	bool setSocketKeepAlive(int32_t enable, uint32_t delay);

protected:

	/// Runnable
	virtual void run()override;

	/// SessionManager
	virtual void executeOperation()override;

	/// Client
	virtual void onIdleRun()override;

	virtual void onTimerUpdateRun()override;

	/// TCPClient
	void onSocketConnect(Socket* socket, int32_t status);

	void onSessionClose(Session* session);

	void onSessionRecvData(Session* session, char* data, uint32_t len);

	void createNewConnect(void* data);

	void clearData();

	clientSessionData* getClientSessionDataBySessionId(uint32_t sessionId);

	clientSessionData* getClientSessionDataBySession(Session* session);
	

protected:
	bool m_enableNoDelay;	
	int32_t m_enableKeepAlive; 
	uint32_t m_keepAliveDelay;

	std::map<uint32_t, clientSessionData*> m_allSessionMap;
	
	bool m_isStop;
};
NS_NET_UV_END



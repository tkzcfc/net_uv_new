#pragma once

#include "HttpCommon.h"
#include <unordered_map>

NS_NET_UV_BEGIN

class HttpRequest;
class HttpResponse;
class HttpContext;

using HttpServerCallback = std::function<void(const HttpRequest&, HttpResponse*)>;
using HttpServerCloseCallback = std::function<void()>;

class HttpServer
{
public:

	HttpServer();

	~HttpServer();

	bool startServer(const char* ip, uint32_t port, bool isIPV6, int32_t maxCount);

	bool stopServer();

	void updateFrame();

	uint32_t getActiveCount();

	void disconnectAllSession();

	uint32_t getListenPort();

public:
	inline Session* getCurSession();

	inline void setHttpCallback(const HttpServerCallback& call);

	inline void setCloseCallback(const HttpServerCloseCallback& call);

protected:

	void onSvrNewConnectCallback(Server* svr, Session* session);

	void onSvrRecvCallback(Server* svr, Session* session, char* data, uint32_t len);

	void onSvrDisconnectCallback(Server* svr, Session* session);

private:

	HttpServerCallback m_svrCallback;

	HttpServerCloseCallback m_svrCloseCallback;

	TCPServer* m_svr;

	std::unordered_map<Session*, HttpContext*> m_contextMap;

	Session* m_curSession;
};

Session* HttpServer::getCurSession()
{
	return m_curSession;
}

void HttpServer::setHttpCallback(const HttpServerCallback& call)
{
	m_svrCallback = std::move(call);
}

void HttpServer::setCloseCallback(const HttpServerCloseCallback& call)
{
	m_svrCloseCallback = std::move(call);
}

NS_NET_UV_END

#include "HttpServer.h"
#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

NS_NET_UV_BEGIN

HttpServer::HttpServer()
	: m_svrCloseCallback(nullptr)
	, m_curSession(nullptr)
{
	m_svr = (TCPServer*)fc_malloc(sizeof(TCPServer));
	new(m_svr) TCPServer();

	m_svr->setNewConnectCallback(std::bind(&HttpServer::onSvrNewConnectCallback, this, std::placeholders::_1, std::placeholders::_2));
	m_svr->setRecvCallback(std::bind(&HttpServer::onSvrRecvCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	m_svr->setDisconnectCallback(std::bind(&HttpServer::onSvrDisconnectCallback, this, std::placeholders::_1, std::placeholders::_2));
	m_svr->setCloseCallback([this](Server*) 
	{
		if (m_svrCloseCallback != nullptr)
		{
			m_svrCloseCallback();
		}
	});

	m_svrCallback = [](const HttpRequest&, HttpResponse* resp) 
	{
		resp->setStatusCode(HttpResponse::k404NotFound);
		resp->setStatusMessage("Not Found");
		resp->setCloseConnection(true);
	};
}

HttpServer::~HttpServer()
{
	if (m_svr)
	{
		m_svr->~TCPServer();
		fc_free(m_svr);
		m_svr = NULL;
	}
}

bool HttpServer::startServer(const char* ip, uint32_t port, bool isIPV6, int32_t maxCount)
{
	return m_svr->startServer(ip, port, isIPV6, maxCount);
}

bool HttpServer::stopServer()
{
	return m_svr->stopServer();
}

void HttpServer::updateFrame()
{
	m_svr->updateFrame();
}

uint32_t HttpServer::getActiveCount()
{
	return (uint32_t)m_contextMap.size();
}

void HttpServer::disconnectAllSession()
{
	for (auto &it : m_contextMap)
	{
		it.first->disconnect();
	}
}

uint32_t HttpServer::getListenPort()
{
	return m_svr->getListenPort();
}

void HttpServer::onSvrNewConnectCallback(Server* svr, Session* session)
{
	HttpContext* context = (HttpContext*)fc_malloc(sizeof(HttpContext));
	new(context) HttpContext();
	m_contextMap[session] = context;
}

void HttpServer::onSvrRecvCallback(Server* svr, Session* session, char* data, uint32_t len)
{
	m_curSession = session;

	HttpContext* context = m_contextMap[session];

	if (!context->parseRequest(data, len))
	{
		static const char* badRequest = "HTTP/1.1 400 Bad Request\r\n\r\n";
		session->send((char*)badRequest, sizeof(badRequest));
	}

	if (context->gotAll())
	{
		const auto & req = context->request();

		const std::string& connection = req.getHeader("Connection");
		bool close = connection == "close" || (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
		
		HttpResponse response;
		response.setCloseConnection(close);
		
		m_svrCallback(req, &response);
		
		std::string res = response.toString();

		session->send((char*)res.c_str(), res.size());
		if (response.closeConnection())
		{
			session->disconnect();
		}

		context->reset();
	}
	else
	{
		session->disconnect();
	}

	m_curSession = nullptr;
}

void HttpServer::onSvrDisconnectCallback(Server* svr, Session* session)
{
	HttpContext* context = m_contextMap[session];
	context->~HttpContext();
	fc_free(context);
	m_contextMap.erase(session);
}

NS_NET_UV_END

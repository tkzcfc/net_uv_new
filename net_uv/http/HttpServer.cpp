#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

NS_NET_UV_BEGIN


const static size_t HTTP_WRITE_CHUNK_SIZE = 1400;

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
	return (uint32_t)m_requestMap.size();
}

void HttpServer::disconnectAllSession()
{
	for (auto &it : m_requestMap)
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
	HttpRequest* request = (HttpRequest*)fc_malloc(sizeof(HttpRequest));
	new(request) HttpRequest();
	m_requestMap[session] = request;
}

void HttpServer::onSvrRecvCallback(Server* svr, Session* session, char* data, uint32_t len)
{
	m_curSession = session;
	HttpRequest* request = m_requestMap[session];

	if (!request->parse(data, (size_t)len))
	{
		session->disconnect();
		m_curSession = NULL;
		return;
	}

	if (request->ok())
	{
		const std::string& connection = request->getHeader("Connection");

		bool close = connection == "close";
		if(!close)
		{
			auto major = request->parser().http_major;
			auto minor = request->parser().http_minor;
			if (major == 1 && minor == 0 && connection != "Keep-Alive")
			{
				close = true;
			}
		}

		HttpResponse response;
		response.setCloseConnection(close);

		m_svrCallback(*request, &response);

		std::string output;
		response.toString(output);
		
		auto count = output.size() / HTTP_WRITE_CHUNK_SIZE;
		if (output.size() % HTTP_WRITE_CHUNK_SIZE != 0) count++;

		size_t send_size = 0;
		for (auto i = 0; i < count - 1; ++i)
		{
			session->send((char*)(output.c_str() + send_size), HTTP_WRITE_CHUNK_SIZE);
			send_size += HTTP_WRITE_CHUNK_SIZE;
		}

		auto spare = output.size() - send_size;

		if (response.closeConnection())
			session->sendAndClose((char*)(output.c_str() + send_size), spare);
		else
			session->send((char*)(output.c_str() + send_size), spare);
	}
	m_curSession = NULL;
}

void HttpServer::onSvrDisconnectCallback(Server* svr, Session* session)
{
	HttpRequest* request = m_requestMap[session];
	request->~HttpRequest();
	fc_free(request);
	m_requestMap.erase(session);
}

NS_NET_UV_END

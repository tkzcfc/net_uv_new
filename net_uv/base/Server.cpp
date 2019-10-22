#include "Server.h"


NS_NET_UV_BEGIN

Server::Server()
	: m_closeCall(nullptr)
	, m_newConnectCall(nullptr)
	, m_recvCall(nullptr)
	, m_disconnectCall(nullptr)
	, m_port(0)
	, m_listenPort(0)
	, m_isIPV6(false)
	, m_serverStage(ServerStage::STOP)
{}

Server::~Server()
{}

bool Server::startServer(const char* ip, uint32_t port, bool isIPV6, int32_t maxCount)
{
	assert(m_closeCall != nullptr);
	assert(m_newConnectCall != nullptr);
	assert(m_recvCall != nullptr);
	assert(m_disconnectCall != nullptr);

	m_ip = ip;
	m_port = port;
	m_isIPV6 = isIPV6;
	m_listenPort = port;
	
	return true;
}

std::string Server::getIP()
{
	return m_ip;
}

uint32_t Server::getPort()
{
	return m_port;
}

uint32_t Server::getListenPort()
{
	return m_listenPort;
}

bool Server::isIPV6()
{
	return m_isIPV6;
}

bool Server::isCloseFinish()
{
	return (m_serverStage == ServerStage::STOP);
}

NS_NET_UV_END

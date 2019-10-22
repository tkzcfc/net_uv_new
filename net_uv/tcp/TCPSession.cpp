#include "TCPSession.h"

NS_NET_UV_BEGIN

TCPSession* TCPSession::createSession(SessionManager* sessionManager, TCPSocket* socket)
{
	TCPSession* session = (TCPSession*)fc_malloc(sizeof(TCPSession));
	new(session)TCPSession(sessionManager);
	
	if (session == NULL)
	{
		if (socket)
		{
			socket->~TCPSocket();
			fc_free(socket);
		}
		return NULL;
	}

	if (session->init(socket))
	{
		return session;
	}
	session->~TCPSession();
	fc_free(session);
	return NULL;
}

TCPSession::TCPSession(SessionManager* sessionManager)
	: Session(sessionManager)
	, m_socket(NULL)
{
	assert(sessionManager != NULL);
}

TCPSession::~TCPSession()
{
	if (m_socket)
	{
		m_socket->~TCPSocket();
		fc_free(m_socket);
		m_socket = NULL;
	}
}

bool TCPSession::init(TCPSocket* socket)
{
	assert(socket != NULL);
	if (socket == NULL)
	{
		return false;
	}

	m_socket = socket;
	m_socket->setRecvCallback(std::bind(&TCPSession::on_socket_recv, this, std::placeholders::_1, std::placeholders::_2));
	m_socket->setCloseCallback(std::bind(&TCPSession::on_socket_close, this, std::placeholders::_1));

	return true;
}

void TCPSession::executeSend(char* data, uint32_t len)
{
	if (data == NULL || len <= 0)
		return;

	if (isOnline())
	{
		if (!m_socket->send(data, len))
		{
			executeDisconnect();
		}
	}
	else
	{
		fc_free(data);
	}
}

void TCPSession::executeDisconnect()
{
	if (isOnline())
	{
		setIsOnline(false);
		m_socket->disconnect();
	}
}

bool TCPSession::executeConnect(const char* ip, uint32_t port)
{
	return m_socket->connect(ip, port);
}

void TCPSession::on_socket_recv(char* data, ssize_t len)
{
	if (!isOnline())
		return;

	char* buf = (char*)fc_malloc(sizeof(char) * len);
	memcpy(buf, data, len);
	m_sessionRecvCallback(this, buf, len);
}

void TCPSession::on_socket_close(Socket* socket)
{
	this->setIsOnline(false);
	if (m_sessionCloseCallback)
	{
		m_sessionCloseCallback(this);
	}
}

uint32_t TCPSession::getPort()
{
	return m_socket->getPort();
}

std::string TCPSession::getIp()
{
	return m_socket->getIp();
}

NS_NET_UV_END
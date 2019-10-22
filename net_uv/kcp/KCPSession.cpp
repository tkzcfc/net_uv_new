#include "KCPSession.h"
#include "KCPSocket.h"

NS_NET_UV_BEGIN

KCPSession* KCPSession::createSession(SessionManager* sessionManager, KCPSocket* socket)
{
	KCPSession* session = (KCPSession*)fc_malloc(sizeof(KCPSession));
	new(session)KCPSession(sessionManager);

	if (session == NULL)
	{
		if (socket)
		{
			socket->~KCPSocket();
			fc_free(socket);
		}
		return NULL;
	}

	if (session->init(socket))
	{
		return session;
	}
	session->~KCPSession();
	fc_free(session);
	return NULL;
}

KCPSession::KCPSession(SessionManager* sessionManager)
	: Session(sessionManager)
	, m_socket(NULL)
{
	assert(sessionManager != NULL);
}

KCPSession::~KCPSession()
{
	if (m_socket)
	{
		m_socket->~KCPSocket();
		fc_free(m_socket);
		m_socket = NULL;
	}
}

bool KCPSession::init(KCPSocket* socket)
{
	assert(socket != 0);
	if (socket == NULL)
	{
		return false;
	}

	m_socket = socket;
	m_socket->setCloseCallback(std::bind(&KCPSession::on_socket_close, this, std::placeholders::_1));
	m_socket->setRecvCallback(std::bind(&KCPSession::on_socket_recv, this, std::placeholders::_1, std::placeholders::_2));

	return true;
}

void KCPSession::executeSend(char* data, uint32_t len)
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
	fc_free(data);
}

void KCPSession::executeDisconnect()
{
	if (isOnline())
	{
		setIsOnline(false);
		m_socket->disconnect();
	}
}

bool KCPSession::executeConnect(const char* ip, uint32_t port)
{
	return m_socket->connect(ip, port);
}

void KCPSession::on_socket_recv(char* data, ssize_t len)
{
	if (!isOnline())
		return;
	char* buf = (char*)fc_malloc(sizeof(char) * len);
	memcpy(buf, data, len);
	m_sessionRecvCallback(this, buf, len);
}

void KCPSession::on_socket_close(Socket* socket)
{
	this->setIsOnline(false);
	if (m_sessionCloseCallback)
	{
		m_sessionCloseCallback(this);
	}
}

uint32_t KCPSession::getPort()
{
	return m_socket->getPort();
}

std::string KCPSession::getIp()
{
	return m_socket->getIp();
}

NS_NET_UV_END
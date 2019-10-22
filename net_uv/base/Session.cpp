#include "Session.h"
#include "SessionManager.h"

NS_NET_UV_BEGIN

Session::Session(SessionManager* manager)
	: m_sessionManager(manager)
	, m_isOnline(false)
	, m_sessionID(-1)
	, m_sessionRecvCallback(nullptr)
	, m_sessionCloseCallback(nullptr)
{}

Session::~Session()
{}

void Session::send(char* data, uint32_t len)
{
	getSessionManager()->send(this->m_sessionID, data, len);
}

void Session::disconnect()
{
	getSessionManager()->disconnect(this->m_sessionID);
}

void Session::setIsOnline(bool bIsOnline)
{
	m_isOnline = bIsOnline;
}
NS_NET_UV_END
#include "KCPSocketManager.h"
#include "KCPSocket.h"

NS_NET_UV_BEGIN

KCPSocketManager::KCPSocketManager(uv_loop_t* loop, KCPSocket* owner)
	: m_convSeed(10000)
	, m_owner(owner)
{}

KCPSocketManager::~KCPSocketManager()
{
	m_owner = NULL;
}

void KCPSocketManager::push(KCPSocket* socket)
{
	SOInfo info;
	info.socket = socket;
	info.isConnect = false;
	m_allSocket.push_back(info);
}

void KCPSocketManager::online(KCPSocket* socket)
{
	for (auto it = m_allSocket.begin(); it != m_allSocket.end(); ++it)
	{
		if (it->socket == socket)
		{
			it->isConnect = true;
			return;
		}
	}
}

void KCPSocketManager::remove(KCPSocket* socket)
{
	for (auto it = m_allSocket.begin(); it != m_allSocket.end(); ++it)
	{
		if (it->socket == socket)
		{
			if (it->isConnect == false)
			{
				socket->~KCPSocket();
				fc_free(socket);
			}
			m_allSocket.erase(it);
			return;
		}
	}
}

uint32_t KCPSocketManager::count()
{
	return (uint32_t)m_allSocket.size();
}

void KCPSocketManager::disconnectAll()
{
	auto tmp = m_allSocket;

	for (auto& it : tmp)
	{
		it.socket->disconnect();
	}
}

uint32_t KCPSocketManager::createConvID()
{
	return m_convSeed++;
}

KCPSocket* KCPSocketManager::getSocketBySockAddr(const struct sockaddr* addr)
{
	for (auto& it : m_allSocket)
	{
		if (net_compareAddress(it.socket->getSocketAddr(), addr))
		{
			return it.socket;
		}
	}
	return NULL;
}

NS_NET_UV_END
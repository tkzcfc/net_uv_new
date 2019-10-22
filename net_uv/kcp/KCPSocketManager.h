#pragma once

#include "KCPCommon.h"

NS_NET_UV_BEGIN

class KCPSocket;
class KCPSocketManager
{
public:
	KCPSocketManager(uv_loop_t* loop, KCPSocket* owner);

	virtual ~KCPSocketManager();

	void push(KCPSocket* socket);

	void online(KCPSocket* socket);

	void remove(KCPSocket* socket);

	uint32_t count();

	void disconnectAll();

	KCPSocket* getSocketBySockAddr(const struct sockaddr* addr);

	uint32_t createConvID();

	inline KCPSocket* getOwner();

private:

	struct SOInfo
	{
		KCPSocket* socket;
		bool isConnect;
	};
	std::vector<SOInfo> m_allSocket;

	uint32_t m_convSeed;

	KCPSocket* m_owner;
};

KCPSocket* KCPSocketManager::getOwner()
{
	return m_owner;
}

NS_NET_UV_END
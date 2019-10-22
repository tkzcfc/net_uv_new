#pragma once

#include "P2PPipe.h"

NS_NET_UV_BEGIN

#define PEER_LOCAL_ADDR_INFO_MAX_COUNT 3

class P2PTurn
{
public:

	P2PTurn();

	P2PTurn(const P2PTurn&) = delete;

	virtual ~P2PTurn();

	bool start(const char* ip, uint32_t port);

	void stop();

protected:
	void run();

	void onIdleRun();

	void onPipeRecvJsonCallback(P2PMessageID msgID, rapidjson::Document& document, uint64_t key, const struct sockaddr* addr);
	
	void onPipeRecvKcpCallback(char* data, uint32_t len, uint64_t key, const struct sockaddr* addr);
	
	void onPipeNewSessionCallback(uint64_t key);
	
	void onPipeNewKcpCreateCallback(uint64_t key);
	
	void onPipeRemoveSessionCallback(uint64_t key);
	
protected:
	P2PPipe m_pipe;

	enum TurnState
	{
		STOP,
		START,
		WILL_STOP,
	};
	TurnState m_state;

	UVLoop m_loop;
	UVIdle m_idle;
	Thread m_thread;

	struct PeerData
	{
		PeerData()
		{
			localAddrInfoCount = 0;
			addrInfo.key = 0;
		}
		LocNetAddrInfo localAddrInfoArr[PEER_LOCAL_ADDR_INFO_MAX_COUNT];
		uint32_t localAddrInfoCount;
		AddrInfo addrInfo;
	};
	
	std::map<uint64_t, PeerData> m_key_peerDataMap;
};

NS_NET_UV_END

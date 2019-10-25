#pragma once


#include "../base/Buffer.h"
#include "../base/md5.h"
#include <unordered_map>
#include <time.h>

NS_NET_UV_BEGIN


class NetMsgMgr;
using NetMgrSendBuffCallback = void(*)(NetMsgMgr*, uint32_t, char*, uint32_t len);
using NetMgrOnMsgCallback = void(*)(NetMsgMgr*, uint32_t, char*, uint32_t len);
using NetMgrCloseSocketCallback = void(*)(NetMsgMgr*, uint32_t);

class NetMsgMgr
{
public:
	NetMsgMgr();
	~NetMsgMgr();
	
	void onDisconnect(uint32_t sessionID);

	void onConnect(uint32_t sessionID);

	void onBuff(uint32_t sessionID, char* data, uint32_t len);

	void sendMsg(uint32_t sessionID, char* data, uint32_t len);

	void updateFrame();

	inline void setSendCallback(NetMgrSendBuffCallback call);

	inline void setOnMsgCallback(NetMgrOnMsgCallback call);

	inline void setCloseSctCallback(NetMgrCloseSocketCallback call);

	inline void setHeartBeatTime(uint32_t value);

	inline void setHeartBeatLoseMaxCount(uint32_t value);

	inline void setUserData(void* userdata);

	inline void* getUserData();

private:
	
	void createSessionInfo(uint32_t sessionID);

	void removeSessionInfo(uint32_t sessionID);

	void executeDisconnect(uint32_t sessionID);

private:

	struct SessionInfo
	{
		uint32_t loseCount;
		time_t lastRecvTime;
		Buffer* buf;
		uint32_t sessionID;
	};
	std::unordered_map<uint32_t, SessionInfo*> m_sessionInfoMap;
	std::vector<SessionInfo*>	m_sessionInfoVec;

	void*						m_pUserdata = 0;

	NetMgrSendBuffCallback		m_sendCall = nullptr;
	NetMgrOnMsgCallback			m_onMsgCall = nullptr;
	NetMgrCloseSocketCallback	m_closeSocketCall = nullptr;

	time_t					m_curUpdateTime;
	time_t					m_lastUpdateTime;

	uint32_t				m_heartBeatCheckTime;
	uint32_t				m_heartBeatLoseMaxCount;

	MD5						m_md5;
};


void NetMsgMgr::setSendCallback(NetMgrSendBuffCallback call)
{
	m_sendCall = call;
}

void NetMsgMgr::setOnMsgCallback(NetMgrOnMsgCallback call)
{
	m_onMsgCall = call;
}

void NetMsgMgr::setCloseSctCallback(NetMgrCloseSocketCallback call)
{
	m_closeSocketCall = call;
}

void NetMsgMgr::setHeartBeatTime(uint32_t value)
{
	m_heartBeatCheckTime = value;
}

void NetMsgMgr::setHeartBeatLoseMaxCount(uint32_t value)
{
	m_heartBeatLoseMaxCount = value;
}

void NetMsgMgr::setUserData(void* userdata) 
{
	m_pUserdata = userdata; 
}

void* NetMsgMgr::getUserData() 
{
	return m_pUserdata; 
}

NS_NET_UV_END

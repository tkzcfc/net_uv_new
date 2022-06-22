#include "NetMsgMgr.h"
#include <time.h>

NS_NET_UV_BEGIN

NetMsgMgr::NetMsgMgr()
	: m_heartBeatCheckTime(5)
	, m_heartBeatLoseMaxCount(4)
{
	m_curUpdateTime = time(NULL);
	m_lastUpdateTime = m_curUpdateTime;
}

NetMsgMgr::~NetMsgMgr()
{
	for (auto& it : m_sessionInfoVec)
	{
		it->buf->~Buffer();
		fc_free(it->buf);
		fc_free(it);

	}
	m_sessionInfoMap.clear();
	m_sessionInfoVec.clear();
}

void NetMsgMgr::onDisconnect(uint32_t sessionID)
{
	removeSessionInfo(sessionID);
}

void NetMsgMgr::onConnect(uint32_t sessionID)
{
	createSessionInfo(sessionID);
}

void NetMsgMgr::onBuff(uint32_t sessionID, char* data, uint32_t len)
{
	auto it = m_sessionInfoMap.find(sessionID);
	if (it == m_sessionInfoMap.end())
	{
		assert(0);
		return;
	}

	it->second->lastRecvTime = m_curUpdateTime;
	it->second->loseCount = 0;

	auto recvBuf = it->second->buf;
	recvBuf->add((char*)data, len);

	while (recvBuf->getDataLength() >= sizeof(NetMsgHead))
	{
		NetMsgHead* h = (NetMsgHead*)recvBuf->getHeadBlockData();

		switch (h->type)
		{
		case NetMsgHead::PING:
		{
			NetMsgHead msg;
			msg.len = 0;
			msg.type = NetMsgHead::PONG;
			m_sendCall(this, sessionID, (char*)&msg, sizeof(msg));
			recvBuf->pop(NULL, sizeof(NetMsgHead));
		}break;
		case NetMsgHead::PONG:
		{
			recvBuf->pop(NULL, sizeof(NetMsgHead));
		}break;
		case NetMsgHead::MSG:
		{
			if (h->len > NET_UV_MAX_MSG_LEN || h->len < sizeof(uint32_t))
			{
#if OPEN_NET_UV_DEBUG == 1
				char* pMsg = (char*)fc_malloc(recvBuf->getDataLength());
				recvBuf->get(pMsg);
				std::string errdata(pMsg, recvBuf->getDataLength());
				fc_free(pMsg);
				NET_UV_LOG(NET_UV_L_WARNING, errdata.c_str());
#endif
				NET_UV_LOG(NET_UV_L_WARNING, "Illegal data received");
				recvBuf->clear();
				executeDisconnect(sessionID);
				return;
			}

			int32_t subv = recvBuf->getDataLength() - (h->len + sizeof(NetMsgHead));
			//消息接收完成
			if (subv >= 0)
			{
				char* pMsg = (char*)fc_malloc(recvBuf->getDataLength());
				if (pMsg == NULL)
				{
					recvBuf->clear();
					executeDisconnect(sessionID);
					return;
				}
				recvBuf->get(pMsg);

				char* src = pMsg + sizeof(NetMsgHead);
				uint32_t msgId = *((uint32_t*)src);

				m_onMsgCall(this, sessionID, msgId, src + sizeof(uint32_t), h->len - sizeof(uint32_t));
				recvBuf->clear();

				if (subv > 0)
				{
					recvBuf->add(data + (len - subv), subv);
				}
				fc_free(pMsg);
			}
			else
			{
				return;
			}
		}break;
		default:
		{
#if OPEN_NET_UV_DEBUG == 1
			char* pMsg = (char*)fc_malloc(recvBuf->getDataLength());
			recvBuf->get(pMsg);
			std::string errdata(pMsg, recvBuf->getDataLength());
			fc_free(pMsg);
			NET_UV_LOG(NET_UV_L_WARNING, errdata.c_str());
#endif
			recvBuf->clear();
			executeDisconnect(sessionID);
			break;
		}
		}
	}
}


void  NetMsgMgr::executeDisconnect(uint32_t sessionID)
{
	m_closeSocketCall(this, sessionID);
}

void NetMsgMgr::sendMsg(uint32_t sessionID, uint32_t msgID, char* data, uint32_t len)
{
	auto it = m_sessionInfoMap.find(sessionID);
	if (it == m_sessionInfoMap.end())
	{
		return;
	}

	assert(len >= 0);

	if (len > NET_UV_MAX_MSG_LEN)
	{
#if defined (WIN32) || defined(_WIN32)
		MessageBox(NULL, TEXT("Message exceeds maximum length limit"), TEXT("ERROR"), MB_OK);
#else
		printf("Message exceeds maximum length limit");
#endif
		assert(0);
		return;
	}

	uint32_t totalLen = sizeof(NetMsgHead) + sizeof(uint32_t) + len;
	auto offset = sizeof(NetMsgHead);
	
	char* p = (char*)fc_malloc(totalLen);

	NetMsgHead* h = (NetMsgHead*)p;
	h->len = sizeof(uint32_t) + len;
	h->type = NetMsgHead::NetMsgType::MSG;

	// message id
	*(uint32_t*)(p + offset) = msgID;
	offset += sizeof(uint32_t);

	// data
	if (len > 0) 
	{
		memcpy(p + offset, data, len);
	}

	// 消息分片
	if (totalLen > NET_UV_WRITE_MAX_LEN)
	{
		int32_t curIndex = 0;
		int32_t curArrIndex = 0;

		while (totalLen > 0)
		{
			if (totalLen < NET_UV_WRITE_MAX_LEN)
			{
				char* tmp = p + curIndex;
				m_sendCall(this, sessionID, tmp, totalLen);

				curArrIndex++;
				curIndex = curIndex + totalLen;
				totalLen = 0;
			}
			else
			{
				char* tmp = p + curIndex;
				m_sendCall(this, sessionID, tmp, NET_UV_WRITE_MAX_LEN);

				curArrIndex++;
				totalLen = totalLen - NET_UV_WRITE_MAX_LEN;
				curIndex = curIndex + NET_UV_WRITE_MAX_LEN;
			}
		}
		fc_free(p);
	}
	else
	{
		m_sendCall(this, sessionID, p, totalLen);
		fc_free(p);
	}
}

void NetMsgMgr::updateFrame()
{
	if (m_heartBeatCheckTime <= 0)
	{
		return;
	}

	m_curUpdateTime = time(NULL);
	if (m_lastUpdateTime - m_curUpdateTime < 1)
	{
		return;
	}

	m_lastUpdateTime = m_curUpdateTime;

	for (auto &it : m_sessionInfoVec)
	{
		if (m_curUpdateTime - it->lastRecvTime > m_heartBeatCheckTime)
		{
			it->loseCount++;

			if (it->loseCount > m_heartBeatLoseMaxCount)
			{
				it->buf->clear();
				executeDisconnect(it->sessionID);
			}
			else
			{
				NetMsgHead msg;
				msg.len = 0;
				msg.type = NetMsgHead::PING;
				m_sendCall(this, it->sessionID, (char*)&msg, sizeof(NetMsgHead));
			}
		}
	}
}

void NetMsgMgr::createSessionInfo(uint32_t sessionID)
{
	if (m_sessionInfoMap.find(sessionID) != m_sessionInfoMap.end())
	{
		assert(0);
		return;
	}
	SessionInfo* info = (SessionInfo*)(fc_malloc(sizeof(SessionInfo)));

	info->sessionID = sessionID;
	info->lastRecvTime = m_curUpdateTime;
	info->loseCount = 0;
	info->buf = (Buffer*)fc_malloc(sizeof(Buffer));
	new(info->buf) Buffer(NET_UV_WRITE_MAX_LEN * 10);

	m_sessionInfoMap.insert(std::make_pair(sessionID, info));
	m_sessionInfoVec.push_back(info);
}

void NetMsgMgr::removeSessionInfo(uint32_t sessionID)
{
	auto it = m_sessionInfoMap.find(sessionID);
	if (it != m_sessionInfoMap.end())
	{
		it->second->buf->~Buffer();
		fc_free(it->second->buf);
		fc_free(it->second);

		m_sessionInfoVec.erase(std::find(m_sessionInfoVec.begin(), m_sessionInfoVec.end(), it->second));
		m_sessionInfoMap.erase(it);
	}
}

NS_NET_UV_END

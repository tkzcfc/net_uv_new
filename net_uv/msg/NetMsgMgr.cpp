#include "NetMsgMgr.h"
#include "NetMsgUtils.h"
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

	while (recvBuf->getDataLength() >= NetMsgHeadLen)
	{
		NetMsgHead* h = (NetMsgHead*)recvBuf->getHeadBlockData();

		switch (h->type)
		{
		case NetMsgHead::PING:
		{
			auto pMsg = (NetMsgHead*)fc_malloc(NetMsgHeadLen);
			pMsg->len = 0;
			pMsg->type = NetMsgHead::PONG;
			m_sendCall(this, sessionID, (char*)pMsg, NetMsgHeadLen);
			fc_free(pMsg);
			recvBuf->pop(NULL, sizeof(NetMsgHead));
		}break;
		case NetMsgHead::PONG:
		{
			recvBuf->pop(NULL, sizeof(NetMsgHead));
		}break;
		case NetMsgHead::MSG:
		{
			//长度大于最大包长或长度小于等于零，不合法客户端
			if (h->len > NET_UV_MAX_MSG_LEN || h->len <= 0)
			{
#if OPEN_NET_UV_DEBUG == 1
				char* pMsg = (char*)fc_malloc(recvBuf->getDataLength());
				recvBuf->get(pMsg);
				std::string errdata(pMsg, recvBuf->getDataLength());
				fc_free(pMsg);
				NET_UV_LOG(NET_UV_L_WARNING, errdata.c_str());
#endif
				NET_UV_LOG(NET_UV_L_WARNING, "数据不合法 (1)!!!!");
				recvBuf->clear();
				executeDisconnect(sessionID);
				return;
			}

			int32_t subv = recvBuf->getDataLength() - (h->len + NetMsgHeadLen);
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

				char* src = pMsg + NetMsgHeadLen;

#if UV_ENABLE_DATA_CHECK == 1
				uint32_t recvLen = 0;
				char* recvData = net_uv_decode(src, h->len, recvLen);

				if (recvData != NULL && recvLen > 0)
				{
					m_onMsgCall(this, sessionID, h->id, recvData, recvLen);
					fc_free(recvData);
				}
				else//数据不合法
				{
					NET_UV_LOG(NET_UV_L_WARNING, "数据不合法 (3)!!!!");
#if OPEN_NET_UV_DEBUG == 1
					std::string errdata(pMsg, recvBuf->getDataLength());
					NET_UV_LOG(NET_UV_L_WARNING, errdata.c_str());
#endif
					recvBuf->clear();
					fc_free(pMsg);
					executeDisconnect(sessionID);
					return;
				}
#else
				m_onMsgCall(this, sessionID, src, h->len);
#endif
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

	if (len > NET_UV_MAX_MSG_LEN)
	{
#if defined (WIN32) || defined(_WIN32)
		MessageBox(NULL, TEXT("消息超过最大限制"), TEXT("错误"), MB_OK);
#else
		printf("消息超过最大限制");
#endif
		assert(0);
		return;
	}

#if UV_ENABLE_DATA_CHECK == 1
	uint32_t encodelen = 0;
	char* p = net_uv_encode(data, len, NetMsgHeadLen, encodelen);
	if (p == NULL)
	{
		return;
	}
	NetMsgHead* h = (NetMsgHead*)p;
	h->len = encodelen;
	h->type = NetMsgHead::NetMsgType::MSG;
	h->id = msgID;

	uint32_t sendlen = NetMsgHeadLen + encodelen;

#else
	uint32_t sendlen = NetMsgHeadLen + len;
	char* p = (char*)fc_malloc(sendlen);
	NetMsgHead* h = (NetMsgHead*)p;
	h->len = len;
	h->type = NetMsgHead::NetMsgType::MSG;

	memcpy(p + NetMsgHeadLen, data, len);
#endif

	// 消息分片
	if (sendlen > NET_UV_WRITE_MAX_LEN)
	{
		int32_t curIndex = 0;
		int32_t curArrIndex = 0;

		while (sendlen > 0)
		{
			if (sendlen < NET_UV_WRITE_MAX_LEN)
			{
				char* tmp = p + curIndex;
				m_sendCall(this, sessionID, tmp, sendlen);

				curArrIndex++;
				curIndex = curIndex + sendlen;
				sendlen = 0;
			}
			else
			{
				char* tmp = p + curIndex;
				m_sendCall(this, sessionID, tmp, NET_UV_WRITE_MAX_LEN);

				curArrIndex++;
				sendlen = sendlen - NET_UV_WRITE_MAX_LEN;
				curIndex = curIndex + NET_UV_WRITE_MAX_LEN;
			}
		}
		fc_free(p);
	}
	else
	{
		m_sendCall(this, sessionID, p, sendlen);
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
				auto pMsg = (NetMsgHead*)fc_malloc(NetMsgHeadLen);
				pMsg->len = 0;
				pMsg->type = NetMsgHead::PING;
				m_sendCall(this, it->sessionID, (char*)pMsg, NetMsgHeadLen);
				fc_free(pMsg);
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

#include "P2PPipe.h"

NS_NET_UV_BEGIN

#define P2P_KCP_MAX_RECV (1024 * 2)

P2PPipe::P2PPipe()
	: m_socket(NULL)
	, m_updateInterval(0U)
	, m_updateTime(0U)
	, m_recvJsonCallback(nullptr)
	, m_recvKcpCallback(nullptr)
	, m_newSessionCallback(nullptr)
	, m_newKcpCreateCallback(nullptr)
	, m_removeSessionCallback(nullptr)
{
	m_recvBuf = (char*)fc_malloc(P2P_KCP_MAX_RECV);
}

P2PPipe::~P2PPipe()
{
	shutdownSocket();
	fc_free(m_recvBuf);
}

bool P2PPipe::bind(const char* bindIP, uint32_t binPort, uv_loop_t* loop)
{
	assert(m_recvJsonCallback != nullptr);
	assert(m_recvKcpCallback != nullptr);
	assert(m_newSessionCallback != nullptr);
	assert(m_newKcpCreateCallback != nullptr);
	assert(m_removeSessionCallback != nullptr);

	if (m_socket)
	{
		return false;
	}

	m_socket = (UDPSocket*)fc_malloc(sizeof(UDPSocket));
	new (m_socket)UDPSocket(loop);

	m_socket->setReadCallback(std::bind(&P2PPipe::on_udp_read, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));

	if (!m_socket->bind(bindIP, binPort) || !m_socket->listen(0))
	{
		shutdownSocket();
		return false;
	}

	NET_UV_LOG(NET_UV_L_INFO, "bind [%u]", m_socket->getPort());

	return true;
}

bool P2PPipe::kcpSend(char* data, uint32_t len, uint32_t toIP, uint32_t toPort)
{
	AddrInfo info;
	info.ip = toIP;
	info.port = toPort;
	return kcpSend(data, len, info.key);
}

bool P2PPipe::kcpSend(char* data, uint32_t len, uint64_t key)
{
	auto it = m_allSessionDataMap.find(key);
	if (it != m_allSessionDataMap.end() && it->second.kcp)
	{
		return (ikcp_send(it->second.kcp, data, len) == 0);
	}
	return false;
}

void P2PPipe::send(P2PMessageID msgID, const char* data, int32_t len, uint32_t toIP, uint32_t toPort)
{
#if OPEN_NET_UV_DEBUG
	if (msgID != P2P_MSG_ID_PING && msgID != P2P_MSG_ID_PONG && msgID != P2P_MSG_ID_KCP)
	{
		std::string logstr(data, len);
		NET_UV_LOG(NET_UV_L_INFO, "send to [%u]:[%u]  [%d]%s", toIP, toPort, msgID, logstr.c_str());
	}
#endif

	const int32_t alloc_len = sizeof(P2PMessage) + len;

	char* pData = (char*)fc_malloc(alloc_len);

	P2PMessage* msg = (P2PMessage*)pData;
	msg->msgID = msgID;
	msg->msgLen = len;
	msg->uniqueID = 0;

	memcpy((char*)&msg[1], data, len);
	
	AddrInfo info;
	info.ip = toIP;
	info.port = toPort;

	auto it = m_allSessionDataMap.find(info.key);
	if (it == m_allSessionDataMap.end())
	{
		char * ipaddr = NULL;
		char addr[32] = {0};
		in_addr inaddr;
		inaddr.s_addr = toIP;
		ipaddr = inet_ntoa(inaddr);
		strcpy(addr, ipaddr);

		sockaddr_in send_addr;
		uv_ip4_addr(addr, toPort, &send_addr);

		m_socket->udpSend((const char*)msg, alloc_len, (const struct sockaddr*)&send_addr);
	}
	else
	{
		m_socket->udpSend((const char*)msg, alloc_len, (const struct sockaddr*)&it->second.send_addr);
	}
	fc_free(pData);
}

void P2PPipe::send(P2PMessageID msgID, const char* data, int32_t len, const struct sockaddr* addr)
{
#if OPEN_NET_UV_DEBUG
	if (msgID != P2P_MSG_ID_PING && msgID != P2P_MSG_ID_PONG && msgID != P2P_MSG_ID_KCP)
	{
		std::string logstr(data, len);
		NET_UV_LOG(NET_UV_L_INFO, "send [%d]%s", msgID, logstr.c_str());
	}
#endif

	const int32_t alloc_len = sizeof(P2PMessage) + len;

	char* pData = (char*)fc_malloc(alloc_len);

	P2PMessage* msg = (P2PMessage*)pData;
	msg->msgID = msgID;
	msg->msgLen = len;
	msg->uniqueID = 0;

	memcpy((char*)&msg[1], data, len);

	m_socket->udpSend((const char*)msg, alloc_len, addr);

	fc_free(pData);
}

void P2PPipe::disconnect(uint64_t key)
{
	if (isContain(key))
	{
		AddrInfo info;
		info.key = key;
		send(P2PMessageID::P2P_MSG_ID_C2C_DISCONNECT, P2P_NULL_JSON, P2P_NULL_JSON_LEN, info.ip, info.port);
	}
}

void P2PPipe::update(uint32_t updateTime)
{
	uint32_t interval = updateTime - m_updateTime;
	m_updateTime = updateTime;

	heartCheck(interval);

	for (auto & it : m_allSessionDataMap)
	{
		if (it.second.kcp)
		{
			ikcp_update(it.second.kcp, updateTime);
		}
	}
}

void P2PPipe::heartCheck(uint32_t interval)
{
	m_updateInterval += interval;
	if (m_updateInterval < 1000)
	{
		return;
	}
	m_updateInterval = 0;

	if (m_allSessionDataMap.empty())
	{
		return;
	}

	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();
	writer.Key("time");
	writer.Uint(m_updateTime);
	writer.EndObject();

	for (auto it = m_allSessionDataMap.begin(); it != m_allSessionDataMap.end(); )
	{
		if (it->second.isStartCheck)
		{
			if (it->second.noResponseCount > 5)
			{
				onSessionRemove(it);
				it = m_allSessionDataMap.erase(it);
			}
			else
			{
				if (m_updateTime - it->second.lastCheckTime >= 1000)
				{
					it->second.noResponseCount++;
					send(P2PMessageID::P2P_MSG_ID_PING, s.GetString(), s.GetLength(), (const struct sockaddr*)&it->second.send_addr);
				}
				it++;
			}
		}
		else
		{
			it++;
		}
	}
}

void P2PPipe::shutdownSocket()
{
	if (m_socket)
	{
		m_socket->~UDPSocket();
		fc_free(m_socket);
		m_socket = NULL;
	}

	for (auto it = m_allSessionDataMap.begin(); it != m_allSessionDataMap.end(); ++it)
	{
		onSessionRemove(it);
	}
	m_allSessionDataMap.clear();
}

bool P2PPipe::isContain(uint64_t key)
{
	return (m_allSessionDataMap.find(key) != m_allSessionDataMap.end());
}

void P2PPipe::close()
{
	for (auto& it : m_allSessionDataMap)
	{
		send(P2PMessageID::P2P_MSG_ID_C2C_DISCONNECT, P2P_NULL_JSON, P2P_NULL_JSON_LEN, (const struct sockaddr*)&it.second.send_addr);
	}
	shutdownSocket();
}

void P2PPipe::on_udp_read(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{
	// 不支持IPV6
	if (addr->sa_family != AF_INET)
		return;

	// 长度校验失败
	if (nread < sizeof(P2PMessage))
		return;

	P2PMessage* msg = (P2PMessage*)buf->base;

	// 长度校验失败
	if (msg->msgLen != nread - sizeof(P2PMessage))
	{
		return;
	}

	// 消息ID校验失败
	if (msg->msgID <= P2PMessageID::P2P_MSG_ID_BEGIN || P2PMessageID::P2P_MSG_ID_END <= msg->msgID)
		return;

	const struct sockaddr_in* recv_addr = (const struct sockaddr_in*)addr;

	AddrInfo info;
	info.ip = recv_addr->sin_addr.s_addr;
	info.port = ntohs(recv_addr->sin_port);
	
	// 合法消息
	char* data = buf->base + sizeof(P2PMessage);

#if OPEN_NET_UV_DEBUG
	if (msg->msgID != P2PMessageID::P2P_MSG_ID_PING && msg->msgID != P2PMessageID::P2P_MSG_ID_PONG)
	{
		std::string recv_str(data, msg->msgLen);
		NET_UV_LOG(NET_UV_L_INFO, "recv[%u]:[%u] : [%d]%s", info.ip, info.port, msg->msgID, recv_str.c_str());
	}
#endif

	recvData(info.key, addr);

	// ping
	if (msg->msgID == P2PMessageID::P2P_MSG_ID_PING)
	{
		this->send(P2PMessageID::P2P_MSG_ID_PONG, data, msg->msgLen, addr);
		return;
	}

	// kcp格式数据
	if (msg->msgID == P2PMessageID::P2P_MSG_ID_KCP)
	{
		on_recv_kcpMsg(info.key, data, msg->msgLen, addr);
		return;
	}

	// json格式数据
	if (P2PMessageID::P2P_MSG_ID_JSON_BEGIN < msg->msgID && msg->msgID < P2PMessageID::P2P_MSG_ID_JSON_END)
	{
		rapidjson::Document document;
		document.Parse(data, msg->msgLen);

		if (!document.HasParseError())
		{
			// pong
			if (msg->msgID == P2PMessageID::P2P_MSG_ID_PONG)
			{
				on_recv_pong(info.key, document, addr);
			}
			// kcp create
			else if (msg->msgID == P2PMessageID::P2P_MSG_ID_CREATE_KCP)
			{
				on_recv_createKcp(info.key, document, addr);
			}
			// kcp create result
			else if (msg->msgID == P2PMessageID::P2P_MSG_ID_CREATE_KCP_RESULT)
			{
				on_recv_createKcpResult(info.key, document, addr);
			}
			// disconnect
			else if (msg->msgID == P2PMessageID::P2P_MSG_ID_C2C_DISCONNECT)
			{
				on_recv_disconnect(info.key, document, addr);
			}
			else
			{
				m_recvJsonCallback((P2PMessageID)msg->msgID, document, info.key, addr);
			}
		}
		else
		{
			NET_UV_LOG(NET_UV_L_ERROR, "json parse error: %u", document.GetParseError());
		}
	}
}

void P2PPipe::on_recv_kcpMsg(uint64_t key, char* data, uint32_t len, const struct sockaddr* addr)
{
	auto it = m_allSessionDataMap.find(key);

	if (it == m_allSessionDataMap.end())
	{
		return;
	}

	auto kcp = it->second.kcp;
	if (kcp == NULL)
	{
		return;
	}

	ikcp_input(kcp, data, len);

	int32_t kcp_recvd_bytes = 0;
	do
	{
		kcp_recvd_bytes = ikcp_recv(kcp, m_recvBuf, P2P_KCP_MAX_RECV);

		if (kcp_recvd_bytes < 0)
		{
			kcp_recvd_bytes = 0;
		}
		else
		{
			m_recvKcpCallback(m_recvBuf, kcp_recvd_bytes, key, addr);
		}
	} while (kcp_recvd_bytes > 0);
}

void P2PPipe::on_recv_pong(uint64_t key, rapidjson::Document& document, const struct sockaddr* addr)
{
	auto it = m_allSessionDataMap.find(key);
	if (it != m_allSessionDataMap.end())
	{
		if (document.HasMember("time"))
		{
			rapidjson::Value& time_value = document["time"];
			if (time_value.IsUint())
			{
				int32_t sub = m_updateTime - time_value.GetUint();
				it->second.delayTime = sub >= 0 ? sub : -sub;
			}
		}
	}
}

void P2PPipe::on_recv_createKcp(uint64_t key, rapidjson::Document& document, const struct sockaddr* addr)
{
	createKcp(key, 0xFFFF, 0);
	this->send(P2PMessageID::P2P_MSG_ID_CREATE_KCP_RESULT, P2P_NULL_JSON, P2P_NULL_JSON_LEN, addr);
}

void P2PPipe::on_recv_createKcpResult(uint64_t key, rapidjson::Document& document, const struct sockaddr* addr)
{
	createKcp(key, 0xFFFF, 1);
}

void P2PPipe::on_recv_disconnect(uint64_t key, rapidjson::Document& document, const struct sockaddr* addr)
{
	auto it = m_allSessionDataMap.find(key);
	if (it != m_allSessionDataMap.end())
	{
		onSessionRemove(it);
		m_allSessionDataMap.erase(it);
	}
}

void P2PPipe::createKcp(uint64_t key, uint32_t conv, uint32_t tag)
{
	auto it = m_allSessionDataMap.find(key);
	if (it != m_allSessionDataMap.end() && it->second.kcp == NULL)
	{
		ikcpcb* kcp = ikcp_create(conv, &it->second);
		kcp->output = P2PPipe::udp_output;

		ikcp_wndsize(kcp, 128, 128);

		// 启动快速模式
		// 第二个参数 nodelay-启用以后若干常规加速将启动
		// 第三个参数 interval为内部处理时钟，默认设置为 10ms
		// 第四个参数 resend为快速重传指标，设置为2
		// 第五个参数 为是否禁用常规流控，这里禁止
		ikcp_nodelay(kcp, 1, 10, 2, 1);
		//ikcp_nodelay(m_kcp, 1, 5, 1, 1); // 设置成1次ACK跨越直接重传, 这样反应速度会更快. 内部时钟5毫秒.

		it->second.kcp = kcp;

		it->second.isStartCheck = true;

		m_newKcpCreateCallback(key, tag);
	}
}

void P2PPipe::recvData(uint64_t key, const struct sockaddr* addr)
{
	auto it = m_allSessionDataMap.find(key);
	if (it == m_allSessionDataMap.end())
	{
		SessionData sessionData;
		sessionData.lastCheckTime = m_updateTime;
		sessionData.noResponseCount = 0;
		sessionData.isStartCheck = true;
		sessionData.delayTime = 0;
		sessionData.pipe = this;
		sessionData.kcp = NULL;
		memcpy(&sessionData.send_addr, addr, sizeof(sessionData.send_addr));

		m_allSessionDataMap.insert(std::make_pair(key, sessionData));

		m_newSessionCallback(key);
	}
	else
	{
		it->second.lastCheckTime = m_updateTime;
		it->second.noResponseCount = 0;
	}
}

void P2PPipe::onSessionRemove(std::map<uint64_t, SessionData>::iterator& it)
{
	if (it->second.kcp)
	{
		ikcp_release(it->second.kcp);
		it->second.kcp = NULL;
	}
	m_removeSessionCallback(it->first);
}

int32_t P2PPipe::udp_output(const char *buf, int32_t len, ikcpcb *kcp, void *user)
{
	SessionData* sessionData = (SessionData*)user;
	sessionData->pipe->send(P2PMessageID::P2P_MSG_ID_KCP, buf, len, (const sockaddr*)&sessionData->send_addr);
	return 0;
}

NS_NET_UV_END

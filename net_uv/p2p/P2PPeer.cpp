#include "P2PPeer.h"

NS_NET_UV_BEGIN


enum P2POperationCMD
{
	/// input
	P2P_CONNECT_TO_PEER,
	P2P_CONNECT_TO_TURN,
	P2P_SEND_TO_PEER,
	P2P_DISCONNECT_TO_PEER,

	/// output
	P2P_START_FAIL,
	P2P_START_SUC,
	P2P_CONNECT_TURN_SUC,
	P2P_CONNECT_TURN_FAIL,
	P2P_DISCONNECT_TURN,
	P2P_CONNECT_PEER_SUC,
	P2P_CONNECT_PEER_TIMEOUT,
	P2P_CONNECT_PEER_FAIL_NOT_FOUND,
	P2P_CONNECT_PEER_FAIL_CLI_SESSION,
	P2P_DISCONNECT_PEER,
	P2P_RECV_KCP_DATA,
	P2P_NEWCONNECT,
	P2P_STOP,
};

P2PPeer::P2PPeer()
	: m_state(PeerState::STOP)
	, m_turnPort(0)
	, m_tryConnectTurnCount(0)
	, m_isConnectTurn(false)
	, m_isStopConnectToTurn(true)
	, m_startCallback(nullptr)
	, m_newConnectCallback(nullptr)
	, m_connectToPeerCallback(nullptr)
	, m_connectToTurnCallback(nullptr)
	, m_disConnectToPeerCallback(nullptr)
	, m_disConnectToTurnCallback(nullptr)
	, m_recvCallback(nullptr)
{
	m_pipe.setRecvJsonCallback(std::bind(&P2PPeer::onPipeRecvJsonCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	m_pipe.setRecvKcpCallback(std::bind(&P2PPeer::onPipeRecvKcpCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	m_pipe.setNewSessionCallback(std::bind(&P2PPeer::onPipeNewSessionCallback, this, std::placeholders::_1));
	m_pipe.setNewKcpCreateCallback(std::bind(&P2PPeer::onPipeNewKcpCreateCallback, this, std::placeholders::_1, std::placeholders::_2));
	m_pipe.setRemoveSessionCallback(std::bind(&P2PPeer::onPipeRemoveSessionCallback, this, std::placeholders::_1));
}

P2PPeer::~P2PPeer()
{
	stop();
	m_thread.join();
}

bool P2PPeer::start(const char* ip, uint32_t port)
{
	if (m_state != PeerState::STOP)
	{
		return false;
	}

	assert(m_startCallback != nullptr);
	assert(m_newConnectCallback != nullptr);
	assert(m_connectToPeerCallback != nullptr);
	assert(m_connectToTurnCallback != nullptr);
	assert(m_disConnectToPeerCallback != nullptr);
	assert(m_disConnectToTurnCallback != nullptr);
	assert(m_recvCallback != nullptr);
	assert(m_closeCallback != nullptr);

	if (m_pipe.bind("0.0.0.0", 0, m_loop.ptr()) == false)
	{
		startFailureLogic();
		return false;
	}
	m_turnIP = ip;
	m_turnPort = port;
	m_state = PeerState::START;

	restartConnectTurn();

	m_thread.create([](void* arg) 
	{
		P2PPeer* self = (P2PPeer*)arg;
		self->run();
	}, this);
	return true;
}

void P2PPeer::stop()
{
	if (m_state != PeerState::START)
	{
		return;
	}
	m_state = PeerState::WILL_STOP;
}

void P2PPeer::restartConnectTurn()
{
	if (m_state != PeerState::START)
	{
		return;
	}
	pushInputOperation(0, P2POperationCMD::P2P_CONNECT_TO_TURN, NULL, 0);
}

bool P2PPeer::connectToPeer(uint64_t key)
{
	if (m_state != PeerState::START)
	{
		return false;
	}

	if (key == m_selfAddrInfo.key)
	{
		return false;
	}

	pushInputOperation(key, P2POperationCMD::P2P_CONNECT_TO_PEER, NULL, 0);
	return true;
}

void P2PPeer::updateFrame()
{
	if (m_outputLock.trylock() != 0)
	{
		return;
	}

	if (m_outputQue.empty())
	{
		m_outputLock.unlock();
		return;
	}

	while (!m_outputQue.empty())
	{
		m_outputQueCache.push(m_outputQue.front());
		m_outputQue.pop();
	}

	m_outputLock.unlock();
	bool isStopCMD = false;;

	while (!m_outputQueCache.empty())
	{
		auto& opData = m_outputQueCache.front();
		switch (opData.what)
		{
		case P2POperationCMD::P2P_START_FAIL:
		{
			m_startCallback(false);
		}break;
		case P2POperationCMD::P2P_START_SUC:
		{
			m_startCallback(true);
		}break;
		case P2POperationCMD::P2P_CONNECT_TURN_SUC:
		{
			m_connectToTurnCallback(true, m_selfAddrInfo.key);
		}break;
		case P2POperationCMD::P2P_CONNECT_TURN_FAIL:
		{
			m_connectToTurnCallback(false, 0);
		}break;
		case P2POperationCMD::P2P_DISCONNECT_TURN:
		{
			m_disConnectToTurnCallback();
		}break;
		case P2POperationCMD::P2P_CONNECT_PEER_FAIL_NOT_FOUND:
		{
			m_connectToPeerCallback(opData.key, 0);
		}break;
		case P2POperationCMD::P2P_CONNECT_PEER_SUC:
		{
			m_connectToPeerCallback(opData.key, 1);
		}break;
		case P2POperationCMD::P2P_CONNECT_PEER_TIMEOUT:
		{
			m_connectToPeerCallback(opData.key, 2);
		}break;
		case P2POperationCMD::P2P_CONNECT_PEER_FAIL_CLI_SESSION:
		{
			m_connectToPeerCallback(opData.key, 3);
		}break;
		case P2POperationCMD::P2P_DISCONNECT_PEER:
		{
			m_disConnectToPeerCallback(opData.key);
		}break;
		case P2POperationCMD::P2P_RECV_KCP_DATA:
		{
			m_recvCallback(opData.key, (char*)opData.data, opData.datalen);
			fc_free(opData.data);
		}break;
		case P2POperationCMD::P2P_NEWCONNECT:
		{
			m_newConnectCallback(opData.key);
		}break;
		case P2POperationCMD::P2P_STOP:
		{
			isStopCMD = true;
		}break;
		default:
			break;
		}

		m_outputQueCache.pop();
	}
	if (isStopCMD)
	{
		m_closeCallback();
	}
}

void P2PPeer::send(uint64_t key, char* data, uint32_t len)
{
	char* sendData = (char*)fc_malloc(len);
	memcpy(sendData, data, len);
	pushInputOperation(key, P2POperationCMD::P2P_SEND_TO_PEER, sendData, len);
}

void P2PPeer::disconnect(uint64_t key)
{
	pushInputOperation(key, P2POperationCMD::P2P_DISCONNECT_TO_PEER, NULL, 0);
}

/// Runnable
void P2PPeer::run()
{
	struct sockaddr* turnAddr = net_getsocketAddr(m_turnIP.c_str(), m_turnPort, NULL);
	// 域名解析失败或解析出来为IPV6
	if (turnAddr == NULL || turnAddr->sa_family != AF_INET)
	{
		startFailureLogic();
		pushOutputOperation(0, P2POperationCMD::P2P_START_FAIL, NULL, 0);
		return;
	}
	pushOutputOperation(0, P2POperationCMD::P2P_START_SUC, NULL, 0);

	struct sockaddr_in* addr4 = (struct sockaddr_in*)turnAddr;

	m_turnAddrInfo.ip = addr4->sin_addr.s_addr;
	m_turnAddrInfo.port = ntohs(addr4->sin_port);

	fc_free(turnAddr);

	m_idle.start(m_loop.ptr(), [](uv_timer_t* handle)
	{
		((P2PPeer*)handle->data)->onIdleRun();
	}, 1, 1, this);

	m_timer.start(m_loop.ptr(), [](uv_timer_t* handle)
	{
		((P2PPeer*)handle->data)->onTimerRun();
	}, 200, 200, this);

	m_loop.run(UV_RUN_DEFAULT);
	m_loop.close();

	m_state = PeerState::STOP;
	clearData();

	pushOutputOperation(0, P2POperationCMD::P2P_STOP, NULL, 0);
}

void P2PPeer::onIdleRun()
{
	runInputOperation();

	m_pipe.update((IUINT32)(uv_now(m_loop.ptr()) & 0xfffffffful));

	if (m_state == PeerState::WILL_STOP)
	{
		m_pipe.close();
		m_timer.stop();
		m_idle.stop();
	}
}

void P2PPeer::onTimerRun()
{
	doConnectToTurn();

	if (m_burrowManager.empty() == false)
	{
		for (auto it = m_burrowManager.begin(); it != m_burrowManager.end(); )
		{
			// 打洞数据发送次数超过20次，则停止发送
			if (it->second.sendCount >= 20)
			{
				it = m_burrowManager.erase(it);
			}
			else
			{
				m_pipe.send(P2PMessageID::P2P_MSG_ID_C2C_HELLO, it->second.sendData.c_str(), it->second.sendData.size(), (const struct sockaddr*)&it->second.targetAddr);
				it->second.sendCount++;
				it++;
			}
		}
	}

	if (m_connectToPeerSessionMng.empty() == false)
	{
		for (auto it = m_connectToPeerSessionMng.begin(); it != m_connectToPeerSessionMng.end(); )
		{
			if (it->second.state != SessionState::CONNECT)
			{
				// 连接超过最大尝试次数
				if (it->second.tryConnectCount > 10)
				{
					pushOutputOperation(it->first, P2POperationCMD::P2P_CONNECT_PEER_TIMEOUT, NULL, 0);
					it = m_connectToPeerSessionMng.erase(it);
					continue;
				}
				else
				{
					it->second.tryConnectCount++;
				}
			}
			it++;
		}
	}
}

void P2PPeer::startFailureLogic()
{
	m_pipe.close();
	m_loop.run(UV_RUN_DEFAULT);
	m_loop.close();
	m_state = PeerState::STOP;
}

void P2PPeer::onPipeRecvJsonCallback(P2PMessageID msgID, nlohmann::json& document, uint64_t key, const struct sockaddr* addr)
{
	switch (msgID)
	{
	case P2PMessageID::P2P_MSG_ID_T2C_CLIENT_LOGIN_RESULT:
	{
		auto it_key = document.find("key");
		if (it_key != document.end() && (*it_key).is_number_unsigned())
		{
			m_selfAddrInfo.key = it_key.value();
			m_isConnectTurn = true;
			m_isStopConnectToTurn = true;
			pushOutputOperation(0, P2POperationCMD::P2P_CONNECT_TURN_SUC, NULL, 0);
		}
	}
	break;
	case P2PMessageID::P2P_MSG_ID_T2C_START_BURROW:
	{
		auto it_key = document.find("key");
		if (it_key != document.end() && (*it_key).is_number_unsigned())
		{
			uint64_t tokey = it_key.value();
			startBurrow(tokey, false);
		}
	}break;
	case P2PMessageID::P2P_MSG_ID_C2C_HELLO:
	{
		auto it_key = document.find("isClient");
		if (it_key != document.end() && (*it_key).is_boolean())
		{
			bool isClient = it_key.value();
			if (isClient)
			{
				doSendCreateKcp(key);
			}
		}
	}
	break;
	case P2PMessageID::P2P_MSG_ID_C2T_CHECK_PEER_RESULT:
	{
		auto it_toKey = document.find("toKey");
		auto it_code = document.find("code");
		auto it_tarAddr = document.find("tarAddr");

		if (it_toKey != document.end() && (*it_toKey).is_number_unsigned() &&
			it_code != document.end() && (*it_code).is_number_integer() &&
			it_tarAddr != document.end() && (*it_tarAddr).is_number_unsigned())
		{
			uint64_t tokey = it_toKey.value();
			uint32_t code = it_code.value();
			uint64_t tarAddr = it_tarAddr.value();

			auto it = m_connectToPeerSessionMng.find(tokey);
			if (it == m_connectToPeerSessionMng.end())
			{
				return;
			}
			it->second.state = CONNECTING;
			it->second.tryConnectCount = 0;

			if (m_acceptPerrSessionMng.find(tarAddr) == m_acceptPerrSessionMng.end())
			{
				if (code == 0 || code == 1)
				{
					it->second.sendToAddr.key = tarAddr;
					startBurrow(tarAddr, true);
				}
				// code : 2
				// 找不到目标
				else
				{
					pushOutputOperation(it->first, P2POperationCMD::P2P_CONNECT_PEER_FAIL_NOT_FOUND, NULL, 0);
					m_connectToPeerSessionMng.erase(it);
				}
			}
			else
			{
				// 已经作为客户端连接到本Peer了
				pushOutputOperation(tarAddr, P2POperationCMD::P2P_CONNECT_PEER_FAIL_CLI_SESSION, NULL, 0);
				m_connectToPeerSessionMng.erase(it);
			}
		}
	}break;
	default:
		break;
	}
}

void P2PPeer::onPipeRecvKcpCallback(char* data, uint32_t len, uint64_t key, const struct sockaddr* addr)
{
	char* pData = (char*)fc_malloc(len);
	memcpy(pData, data, len);
	pushOutputOperation(key, P2POperationCMD::P2P_RECV_KCP_DATA, pData, len);
}

void P2PPeer::onPipeNewSessionCallback(uint64_t key)
{}

void P2PPeer::onPipeNewKcpCreateCallback(uint64_t key, uint32_t tag)
{
	if (m_turnAddrInfo.key == key)
	{
		return;
	}
	stopBurrow(key);

	// 接收到消息 P2P_MSG_ID_CREATE_KCP 
	if (tag == 0)
	{
		auto it = m_connectToPeerSessionMng.find(key);
		if (it != m_connectToPeerSessionMng.end())
		{
			if (it->second.state != SessionState::CONNECT)
			{
				it->second.state = SessionState::CONNECT;
				pushOutputOperation(key, P2POperationCMD::P2P_CONNECT_PEER_SUC, NULL, 0);
			}
		}
		else
		{
			for (it = m_connectToPeerSessionMng.begin(); it != m_connectToPeerSessionMng.end(); ++it)
			{
				if (it->second.sendToAddr.key == key)
				{
					it->second.state = SessionState::CONNECT;
					pushOutputOperation(it->first, P2POperationCMD::P2P_CONNECT_PEER_SUC, NULL, 0);
					break;
				}
			}
		}
	}
	// 接收到消息 P2P_MSG_ID_CREATE_KCP_RESULT
	else if(tag == 1)
	{
		auto it = m_acceptPerrSessionMng.find(key);
		if (it == m_acceptPerrSessionMng.end())
		{
			pushOutputOperation(key, P2POperationCMD::P2P_NEWCONNECT, NULL, 0);
			m_acceptPerrSessionMng[key] = true;
		}			
	}
}

void P2PPeer::onPipeRemoveSessionCallback(uint64_t key)
{
	if (m_turnAddrInfo.key == key)
	{
		m_isConnectTurn = false;
		m_isStopConnectToTurn = true;
		pushOutputOperation(0, P2POperationCMD::P2P_DISCONNECT_TURN, NULL, 0);
		return;
	}

	auto it_s = m_connectToPeerSessionMng.find(key);
	if (it_s != m_connectToPeerSessionMng.end())
	{
		pushOutputOperation(key, P2POperationCMD::P2P_DISCONNECT_PEER, NULL, 0);
		m_connectToPeerSessionMng.erase(it_s);
		return;
	}
	else
	{
		for (it_s = m_connectToPeerSessionMng.begin(); it_s != m_connectToPeerSessionMng.end(); ++it_s)
		{
			if (it_s->second.sendToAddr.key == key)
			{
				pushOutputOperation(it_s->first, P2POperationCMD::P2P_DISCONNECT_PEER, NULL, 0);
				m_connectToPeerSessionMng.erase(it_s);
				return;
			}
		}
	}

	auto it = m_acceptPerrSessionMng.find(key);
	if (it != m_acceptPerrSessionMng.end())
	{
		pushOutputOperation(key, P2POperationCMD::P2P_DISCONNECT_PEER, NULL, 0);
		m_acceptPerrSessionMng.erase(it);
		return;
	}
}

void P2PPeer::pushInputOperation(uint64_t key, uint32_t what, void* data, uint32_t datalen)
{
	OperationData opData;
	opData.key = key;
	opData.what = what;
	opData.data = data;
	opData.datalen = datalen;

	m_inputLock.lock();
	m_inputQue.emplace(opData);
	m_inputLock.unlock();
}

void P2PPeer::pushOutputOperation(uint64_t key, uint32_t what, void* data, uint32_t datalen)
{
	OperationData opData;
	opData.key = key;
	opData.what = what;
	opData.data = data;
	opData.datalen = datalen;

	m_outputLock.lock();
	m_outputQue.emplace(opData);
	m_outputLock.unlock();
}

void P2PPeer::runInputOperation()
{
	if (m_inputLock.trylock() != 0)
	{
		return;
	}

	if (m_inputQue.empty())
	{
		m_inputLock.unlock();
		return;
	}

	while (!m_inputQue.empty())
	{
		m_inputQueCache.push(m_inputQue.front());
		m_inputQue.pop();
	}

	m_inputLock.unlock();

	while (!m_inputQueCache.empty())
	{
		auto& opData = m_inputQueCache.front();
		switch (opData.what)
		{
		case P2POperationCMD::P2P_CONNECT_TO_PEER:
		{
			if (m_connectToPeerSessionMng.find(opData.key) == m_connectToPeerSessionMng.end())
			{
				nlohmann::json obj;
				obj["toKey"] = opData.key;

				std::string serialized_str = obj.dump();

				SessionData sessionData;
				sessionData.state = CHECKING;
				sessionData.tryConnectCount = 0;
				sessionData.sendToAddr.key = 0;
				m_connectToPeerSessionMng[opData.key] = sessionData;

				m_pipe.send(P2PMessageID::P2P_MSG_ID_C2T_CHECK_PEER, serialized_str.c_str(), serialized_str.size(), m_turnAddrInfo.ip, m_turnAddrInfo.port);
			}
		}break;
		case P2POperationCMD::P2P_CONNECT_TO_TURN:
		{
			m_tryConnectTurnCount = 0;
			if (m_isConnectTurn == false)
			{
				m_isStopConnectToTurn = false;
				doConnectToTurn();
			}
		}break;
		case P2POperationCMD::P2P_SEND_TO_PEER:
		{
			auto it = m_connectToPeerSessionMng.find(opData.key);
			if (it != m_connectToPeerSessionMng.end())
			{
				m_pipe.kcpSend((char*)opData.data, opData.datalen, it->second.sendToAddr.key);
			}
			else
			{
				m_pipe.kcpSend((char*)opData.data, opData.datalen, opData.key);
			}
			fc_free(opData.data);
		}break;
		case P2POperationCMD::P2P_DISCONNECT_TO_PEER:
		{
			m_pipe.disconnect(opData.key);
		}break;
		default:
			break;
		}
		m_inputQueCache.pop();
	}
}

void P2PPeer::startBurrow(uint64_t toKey, bool isClient)
{
	auto it = m_burrowManager.find(toKey);
	if (it == m_burrowManager.end())
	{
		AddrInfo info;
		info.key = toKey;

		char * ipaddr = NULL;
		char addr[20];
		in_addr inaddr;
		inaddr.s_addr = info.ip;
		ipaddr = inet_ntoa(inaddr);
		strcpy(addr, ipaddr);

		BurrowData burrowData;
		burrowData.sendCount = 0;
		uv_ip4_addr(addr, info.port, &burrowData.targetAddr);


		nlohmann::json obj;
		obj["isClient"] = isClient;
		burrowData.sendData = obj.dump();

		m_burrowManager.insert(std::make_pair(toKey, burrowData));

		m_pipe.send(P2PMessageID::P2P_MSG_ID_C2C_HELLO, burrowData.sendData.c_str(), burrowData.sendData.size(), (const struct sockaddr*)&burrowData.targetAddr);
	}
	else
	{
		it->second.sendCount = 0;
	}
}

void P2PPeer::stopBurrow(uint64_t key)
{
	auto it = m_burrowManager.find(key);
	if (it != m_burrowManager.end())
	{
		m_burrowManager.erase(it);
	}
}

void P2PPeer::doConnectToTurn()
{
	if (m_isStopConnectToTurn)
		return;

	if (m_tryConnectTurnCount > 10)
	{
		m_localAddrInfoCache.clear();
		m_isStopConnectToTurn = true;
		pushOutputOperation(0, P2POperationCMD::P2P_CONNECT_TURN_FAIL, NULL, 0);
		return;
	}

	if (m_localAddrInfoCache.empty())
	{
		getLocalAddressIPV4Info(m_localAddrInfoCache);
	}
	

	nlohmann::json obj;
	for (auto i = 0U; i < m_localAddrInfoCache.size(); ++i)
	{
		nlohmann::json tmp;
		tmp["ip"] = m_localAddrInfoCache[i].addr.key;
		tmp["mask"] = m_localAddrInfoCache[i].mask;

		obj[i] = tmp;
	}

	std::string serialized_str = obj.dump();

	m_tryConnectTurnCount++;
	m_pipe.send(P2PMessageID::P2P_MSG_ID_C2T_CLIENT_LOGIN, serialized_str.c_str(), serialized_str.size(), m_turnAddrInfo.ip, m_turnAddrInfo.port);
}

void P2PPeer::doSendCreateKcp(uint64_t toKey)
{
	AddrInfo info;
	info.key = toKey;
	m_pipe.send(P2PMessageID::P2P_MSG_ID_CREATE_KCP, P2P_NULL_JSON, P2P_NULL_JSON_LEN, info.ip, info.port);
}

void P2PPeer::clearData()
{
	m_inputLock.lock();

	while (!m_inputQue.empty())
	{
		if (m_inputQue.front().data != NULL)
		{
			fc_free(m_inputQue.front().data);
		}
		m_inputQue.pop();
	}

	m_inputLock.unlock();

	m_outputLock.lock();

	while (!m_outputQue.empty())
	{
		if (m_outputQue.front().data != NULL)
		{
			fc_free(m_outputQue.front().data);
		}
		m_outputQue.pop();
	}

	m_outputLock.unlock();
}

void P2PPeer::getLocalAddressIPV4Info(std::vector<LocNetAddrInfo>& infoArr)
{
	if (m_pipe.getSocket() == NULL || m_pipe.getSocket()->getPort() == 0)
	{
		return;
	}

	uv_interface_address_t *info;
	int count, i;

	uv_interface_addresses(&info, &count);
	i = count;

	uint32_t bindport = m_pipe.getSocket()->getPort();

	while (i--) 
	{
		uv_interface_address_t interface = info[i];
		if (!interface.is_internal && interface.address.address4.sin_family == AF_INET)
		{
			LocNetAddrInfo locnetInfo;
			locnetInfo.addr.ip = interface.address.address4.sin_addr.s_addr;
			locnetInfo.addr.port = bindport;
			locnetInfo.mask = interface.netmask.netmask4.sin_addr.s_addr;

			infoArr.push_back(locnetInfo);
		}
	}
	uv_free_interface_addresses(info, count);
}

NS_NET_UV_END

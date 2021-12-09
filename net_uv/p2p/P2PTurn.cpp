#include "P2PTurn.h"

NS_NET_UV_BEGIN

P2PTurn::P2PTurn()
	: m_state(TurnState::STOP)
{
	m_pipe.setRecvJsonCallback(std::bind(&P2PTurn::onPipeRecvJsonCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	m_pipe.setRecvKcpCallback(std::bind(&P2PTurn::onPipeRecvKcpCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	m_pipe.setNewSessionCallback(std::bind(&P2PTurn::onPipeNewSessionCallback, this, std::placeholders::_1));
	m_pipe.setNewKcpCreateCallback(std::bind(&P2PTurn::onPipeNewKcpCreateCallback, this, std::placeholders::_1));
	m_pipe.setRemoveSessionCallback(std::bind(&P2PTurn::onPipeRemoveSessionCallback, this, std::placeholders::_1));
}

P2PTurn::~P2PTurn()
{
	stop();
	m_thread.join();
}

bool P2PTurn::start(const char* ip, uint32_t port)
{
	if (m_state != TurnState::STOP)
	{
		return false;
	}
	
	if (m_pipe.bind(ip, port, m_loop.ptr()) == false)
	{
		m_loop.run(UV_RUN_DEFAULT);
		m_loop.close();
		return false;
	}
	m_state = TurnState::START;

	m_thread.create([](void* arg) 
	{
		P2PTurn* self = (P2PTurn*)arg;
		self->run();
	}, this);

	return true;
}

void P2PTurn::stop()
{
	m_state = TurnState::WILL_STOP;
}

/// Runnable
void P2PTurn::run()
{
	m_idle.start(m_loop.ptr(), [](uv_timer_t* handle)
	{
		((P2PTurn*)handle->data)->onIdleRun();
	}, 1, 1, this);
		
	m_loop.run(UV_RUN_DEFAULT);
	m_loop.close();
	
	m_state = TurnState::STOP;
}

void P2PTurn::onIdleRun()
{
	m_pipe.update((IUINT32)(uv_now(m_loop.ptr()) & 0xfffffffful));
	if (m_state == TurnState::WILL_STOP)
	{
		m_pipe.close();
		m_idle.stop();
	}
}

void P2PTurn::onPipeRecvJsonCallback(P2PMessageID msgID, nlohmann::json& document, uint64_t key, const struct sockaddr* addr)
{
	switch (msgID)
	{
	case net_uv::P2P_MSG_ID_C2T_CLIENT_LOGIN:
	{
		auto it = m_key_peerDataMap.find(key);
		if (it != m_key_peerDataMap.end())
		{
			m_key_peerDataMap.erase(it);
		}
		
		if (document.is_array())
		{
			PeerData peerData;
			peerData.localAddrInfoCount = 0;
			peerData.addrInfo.key = key;

			
			auto size = document.size();
			for (auto i = 0U; i < size; ++i)
			{
				if(i >= PEER_LOCAL_ADDR_INFO_MAX_COUNT)
					break;

				auto& obj = document[i];

				if (!obj.is_object())
					break;

				auto it_ip = obj.find("ip");
				auto it_mask = obj.find("mask");

				if (it_ip != obj.end() && (*it_ip).is_number_integer() &&
					it_mask != obj.end() && (*it_mask).is_number_integer())
				{
					peerData.localAddrInfoArr[i].addr.key = it_ip.value();
					peerData.localAddrInfoArr[i].mask = it_mask.value();
					peerData.localAddrInfoCount = i + 1;
				}
			}
			m_key_peerDataMap.emplace(key, peerData);
		}

		nlohmann::json obj;
		obj["key"] = key;

		std::string serialized_str = obj.dump();
		m_pipe.send(P2PMessageID::P2P_MSG_ID_T2C_CLIENT_LOGIN_RESULT, serialized_str.c_str(), serialized_str.size(), addr);
	}
		break;
	case net_uv::P2P_MSG_ID_C2T_CHECK_PEER:
	{
		auto it_toKey = document.find("toKey");
		if (it_toKey != document.end() && (*it_toKey).is_number_unsigned())
		{
			uint64_t toKey = it_toKey.value();
			uint32_t code = 0;
			uint64_t targetAddr = toKey;
			uint64_t burrowAddr = key;

			auto it_target = m_key_peerDataMap.find(toKey);
			auto it_my = m_key_peerDataMap.find(key);
			if (it_target == m_key_peerDataMap.end() || it_my == m_key_peerDataMap.end())
			{
				// 找不到目标
				code = 2;
			}
			else
			{
				AddrInfo targetInfo;
				targetInfo.key = toKey;

				AddrInfo myInfo;
				myInfo.key = key;

				// 同一局域网
				if (myInfo.ip == targetInfo.ip && it_target->second.localAddrInfoCount > 0 && it_my->second.localAddrInfoCount > 0)
				{
					// 判断是否在一个网段
					for (auto i = 0U; i < it_target->second.localAddrInfoCount; ++i)
					{
						uint32_t tag_ip = it_target->second.localAddrInfoArr[i].addr.ip;
						uint32_t tag_mask = it_target->second.localAddrInfoArr[i].mask;
						for (auto j = 0U; j < it_my->second.localAddrInfoCount; ++j)
						{
							uint32_t my_ip = it_my->second.localAddrInfoArr[j].addr.ip;
							uint32_t my_mask = it_my->second.localAddrInfoArr[j].mask;
							if ((tag_ip & tag_mask) == (my_ip & my_mask))
							{
								// 使用局域网连接
								code = 1;
								targetAddr = it_target->second.localAddrInfoArr[i].addr.key;
							}
						}
					}
				}
			}

			nlohmann::json obj;
			obj["code"] = code;
			obj["toKey"] = toKey;
			obj["tarAddr"] = targetAddr;

			std::string serialized_str = obj.dump();
			m_pipe.send(P2PMessageID::P2P_MSG_ID_C2T_CHECK_PEER_RESULT, serialized_str.c_str(), serialized_str.size(), addr);

			/// 向目标peer发送打洞指令
			if (code == 0)
			{
				nlohmann::json obj;
				obj["key"] = burrowAddr;

				std::string serialized_str = obj.dump();
				m_pipe.send(P2PMessageID::P2P_MSG_ID_T2C_START_BURROW, serialized_str.c_str(), serialized_str.size(), it_target->second.addrInfo.ip, it_target->second.addrInfo.port);
			}
		}
	} break;
	default:
		break;
	}
}

void P2PTurn::onPipeRecvKcpCallback(char* data, uint32_t len, uint64_t key, const struct sockaddr* addr)
{}

void P2PTurn::onPipeNewSessionCallback(uint64_t key)
{
	NET_UV_LOG(NET_UV_L_INFO,"%llu\t进入", key);
}

void P2PTurn::onPipeNewKcpCreateCallback(uint64_t key)
{}

void P2PTurn::onPipeRemoveSessionCallback(uint64_t key)
{
	NET_UV_LOG(NET_UV_L_INFO, "%llu\t离开\n", key);
	
	auto it = m_key_peerDataMap.find(key);
	if (it != m_key_peerDataMap.end())
	{
		m_key_peerDataMap.erase(it);
	}
}

NS_NET_UV_END

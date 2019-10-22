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
	m_idle.start(m_loop.ptr(), [](uv_idle_t* handle)
	{
		((P2PTurn*)handle->data)->onIdleRun();
	}, this);
		
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
	ThreadSleep(1);
}

void P2PTurn::onPipeRecvJsonCallback(P2PMessageID msgID, rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)
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
		
		if (document.IsArray())
		{
			PeerData peerData;
			peerData.localAddrInfoCount = 0;
			peerData.addrInfo.key = key;

			auto cliInfoArr = document.GetArray();
			
			for (auto i = 0U; i < cliInfoArr.Size(); ++i)
			{
				if(i >= PEER_LOCAL_ADDR_INFO_MAX_COUNT)
					break;

				if(!cliInfoArr[i].IsObject() 
					|| !cliInfoArr[i].HasMember("ip") 
					|| !cliInfoArr[i].HasMember("mask")
					|| !cliInfoArr[i]["ip"].IsUint64()
					|| !cliInfoArr[i]["mask"].IsUint())
					break;

				peerData.localAddrInfoArr[i].addr.key = cliInfoArr[i]["ip"].GetUint64();
				peerData.localAddrInfoArr[i].mask = cliInfoArr[i]["mask"].GetUint();
				peerData.localAddrInfoCount = i + 1;
			}
			m_key_peerDataMap.emplace(key, peerData);
		}

		rapidjson::StringBuffer s;
		rapidjson::Writer<rapidjson::StringBuffer> writer(s);

		writer.StartObject();
		writer.Key("key");
		writer.Uint64(key);
		writer.EndObject();

		m_pipe.send(P2PMessageID::P2P_MSG_ID_T2C_CLIENT_LOGIN_RESULT, s.GetString(), s.GetLength(), addr);
	}
		break;
	case net_uv::P2P_MSG_ID_C2T_CHECK_PEER:
	{
		if (document.HasMember("toKey"))
		{
			rapidjson::Value& key_value = document["toKey"];
			if (key_value.IsUint64())
			{
				rapidjson::StringBuffer s;
				rapidjson::Writer<rapidjson::StringBuffer> writer(s);

				uint32_t code = 0;
				uint64_t targetAddr = key_value.GetUint64();
				uint64_t burrowAddr = key;

				auto it_target = m_key_peerDataMap.find(key_value.GetUint64());
				auto it_my = m_key_peerDataMap.find(key);
				if (it_target == m_key_peerDataMap.end() || it_my == m_key_peerDataMap.end())
				{
					// 找不到目标
					code = 2;
				}
				else
				{
					AddrInfo targetInfo;
					targetInfo.key = key_value.GetUint64();

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
				writer.StartObject();

				writer.Key("code");
				writer.Uint(code);

				writer.Key("toKey");
				writer.Uint64(key_value.GetUint64());

				writer.Key("tarAddr");
				writer.Uint64(targetAddr);

				writer.EndObject();
				
				m_pipe.send(P2PMessageID::P2P_MSG_ID_C2T_CHECK_PEER_RESULT, s.GetString(), s.GetLength(), addr);

				/// 向目标peer发送打洞指令
				if (code == 0)
				{
					rapidjson::StringBuffer s;
					rapidjson::Writer<rapidjson::StringBuffer> writer(s);

					writer.StartObject();
					writer.Key("key");
					writer.Uint64(burrowAddr);
					writer.EndObject();

					m_pipe.send(P2PMessageID::P2P_MSG_ID_T2C_START_BURROW, s.GetString(), s.GetLength(), it_target->second.addrInfo.ip, it_target->second.addrInfo.port);
				}
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

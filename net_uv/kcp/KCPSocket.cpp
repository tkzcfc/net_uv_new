#include "KCPSocket.h"

NS_NET_UV_BEGIN


KCPSocket::KCPSocket(uv_loop_t* loop)
	: m_socketAddr(NULL)
	, m_udp(NULL)
	, m_socketMng(NULL)
	, m_kcpState(State::CLOSED)
	, m_newConnectionCall(nullptr)
	, m_connectFilterCall(nullptr)
	, m_conv(0)
	, m_weakRefTag(false)
	, m_kcp(NULL)
	, m_recvBuf(NULL)
	, m_soType(KCPSOType::CLI_SO)
	, m_autoSend_Data(NULL)
	, m_autoSend_DataLen(0)
	, m_autoSendCount(-1)
	, m_idle(NULL)
	, m_idleUpdateTime(0U)
	, m_lastKcpRecvTime(0U)
	, m_randValue(0)
	, m_heartbeatLoseCount(0U)
{
	m_loop = loop;
}

KCPSocket::~KCPSocket()
{
	cancelCurAutoSend();

	FC_SAFE_FREE(m_socketAddr);
	FC_SAFE_FREE(m_recvBuf);

	if (m_idle)
	{
		m_idle->~UVIdle();
		fc_free(m_idle);
		m_idle = NULL;
	}

	this->releaseKcp();

	if (!m_weakRefTag && m_udp)
	{
		net_closeHandle((uv_handle_t*)m_udp, net_closehandle_defaultcallback);
		m_udp = NULL;
	}

	if (m_socketMng != NULL && !m_weakRefTag)
	{
		m_socketMng->~KCPSocketManager();
		fc_free(m_socketMng);
		m_socketMng = NULL;
	}
	stopIdle();
}

bool KCPSocket::bind(const char* ip, uint32_t port)
{
	if (m_udp != NULL)
	{
		return false;
	}

	this->setIp(ip);
	this->setPort(port);
	this->setIsIPV6(false);
	this->m_soType = KCPSOType::SVR_SO;

	struct sockaddr_in bind_addr;
	int32_t r = uv_ip4_addr(ip, port, &bind_addr);

	if (r != 0)
	{
		return false;
	}

	m_udp = (uv_udp_t*)fc_malloc(sizeof(uv_udp_t));
	r = uv_udp_init(m_loop, m_udp);
	CHECK_UV_ASSERT(r);
	m_udp->data = this;

	r = uv_udp_bind(m_udp, (const struct sockaddr*) &bind_addr, UV_UDP_REUSEADDR);
	
	return (r == 0);
}

bool KCPSocket::bind6(const char* ip, uint32_t port)
{
	if (m_udp != NULL)
	{
		return false;
	}

	this->setIp(ip);
	this->setPort(port);
	this->setIsIPV6(true);
	this->m_soType = KCPSOType::SVR_SO;

	struct sockaddr_in6 bind_addr;
	int32_t r = uv_ip6_addr(ip, port, &bind_addr);

	if (r != 0)
	{
		return false;
	}

	m_udp = (uv_udp_t*)fc_malloc(sizeof(uv_udp_t));
	r = uv_udp_init(m_loop, m_udp);
	CHECK_UV_ASSERT(r);
	m_udp->data = this;

	r = uv_udp_bind(m_udp, (const struct sockaddr*) &bind_addr, UV_UDP_REUSEADDR);

	return (r == 0);
}

bool KCPSocket::listen(int32_t count)
{
	if (m_socketMng)
	{
		assert(0);
		return true;
	}

	if (m_udp == NULL)
	{
		assert(0);
		return false;
	}

	int32_t r = uv_udp_recv_start(m_udp, uv_on_alloc_buffer, uv_on_after_read);
	if (r != 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "uv_udp_recv_start error: %s", net_getUVError(r).c_str());
		return false;
	}

	m_kcpState = State::LISTEN;

	m_socketMng = (KCPSocketManager*)fc_malloc(sizeof(KCPSocketManager));
	new (m_socketMng) KCPSocketManager(m_loop, this);

	net_adjustBuffSize((uv_handle_t*)m_udp, KCP_UV_SOCKET_RECV_BUF_LEN, KCP_UV_SOCKET_SEND_BUF_LEN);

	if (getPort() == 0)
	{
		setPort(net_udp_getPort(m_udp));
	}

	return true;
}

bool KCPSocket::connect(const char* ip, uint32_t port)
{
	if (m_kcpState != State::CLOSED)
	{
		return false;
	}

	struct sockaddr* addr = net_getsocketAddr(ip, port, NULL);

	if (addr == NULL)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "[%s:%d]Address resolution failed", ip, port);
		return false;
	}

	this->setIp(ip);
	this->setPort(port);
	this->setSocketAddr(addr);
	this->m_soType = KCPSOType::CLI_SO;

	if (m_udp == NULL)
	{
		m_udp = (uv_udp_t*)fc_malloc(sizeof(uv_udp_t));
		int32_t r = uv_udp_init(m_loop, m_udp);
		CHECK_UV_ASSERT(r);
		m_udp->data = this;

		struct sockaddr_in bind_addr;
		r = uv_ip4_addr("0.0.0.0", 0, &bind_addr);
		CHECK_UV_ASSERT(r);
		r = uv_udp_bind(m_udp, (const struct sockaddr *)&bind_addr, 0);
		CHECK_UV_ASSERT(r);
		//r = uv_udp_set_broadcast(m_udp, 1);
		//CHECK_UV_ASSERT(r);

		net_adjustBuffSize((uv_handle_t*)m_udp, KCP_UV_SOCKET_RECV_BUF_LEN, KCP_UV_SOCKET_SEND_BUF_LEN);
	}

	int32_t r = uv_udp_recv_start(m_udp, uv_on_alloc_buffer, uv_on_after_read);
	if (r != 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "uv_udp_recv_start error: %s", net_getUVError(r).c_str());
		return false;
	}

	m_kcpState = State::SYN_SENT;

	m_randValue = std::rand() % 100 + 100;
	std::string str = kcp_making_syn(m_randValue);
	setAutoSendData(str.c_str(), str.size());

	startIdle();

	return true;
}

bool KCPSocket::send(char* data, int32_t len)
{
	if (m_kcp == NULL)
	{
		return false;
	}
	assert(len <= KCP_MAX_MSG_SIZE);
	return ikcp_send(m_kcp, data, len) >= 0;
}

void KCPSocket::disconnect()
{
	m_lastKcpRecvTime = 0;

	if (m_kcpState == State::LISTEN)
	{
		m_kcpState = State::STOP_LISTEN;
		startIdle();
		return;
	}
	if (m_kcpState == State::STOP_LISTEN)
	{
		return;
	}

	if (getConv() != 0)
	{
		std::string str = kcp_making_fin(getConv());
		setAutoSendData(str.c_str(), str.size());

		if (m_soType == KCPSOType::CLI_SO)
		{
			m_kcpState = (m_kcpState == State::ESTABLISHED) ? State::FIN_WAIT : State::CLOSED;
		}
		else
		{
			m_kcpState = (m_kcpState == State::ESTABLISHED) ? State::CLOSE_WAIT : State::CLOSED;
		}
	}
	this->releaseKcp();
}

bool KCPSocket::accept(const struct sockaddr* addr, uint32_t synValue)
{
	if (m_socketMng == NULL)
		return false;

	std::string strip;
	uint32_t port;
	uint32_t addrlen = net_getsockAddrIPAndPort(addr, strip, port);
	if (addrlen == 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "KCPSocket created by kcp svr socket failed, address resolution failed");
		return false;
	}

	struct sockaddr* socker_addr = (struct sockaddr*)fc_malloc(addrlen);
	memcpy(socker_addr, addr, addrlen);

	KCPSocket* socket = (KCPSocket*)fc_malloc(sizeof(KCPSocket));
	new (socket) KCPSocket(m_loop);
	socket->setIp(strip);
	socket->setPort(port);
	socket->setIsIPV6(socker_addr->sa_family == AF_INET6);
	socket->setWeakRefSocketManager(m_socketMng);
	socket->m_udp = this->m_udp; 
	socket->m_soType = KCPSOType::SVR_SO;
	socket->svr_connect(socker_addr, m_socketMng->createConvID(), synValue);

	m_socketMng->push(socket);

	return true;
}

void KCPSocket::svr_connect(struct sockaddr* addr, IUINT32 conv, uint32_t synValue)
{
	this->setSocketAddr(addr);
	this->setConv(conv);

	m_kcpState = State::SYN_RECV;

	std::string str = kcp_making_syn_and_ack(synValue + 1, conv);
	setAutoSendData(str.c_str(), str.size());

	startIdle();
}

void KCPSocket::idleLogic()
{
	if (m_kcp)
	{
		ikcp_update(m_kcp, (IUINT32)(uv_now(m_loop) & 0xfffffffful));
	}

	if (m_kcpState == State::STOP_LISTEN)
	{
		if (uv_now(m_loop) - m_idleUpdateTime > 500)
		{
			m_idleUpdateTime = uv_now(m_loop);
			if (m_socketMng->count() <= 0)
			{
				shutdownSocket();
			}
			else
			{
				m_socketMng->disconnectAll();
			}
		}
		return;
	}

	if (uv_now(m_loop) - m_idleUpdateTime > 200)
	{
		m_idleUpdateTime = uv_now(m_loop);

		if (m_lastKcpRecvTime > 0 && uv_now(m_loop) - m_lastKcpRecvTime > 6000U)
		{
			m_heartbeatLoseCount++;

			// If there is no message communication more than one minute
			if (m_heartbeatLoseCount > 9)
			{
				this->disconnect();
				return;
			}

			if (m_soType == CLI_SO)
			{
				if (m_heartbeatLoseCount > 2)
				{
					std::string str = kcp_making_heart_packet();
					this->udpSend(str.c_str(), str.size());
				}
			}
			else
			{
				if (m_heartbeatLoseCount > 5)
				{
					std::string str = kcp_making_heart_packet();
					this->udpSend(str.c_str(), str.size());
				}
			}
			m_lastKcpRecvTime = uv_now(m_loop);
		}

		autoSendLogic();
	}
}

void KCPSocket::startIdle()
{
	if (m_idle == NULL)
	{
		m_idle = (UVIdle*)fc_malloc(sizeof(UVIdle));
		new (m_idle) UVIdle();
	}

	m_idle->start(m_loop, [](uv_idle_t* handle) 
	{
		KCPSocket* s = (KCPSocket*)handle->data;
		s->idleLogic();
	}, this);
}

void KCPSocket::stopIdle()
{
	if (NULL == m_idle || false == m_idle->isRunning())
	{
		return;
	}
	m_idle->stop();
	m_idle->ptr()->data = m_idle;
	m_idle->close([](uv_handle_t* handle) 
	{
		fc_free(handle->data);
	});
	m_idle = NULL;
}

void KCPSocket::tryCloseSocket(uv_handle_t* handle)
{
	if (handle && handle == (uv_handle_t*)m_udp)
	{
		fc_free(handle);
		m_udp = NULL;
	}
	
	if (m_udp != NULL)
		return;

	if (m_kcpState == State::FIN_WAIT || m_kcpState == State::CLOSE_WAIT || m_kcpState == State::STOP_LISTEN)
	{
		if (this->m_closeCall != nullptr)
		{
			this->m_closeCall(this);
		}
	}

	if (m_weakRefTag)
	{
		auto mng = m_socketMng;
		m_socketMng = NULL;
		mng->remove(this);
	}
}

void KCPSocket::shutdownSocket()
{
	cancelCurAutoSend();
	releaseKcp();
	stopIdle();
	
	if (!m_weakRefTag)
	{
		if (m_udp)
		{
			uv_udp_recv_stop(m_udp);

			net_closeHandle((uv_handle_t*)m_udp, [](uv_handle_t* handle)
			{
				((KCPSocket*)handle->data)->tryCloseSocket(handle);
			});
		}
	}
	else
	{
		m_udp = NULL;
	}

	this->tryCloseSocket(NULL);
}

void KCPSocket::setSocketAddr(struct sockaddr* addr)
{
	if (m_socketAddr == addr)
		return;
	FC_SAFE_FREE(m_socketAddr);
	m_socketAddr = addr;
}

void KCPSocket::udpSend(const char* data, int32_t len)
{
	if (getSocketAddr() == NULL || m_udp == NULL)
	{
		return;
	}

	uv_buf_t* buf = (uv_buf_t*)fc_malloc(sizeof(uv_buf_t));
	buf->base = (char*)fc_malloc(len);
	buf->len = len;

	memcpy(buf->base, data, len);

	uv_udp_send_t* udp_send = (uv_udp_send_t*)fc_malloc(sizeof(uv_udp_send_t));
	udp_send->data = buf;
	int32_t r = uv_udp_send(udp_send, m_udp, buf, 1, getSocketAddr(), uv_on_udp_send);

	if (r != 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "udp send error %s", uv_strerror(r));
	}
}

void KCPSocket::kcpInput(const char* data, long size)
{
	if (size <= 0 || m_kcp == NULL)
		return;
	resetCheckTimer();

	ikcp_input(m_kcp, data, size);

	int32_t kcp_recvd_bytes = 0;
	do
	{
		if(m_kcp == NULL) break;
		kcp_recvd_bytes = ikcp_recv(m_kcp, m_recvBuf, KCP_RECV_BUF_SIZE);

		if (kcp_recvd_bytes > 0)
		{
			m_recvCall(m_recvBuf, kcp_recvd_bytes);
		}
	} while (kcp_recvd_bytes > 0);
}

void KCPSocket::initKcp(IUINT32 conv)
{
	if (m_recvBuf == NULL)
	{
		m_recvBuf = (char*)fc_malloc(KCP_RECV_BUF_SIZE);
	}

	this->releaseKcp();
	this->setConv(conv);

	m_kcp = ikcp_create(conv, this);
	m_kcp->output = &KCPSocket::udp_output;
	m_kcp->stream = 1;

	ikcp_wndsize(m_kcp, 128, 128);

	// 启动快速模式
	// 第二个参数 nodelay-启用以后若干常规加速将启动
	// 第三个参数 interval为内部处理时钟，默认设置为 10ms
	// 第四个参数 resend为快速重传指标，设置为2
	// 第五个参数 为是否禁用常规流控，这里禁止
	ikcp_nodelay(m_kcp, 1, 10, 2, 1);
	//ikcp_nodelay(m_kcp, 1, 5, 1, 1); // 设置成1次ACK跨越直接重传, 这样反应速度会更快. 内部时钟5毫秒.
}

void KCPSocket::releaseKcp()
{
	if (m_kcp)
	{
		ikcp_release(m_kcp);
		m_kcp = NULL;
	}
	this->setConv(0);
}

void KCPSocket::onUdpRead(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, uint32_t flags)
{
	if (m_soType == KCPSOType::SVR_SO)
	{
		if (m_kcpState == KCPSocket::State::LISTEN)
		{
			auto containSO = m_socketMng->getSocketBySockAddr(addr);

			if (kcp_is_syn(buf->base, nread))
			{
				if (containSO == NULL && m_connectFilterCall(addr))
				{
					uint32_t synValue = kcp_grab_syn_value(buf->base, nread);
					if (synValue > 0)
					{
						this->accept(addr, synValue);
					}
				}
			}
			else
			{
				if (containSO)
				{
					containSO->onUdpRead(handle, nread, buf, addr, flags);
				}
			}
		}
		else if (m_kcpState == State::STOP_LISTEN)
		{}
		else
		{
			if (kcp_is_ack(buf->base, nread))
			{
				if (m_kcpState == KCPSocket::State::SYN_RECV)
				{
					uint32_t conv = kcp_grab_ack_value(buf->base, nread);
					if (this->getConv() == conv - 1)
					{
						m_kcpState = KCPSocket::State::ESTABLISHED;
						this->initKcp(this->getConv());
						this->cancelCurAutoSend();
						m_socketMng->online(this);
						m_socketMng->getOwner()->m_newConnectionCall(this);
						this->resetCheckTimer();
					}
					else
					{
						this->disconnect();
					}
				}
			}
			else if (kcp_is_fin(buf->base, nread))
			{
				if (kcp_grab_fin_value(buf->base, nread) == this->getConv())
				{
					this->disconnect();
				}
				else
				{
					if (m_kcpState == KCPSocket::State::CLOSE_WAIT)
					{
						shutdownSocket();
					}
				}
			}
			else if (kcp_is_heart_packet(buf->base, nread))
			{
				this->resetCheckTimer();
				std::string str = kcp_making_heart_back_packet();
				this->udpSend(str.c_str(), str.size());
			}
			else if (kcp_is_heart_back_packet(buf->base, nread))
			{
				this->resetCheckTimer();
			}
			else
			{
				if (m_kcpState == KCPSocket::State::ESTABLISHED)
				{
					kcpInput(buf->base, nread);
				}
			}
		}
	}
	else
	{
		if (kcp_is_syn_and_ack(buf->base, nread))
		{
			if (m_kcpState == KCPSocket::State::SYN_SENT)
			{
				uint32_t value, conv;
				if (kcp_grab_syn_and_ack_value(buf->base, nread, &value, &conv) && value == m_randValue + 1)
				{
					m_kcpState = KCPSocket::State::ESTABLISHED;
					std::string str = kcp_making_ack(conv + 1);

					this->setAutoSendData(str.c_str(), str.size());
					this->initKcp(conv);
					this->connectResult(1);
					this->resetCheckTimer();
				}
				else
				{
					this->connectResult(0);
				}
			}
		}
		else if (kcp_is_fin(buf->base, nread))
		{
			if (kcp_grab_fin_value(buf->base, nread) == this->getConv())
			{
				this->disconnect();
			}
			else
			{
				if (m_kcpState == State::SYN_SENT)
				{
					this->connectResult(0);
				}
				else if(m_kcpState == State::CLOSE_WAIT)
				{
					shutdownSocket();
				}
			}
		}
		else if (kcp_is_heart_packet(buf->base, nread))
		{
			this->resetCheckTimer();
			std::string str = kcp_making_heart_back_packet();
			this->udpSend(str.c_str(), str.size());
		}
		else if (kcp_is_heart_back_packet(buf->base, nread))
		{
			this->resetCheckTimer();
		}
		else
		{
			if (m_kcpState == State::ESTABLISHED)
			{
				kcpInput(buf->base, nread);
			}
		}
	}
}

void KCPSocket::connectResult(int32_t status)
{
	if (m_connectCall)
	{
		m_connectCall(this, status);
	}
	if (status == 2)
	{
		shutdownSocket();
	}
}

void KCPSocket::acceptFailed()
{
	this->shutdownSocket();
}

void KCPSocket::setAutoSendData(const char* data, uint32_t len)
{
	cancelCurAutoSend();

	if (data == NULL || len <= 0)
		return;

	m_autoSend_Data = (char*)fc_malloc(len);
	memcpy(m_autoSend_Data, data, len);
	m_autoSend_DataLen = len;
	m_autoSendCount = 5;

	this->udpSend(m_autoSend_Data, m_autoSend_DataLen);
}

void KCPSocket::cancelCurAutoSend()
{
	FC_SAFE_FREE(m_autoSend_Data);
	m_autoSend_DataLen = 0;
	m_autoSendCount = -1;
}

void KCPSocket::autoSendLogic()
{
	if (m_autoSendCount < 0)
	{
		return;
	}
	else if (m_autoSendCount == 0)
	{
		switch (m_kcpState)
		{
		case State::LISTEN:
			break;
		case State::SYN_SENT:
			// timeout
			this->connectResult(2);
			break;
		case State::SYN_RECV:
			this->acceptFailed();
			break;
		case State::ESTABLISHED:
			break;
		case State::FIN_WAIT:
		case State::CLOSE_WAIT:
		case State::CLOSED:
			this->shutdownSocket();
			break;
		default:
			break;
		}
	}
	else
	{
		if (m_autoSend_Data)
		{
			this->udpSend(m_autoSend_Data, m_autoSend_DataLen);
		}
	}
	m_autoSendCount--;
}

void KCPSocket::resetCheckTimer()
{
	m_lastKcpRecvTime = uv_now(m_loop);
	m_heartbeatLoseCount = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void KCPSocket::uv_on_udp_send(uv_udp_send_t *req, int32_t status)
{
	if (status != 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "udp send error %s", uv_strerror(status));
	}
	uv_buf_t* buf = (uv_buf_t*)req->data;
	fc_free(buf->base);
	fc_free(buf);
	fc_free(req);
}

void KCPSocket::uv_on_after_read(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{
	if (nread < 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "udp read error %s\n", uv_err_name(nread));
		uv_on_free_buffer((uv_handle_t*)handle, buf);
		return;
	}
	if (addr == NULL)
	{
		//NET_UV_LOG(NET_UV_L_ERROR, "null pointer");
		uv_on_free_buffer((uv_handle_t*)handle, buf);
		return;
	}

	if (nread > 0)
	{
		KCPSocket* s = (KCPSocket*)handle->data;
		s->onUdpRead(handle, nread, buf, addr, flags);
	}
	uv_on_free_buffer((uv_handle_t*)handle, buf);
}

int32_t KCPSocket::udp_output(const char *buf, int32_t len, ikcpcb *kcp, void *user)
{
	KCPSocket* socket = (KCPSocket*)user;
	socket->udpSend(buf, len);
	return 0;
}

NS_NET_UV_END
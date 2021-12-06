#include "KCPSocket.h"

NS_NET_UV_BEGIN


KCPSocket::KCPSocket(uv_loop_t* loop)
	: m_socketAddr(NULL)
	, m_udp(NULL)
	, m_socketMng(NULL)
	, m_kcpState(State::CLOSED)
	, m_stateTimeStamp(0U)
	, m_newConnectionCall(nullptr)
	, m_connectFilterCall(nullptr)
	, m_conv(0)
	, m_weakRefTag(false)
	, m_kcp(NULL)
	, m_recvBuf(NULL)
	, m_soType(KCPSOType::CLI_SO)
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
	FC_SAFE_FREE(m_socketAddr);
	FC_SAFE_FREE(m_recvBuf);

	assert(m_udp == NULL);

	if (m_socketMng != NULL && !m_weakRefTag)
	{
		m_socketMng->~KCPSocketManager();
		fc_free(m_socketMng);
		m_socketMng = NULL;
	}
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
	assert(m_socketMng == NULL);
	assert(m_udp != NULL);

	int32_t r = uv_udp_recv_start(m_udp, uv_on_alloc_buffer, uv_on_after_read);
	if (r != 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "uv_udp_recv_start error: %s", net_getUVError(r).c_str());
		return false;
	}

	this->changeState(State::LISTEN);

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

	changeState(State::SYN_SENT);
	m_randValue = std::rand() % 1000 + std::rand() % 1000 + 1000;
	this->udpSendStr(kcp_making_syn(m_randValue));

	startIdle();

	return true;
}

bool KCPSocket::send(char* data, int len, SocketSendCall call, void* userdata)
{
	if (m_kcp == NULL)
	{
		return false;
	}
	assert(len <= KCP_MAX_MSG_SIZE);
	bool ok = ikcp_send(m_kcp, data, len) >= 0;

	if (call) call(userdata);

	return ok;
}

void KCPSocket::disconnect()
{
	if (m_kcpState == State::LISTEN)
	{
		this->changeState(State::STOP_LISTEN);
		m_socketMng->disconnectAll();
		startIdle();
		return;
	}
	if (m_kcpState == State::STOP_LISTEN)
	{
		return;
	}

	if (getConv() != 0)
	{
		// socket connected
		if(m_kcpState == State::ESTABLISHED)
		{
			this->udpSendStr(kcp_making_fin(getConv()));
			if (m_soType == KCPSOType::CLI_SO)
				this->changeState(State::FIN_WAIT);
			else
				this->changeState(State::CLOSE_WAIT);
		}
		// sever socket
		else if (m_kcpState == State::SYN_RECV)
		{
			this->changeState(State::CLOSED);
		}
		// client socket
		else if (m_kcpState == State::SYN_SENT)
		{
			this->changeState(State::CLOSED);
		}
	}
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
	this->changeState(State::SYN_RECV);
	this->m_randValue = synValue + 1;
	this->udpSendStr(kcp_making_syn_and_ack(m_randValue, conv));
	this->startIdle();
}

void KCPSocket::idleLogic()
{
	if (m_kcp && m_kcpState == State::ESTABLISHED)
	{
		ikcp_update(m_kcp, (IUINT32)(uv_now(m_loop) & 0xfffffffful));
	}

	if (uv_now(m_loop) - m_idleUpdateTime > 200ull)
	{
		m_idleUpdateTime = uv_now(m_loop);
		checkLogic();
	}
}

void KCPSocket::checkLogic()
{
	switch (m_kcpState)
	{
	case State::LISTEN:
		break;
	case State::STOP_LISTEN:
	{
		if (m_socketMng->count() <= 0)
			shutdownSocket();
		else
			m_socketMng->disconnectAll();
	}
	break;
	case State::SYN_SENT:
	{
		// connect timeout
		if (uv_now(m_loop) - m_stateTimeStamp > 3000)
			this->connectResult(2);
		else
			this->udpSendStr(kcp_making_syn(m_randValue));
	}
	break;
	case State::SYN_RECV:
	{
		if (uv_now(m_loop) - m_stateTimeStamp > 5000)
			this->acceptFailed();
		else
			this->udpSendStr(kcp_making_syn_and_ack(m_randValue, this->getConv()));
	}
	break;
	case State::ESTABLISHED:
	{
		if (m_soType == KCPSOType::CLI_SO && uv_now(m_loop) - m_stateTimeStamp < 2000)
			this->udpSendStr(kcp_making_ack(this->getConv() + 1));
	}
	break;
	case State::FIN_WAIT:
	case State::CLOSE_WAIT:
		if (uv_now(m_loop) - m_stateTimeStamp > 1000)
			this->shutdownSocket();
		else
			this->udpSendStr(kcp_making_fin(getConv()));
		break;
	default:
		this->shutdownSocket();
		break;
	}

	if (m_kcpState == State::ESTABLISHED && uv_now(m_loop) - m_lastKcpRecvTime > 6000U)
	{
		m_heartbeatLoseCount++;
		m_lastKcpRecvTime = uv_now(m_loop);

		// If there is no message communication more than one minute
		if (m_heartbeatLoseCount > 9)
		{
			this->disconnect();
			return;
		}

		if (m_soType == CLI_SO)
		{
			if (m_heartbeatLoseCount >= 2)
				this->udpSendStr(kcp_making_heart_packet());
		}
		else
		{
			if (m_heartbeatLoseCount >= 5)
				this->udpSendStr(kcp_making_heart_packet());
		}
	}
}

void KCPSocket::startIdle()
{
	if (m_idle == NULL)
	{
		m_idle = (UVTimer*)fc_malloc(sizeof(UVTimer));
		new (m_idle) UVTimer();
	}

	m_idleUpdateTime = uv_now(m_loop);
	m_idle->start(m_loop, [](uv_timer_t* handle)
	{
		KCPSocket* svr = (KCPSocket*)handle->data;
		svr->idleLogic();
	}, 1, 1, this);
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

void KCPSocket::shutdownSocket()
{
	this->releaseKcp();
	this->stopIdle();
	this->setConv(0);
	m_randValue = 0;
	
	if (m_weakRefTag)
	{
		m_udp = NULL;
		if (m_kcpState == State::CLOSE_WAIT && this->m_closeCall != nullptr)
			this->m_closeCall(this);

		this->changeState(State::CLOSED);

		auto mng = m_socketMng;
		m_socketMng = NULL;
		mng->remove(this);
	}
	else
	{
		if (m_udp)
		{
			uv_udp_recv_stop(m_udp);
			net_closeHandle((uv_handle_t*)m_udp, [](uv_handle_t* handle)
			{
				((KCPSocket*)handle->data)->onCloseSocketFinished(handle);
			});
		}
		else
		{
			this->changeState(State::CLOSED);
		}
	}
}

void KCPSocket::onCloseSocketFinished(uv_handle_t* handle)
{
	fc_free(handle);
	m_udp = NULL;
	if (m_kcpState == State::FIN_WAIT || m_kcpState == State::STOP_LISTEN)
	{
		if (this->m_closeCall != nullptr)
		{
			this->m_closeCall(this);
		}
	}
	this->changeState(State::CLOSED);
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
}

void KCPSocket::onUdpRead(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, uint32_t flags)
{
	if (m_soType == KCPSOType::SVR_SO)
	{
		if (m_kcpState == State::STOP_LISTEN) return;
		if (m_kcpState == State::LISTEN)
		{
			auto containSO = m_socketMng->getSocketBySockAddr(addr);
			if (kcp_is_syn(buf->base, nread))
			{
				if (containSO == NULL && m_connectFilterCall(addr))
				{
					uint32_t synValue = kcp_grab_syn_value(buf->base, nread);
					if (synValue > 0)
						this->accept(addr, synValue);
				}
			}
			else
			{
				if (containSO)
					containSO->onUdpRead(handle, nread, buf, addr, flags);
			}
			return;
		}
	}
	bool isKcp = false;
	if (kcp_is_syn(buf->base, nread)) {}
	else if (kcp_is_syn_and_ack(buf->base, nread)) 
	{
		if (m_soType == KCPSOType::CLI_SO && m_kcpState == State::SYN_SENT)
		{
			uint32_t value, conv;
			if (kcp_grab_syn_and_ack_value(buf->base, nread, &value, &conv) && value == m_randValue + 1)
			{
				this->initKcp(conv);
				this->changeState(State::ESTABLISHED);
				this->udpSendStr(kcp_making_ack(conv + 1));
				this->connectResult(1);
				this->resetCheckTimer();
			}
			else
			{
				this->connectResult(0);
			}
		}
	}
	else if (kcp_is_ack(buf->base, nread)) 
	{
		if (m_soType == KCPSOType::SVR_SO && m_kcpState == State::SYN_RECV)
		{
			uint32_t conv = kcp_grab_ack_value(buf->base, nread);
			if (this->getConv() == conv - 1)
			{
				this->changeState(State::ESTABLISHED);
				this->initKcp(this->getConv());
				m_socketMng->online(this);
				m_socketMng->getOwner()->m_newConnectionCall(this);
				this->resetCheckTimer();
			}
			else
			{
				this->acceptFailed();
			}
		}
	}
	else if (kcp_is_fin(buf->base, nread)) 
	{
		if (kcp_grab_fin_value(buf->base, nread) == this->getConv())
		{
			if (m_kcpState == State::ESTABLISHED)
				this->disconnect();
			else if (m_kcpState == State::FIN_WAIT)
				this->shutdownSocket();
			else if (m_kcpState == State::CLOSE_WAIT)
				this->shutdownSocket();
		}
	}
	else
	{
		if (m_kcpState == State::ESTABLISHED)
		{
			this->resetCheckTimer();
			if (kcp_is_heart_packet(buf->base, nread))
			{
				this->udpSendStr(kcp_making_heart_back_packet());
			}
			else if (kcp_is_heart_back_packet(buf->base, nread)) {}
			else
			{
				isKcp = true;
				this->kcpInput(buf->base, nread);
			}
		}
	}
}

void KCPSocket::connectResult(int32_t status)
{
	if (m_connectCall)
		m_connectCall(this, status);

	if (status != 1)
	{
		this->changeState(State::CLOSED);
		this->shutdownSocket();
	}
}

void KCPSocket::acceptFailed()
{
	this->changeState(State::CLOSED);
	this->shutdownSocket();
}

void KCPSocket::resetCheckTimer()
{
	m_lastKcpRecvTime = uv_now(m_loop);
	m_heartbeatLoseCount = 0;
}

void KCPSocket::changeState(State newState)
{
	m_kcpState = newState;
	m_stateTimeStamp = uv_now(m_loop);
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
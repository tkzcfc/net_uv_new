#include "UDPSocket.h"

NS_NET_UV_BEGIN

#define UDP_UV_SOCKET_RECV_BUF_LEN (65535)

UDPSocket::UDPSocket(uv_loop_t* loop)
	: m_udp(NULL)
	, m_readCall(nullptr)
{
	m_loop = loop;
}

UDPSocket::~UDPSocket()
{
	shutdownSocket();
}

bool UDPSocket::bind(const char* ip, uint32_t port)
{
	if (m_udp != NULL)
	{
		return 0;
	}

	this->setIp(ip);
	this->setPort(port);
	this->setIsIPV6(false);

	struct sockaddr_in bind_addr;
	int32_t r = uv_ip4_addr(ip, port, &bind_addr);

	if (r != 0)
	{
		return 0;
	}

	m_udp = (uv_udp_t*)fc_malloc(sizeof(uv_udp_t));
	r = uv_udp_init(m_loop, m_udp);
	CHECK_UV_ASSERT(r);
	m_udp->data = this;

	r = uv_udp_bind(m_udp, (const struct sockaddr*) &bind_addr, UV_UDP_REUSEADDR);

	if (r != 0)
	{
		return 0;
	}
	net_adjustBuffSize((uv_handle_t*)m_udp, UDP_UV_SOCKET_RECV_BUF_LEN, UDP_UV_SOCKET_RECV_BUF_LEN);

	r = uv_udp_recv_start(m_udp, uv_on_alloc_buffer, uv_on_after_read);
	if (r != 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "uv_udp_recv_start error: %s", net_getUVError(r).c_str());
		return false;
	}

	return true;
}

bool UDPSocket::bind6(const char* ip, uint32_t port)
{
	if (m_udp != NULL)
	{
		return 0;
	}

	this->setIp(ip);
	this->setPort(port);
	this->setIsIPV6(true);

	struct sockaddr_in6 bind_addr;
	int32_t r = uv_ip6_addr(ip, port, &bind_addr);

	if (r != 0)
	{
		return 0;
	}

	m_udp = (uv_udp_t*)fc_malloc(sizeof(uv_udp_t));
	r = uv_udp_init(m_loop, m_udp);
	CHECK_UV_ASSERT(r);
	m_udp->data = this;

	r = uv_udp_bind(m_udp, (const struct sockaddr*) &bind_addr, UV_UDP_REUSEADDR);

	if (r != 0)
	{
		return 0;
	}
	net_adjustBuffSize((uv_handle_t*)m_udp, UDP_UV_SOCKET_RECV_BUF_LEN, UDP_UV_SOCKET_RECV_BUF_LEN);

	r = uv_udp_recv_start(m_udp, uv_on_alloc_buffer, uv_on_after_read);
	if (r != 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "uv_udp_recv_start error: %s", net_getUVError(r).c_str());
		return false;
	}

	return true;
}

bool UDPSocket::listen(int32_t count)
{
	if (m_udp == NULL)
	{
		return false;
	}

	if (getPort() == 0)
	{
		setPort(net_udp_getPort(m_udp));
	}
	return true;
}

bool UDPSocket::connect(const char* ip, uint32_t port)
{
	return true;
}

bool UDPSocket::send(char* data, int32_t len)
{
	return false;
}

void UDPSocket::disconnect()
{}

bool UDPSocket::udpSend(const char* data, int32_t len, const struct sockaddr* addr)
{
	if (m_udp == NULL)
	{
		return false;
	}
	uv_buf_t* buf = (uv_buf_t*)fc_malloc(sizeof(uv_buf_t));
	buf->base = (char*)fc_malloc(len);
	buf->len = len;

	memcpy(buf->base, data, len);

	uv_udp_send_t* udp_send = (uv_udp_send_t*)fc_malloc(sizeof(uv_udp_send_t));
	udp_send->data = buf;
	int32_t r = uv_udp_send(udp_send, m_udp, buf, 1, addr, uv_on_udp_send);

	if (r != 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "udp send error %s", uv_strerror(r));
		return false;
	}
	return true;
}

void UDPSocket::shutdownSocket()
{
	if (m_udp == NULL)
		return;

	uv_udp_recv_stop(m_udp);
	net_closeHandle((uv_handle_t*)m_udp, net_closehandle_defaultcallback);
	m_udp = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UDPSocket::uv_on_udp_send(uv_udp_send_t *req, int status) 
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

void UDPSocket::uv_on_after_read(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{
	if (nread < 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "udp read error %s\n", uv_err_name(nread));
		uv_on_free_buffer((uv_handle_t*)handle, buf);
		return;
	}
	if (addr == NULL)
	{
		//NET_UV_LOG(NET_UV_L_ERROR, "addr is null");
		uv_on_free_buffer((uv_handle_t*)handle, buf);
		return;
	}

	if (nread > 0)
	{
		UDPSocket* s = (UDPSocket*)handle->data;
		s->m_readCall(handle, nread, buf, addr, flags);
	}
	uv_on_free_buffer((uv_handle_t*)handle, buf);
}

NS_NET_UV_END
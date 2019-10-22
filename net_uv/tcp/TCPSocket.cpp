#include "TCPSocket.h"

NS_NET_UV_BEGIN

TCPSocket::TCPSocket(uv_loop_t* loop)
	: m_newConnectionCall(nullptr)
	, m_tcp(NULL)
{
	m_loop = loop;
}

TCPSocket::~TCPSocket()
{
	if (m_tcp)
	{
		net_closeHandle((uv_handle_t*)m_tcp, net_closehandle_defaultcallback);
		m_tcp = NULL;
	}
}

bool TCPSocket::bind(const char* ip, uint32_t port)
{
	this->setIp(ip);
	this->setPort(port);

	struct sockaddr_in bind_addr;
	int32_t r = uv_ip4_addr(ip, port, &bind_addr);
	if (r != 0)
	{
		return false;
	}

	if (m_tcp == NULL)
	{
		m_tcp = (uv_tcp_t*)fc_malloc(sizeof(uv_tcp_t));
		int32_t r = uv_tcp_init(m_loop, m_tcp);
		CHECK_UV_ASSERT(r);

		m_tcp->data = this;
	}

	r = uv_tcp_bind(m_tcp, (const struct sockaddr*) &bind_addr, 0);

	return (r == 0);
}

bool TCPSocket::bind6(const char* ip, uint32_t port)
{
	this->setIp(ip);
	this->setPort(port);

	struct sockaddr_in6 bind_addr;
	int32_t r = uv_ip6_addr(ip, port, &bind_addr);
	if (r != 0)
	{
		return false;
	}

	if (m_tcp == NULL)
	{
		m_tcp = (uv_tcp_t*)fc_malloc(sizeof(uv_tcp_t));
		int32_t r = uv_tcp_init(m_loop, m_tcp);
		CHECK_UV_ASSERT(r);

		m_tcp->data = this;
	}

	r = uv_tcp_bind(m_tcp, (const struct sockaddr*) &bind_addr, 0);

	return (r == 0);
}

bool TCPSocket::listen(int32_t count)
{
	int32_t r = uv_listen((uv_stream_t *)m_tcp, count, server_on_after_new_connection);

	if (r == 0 && getPort() == 0)
	{
		setPort(net_tcp_getListenPort(m_tcp));
	}

	return (r == 0);
}

bool TCPSocket::connect(const char* ip, uint32_t port)
{
	uint32_t addr_len = 0;
	struct sockaddr* addr = net_getsocketAddr(ip, port, &addr_len);

	if (addr == NULL)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "[%s:%d]Failed to get address information", ip, port);
		return false;
	}

	this->setIp(ip);
	this->setPort(port);

	auto tcp = m_tcp;
	if (tcp == NULL)
	{
		tcp = (uv_tcp_t*)fc_malloc(sizeof(uv_tcp_t));
		int32_t r = uv_tcp_init(m_loop, tcp);
		CHECK_UV_ASSERT(r);

		tcp->data = this;
	}
	
	uv_connect_t* connectReq = (uv_connect_t*)fc_malloc(sizeof(uv_connect_t));
	connectReq->data = this;
	int32_t r = uv_tcp_connect(connectReq, tcp, addr, uv_on_after_connect);
	fc_free(addr);
	if (r)
	{
		return false;
	}
	setTcp(tcp);
	net_adjustBuffSize((uv_handle_t*)tcp, TCP_UV_SOCKET_RECV_BUF_LEN, TCP_UV_SOCKET_SEND_BUF_LEN);
	return true;
}

bool TCPSocket::send(char* data, int32_t len)
{
	if (m_tcp == NULL)
	{
		fc_free(data);
		return false;
	}

	uv_buf_t* buf = (uv_buf_t*)fc_malloc(sizeof(uv_buf_t));
	buf->base = (char*)data;
	buf->len = len;
	
	uv_write_t *req = (uv_write_t*)fc_malloc(sizeof(uv_write_t));
	req->data = buf;

	int32_t r = uv_write(req, (uv_stream_t*)m_tcp, buf, 1, uv_on_after_write);
	return (r == 0);
}

TCPSocket* TCPSocket::accept(uv_stream_t* server, int32_t status)
{
	if (status != 0)
	{
		return NULL;
	}
	uv_tcp_t *client = (uv_tcp_t*)fc_malloc(sizeof(uv_tcp_t));
	int32_t r = uv_tcp_init(m_loop, client);
	CHECK_UV_ASSERT(r);

	uv_stream_t *handle = (uv_stream_t*)client;

	r = uv_accept(server, handle);
	CHECK_UV_ASSERT(r);
	if (r != 0)
	{
		fc_free(client); 
		return NULL;
	}

	struct sockaddr* client_addr = net_tcp_getAddr((const uv_tcp_t*)handle);

	if (client_addr == NULL)
	{
		fc_free(client);
		return NULL;
	}
	std::string strip;
	uint32_t port;
	uint32_t socket_len = net_getsockAddrIPAndPort(client_addr, strip, port);
	bool isIPV6 = client_addr->sa_family == AF_INET6;

	fc_free(client_addr);

	if (socket_len == 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "TCP server failed to accept connection, address resolution failed");
		fc_free(client);
		return NULL;
	}

	net_adjustBuffSize((uv_handle_t*)client, TCP_UV_SOCKET_RECV_BUF_LEN, TCP_UV_SOCKET_SEND_BUF_LEN);

	TCPSocket* newSocket = (TCPSocket*)fc_malloc(sizeof(TCPSocket));
	new (newSocket) TCPSocket(m_loop);

	client->data = newSocket;

	newSocket->setTcp(client);
	newSocket->setIp(strip);
	newSocket->setPort(port);
	newSocket->setIsIPV6(isIPV6);

	r = uv_read_start(handle, uv_on_alloc_buffer, uv_on_after_read);
	if (r != 0)
	{
		newSocket->~TCPSocket();
		fc_free(newSocket);
		return NULL;
	}

	return newSocket;
}

void TCPSocket::disconnect()
{
	shutdownSocket();
}

void TCPSocket::shutdownSocket()
{
	if (m_tcp == NULL)
	{
		return;
	}
	net_closeHandle((uv_handle_t*)m_tcp, uv_on_close_socket);
	m_tcp = NULL;
}

void TCPSocket::uv_on_close_socket(uv_handle_t* socket)
{
	TCPSocket* s = (TCPSocket*)(socket->data);
	if (s->m_closeCall != nullptr)
	{
		s->m_closeCall(s);
	}
	fc_free(socket);
}

bool TCPSocket::setNoDelay(bool enable)
{
	if (m_tcp == NULL)
	{
		return false;
	}
	int32_t r = uv_tcp_nodelay(m_tcp, enable);
	return (r == 0);
}

bool TCPSocket::setKeepAlive(int32_t enable, uint32_t delay)
{
	if (m_tcp == NULL)
	{
		return false;
	}
	int32_t r = uv_tcp_keepalive(m_tcp, enable, delay);
	return (r == 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void TCPSocket::uv_on_after_connect(uv_connect_t* handle, int32_t status)
{
	TCPSocket* s = (TCPSocket*)handle->data;
	
	if (status == 0)
	{
		int32_t r = uv_read_start(handle->handle, uv_on_alloc_buffer, uv_on_after_read);
		if (r == 0)
		{
			s->m_connectCall(s, 1);
		}
		else
		{
			s->m_connectCall(s, 0);
		}
	}
	else
	{
		NET_UV_LOG(NET_UV_L_ERROR, "tcp connect error %s", uv_strerror(status));
		if (status == ETIMEDOUT)
		{
			s->m_connectCall(s, 2);
		}
		else
		{
			s->m_connectCall(s, 0);
		}
	}
	fc_free(handle);
}

void TCPSocket::server_on_after_new_connection(uv_stream_t *server, int32_t status) 
{
	if (status != 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "tcp new connection error %s", uv_strerror(status));
		return;
	}
	TCPSocket* s = (TCPSocket*)server->data;
	s->m_newConnectionCall(server, status);
}

void TCPSocket::uv_on_after_read(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf) 
{
	TCPSocket* s = (TCPSocket*)handle->data;
	if (nread <= 0) 
	{
		s->disconnect();
		uv_on_free_buffer((uv_handle_t*)handle, buf);
		return;
	}
	s->m_recvCall(buf->base, nread);
	uv_on_free_buffer((uv_handle_t*)handle, buf);
}

void TCPSocket::uv_on_after_write(uv_write_t* req, int32_t status)
{
	if (status != 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "tcp write error %s", uv_strerror(status));
	}
	uv_buf_t* buf = (uv_buf_t*)req->data;
	fc_free(buf->base);
	fc_free(buf);
	fc_free(req);
}

NS_NET_UV_END
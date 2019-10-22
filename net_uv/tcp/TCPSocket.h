#pragma once

#include "TCPCommon.h"

NS_NET_UV_BEGIN

using TCPSocketNewConnectionCall = std::function<void(uv_stream_t*, int)>;

class TCPSocket : public Socket
{
public:
	TCPSocket() = delete;
	TCPSocket(const TCPSocket&) = delete;
	TCPSocket(uv_loop_t* loop);

	virtual ~TCPSocket();

	virtual bool bind(const char* ip, unsigned int port)override;

	virtual bool bind6(const char* ip, unsigned int port)override;

	virtual bool listen(int32_t count)override;

	virtual bool connect(const char* ip, unsigned int port)override;

	virtual bool send(char* data, int len)override;

	TCPSocket* accept(uv_stream_t* server, int status);

	virtual void disconnect()override;

	bool setNoDelay(bool enable);

	bool setKeepAlive(int enable, unsigned int delay);
	
	inline void setNewConnectionCallback(const TCPSocketNewConnectionCall& call);

protected:
	void shutdownSocket();

	inline void setTcp(uv_tcp_t* tcp);

	inline uv_tcp_t* getTcp();

	inline uv_loop_t* getLoop();
	
	friend class TCPSession;
protected:

	static void uv_on_close_socket(uv_handle_t* socket);

	static void uv_on_after_connect(uv_connect_t* handle, int status);
	static void uv_on_after_write(uv_write_t* req, int status);
	static void uv_on_after_read(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf);
	static void server_on_after_new_connection(uv_stream_t *server, int status);

protected:
	uv_tcp_t* m_tcp;
	TCPSocketNewConnectionCall m_newConnectionCall;
};

void TCPSocket::setTcp(uv_tcp_t* tcp)
{
	m_tcp = tcp;
}

uv_tcp_t* TCPSocket::getTcp()
{
	return m_tcp;
}

uv_loop_t* TCPSocket::getLoop()
{
	return m_loop;
}

void TCPSocket::setNewConnectionCallback(const TCPSocketNewConnectionCall& call)
{
	m_newConnectionCall = std::move(call);
}

NS_NET_UV_END


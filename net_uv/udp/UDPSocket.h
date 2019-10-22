#pragma once

#include "../base/Misc.h"
#include "../base/Socket.h"

NS_NET_UV_BEGIN

using UDPReadCallback = std::function<void(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)>;

class UDPSocket : public Socket
{
public:
	UDPSocket() = delete;

	UDPSocket(const UDPSocket&) = delete;

	UDPSocket(uv_loop_t* loop);

	virtual ~UDPSocket();

	virtual bool bind(const char* ip, uint32_t port)override;

	virtual bool bind6(const char* ip, uint32_t port)override;

	virtual bool listen(int32_t count)override;

	virtual bool connect(const char* ip, uint32_t port)override;

	virtual bool send(char* data, int32_t len)override;

	virtual void disconnect()override;

	inline void setReadCallback(const UDPReadCallback& call);

	inline uv_udp_t* getUdp();

	bool udpSend(const char* data, int32_t len, const struct sockaddr* addr);

	void shutdownSocket();
	
protected:

	inline void setUdp(uv_udp_t* tcp);
	
protected:

	uv_udp_t* m_udp;
	UDPReadCallback m_readCall;
	
protected:
	static void uv_on_udp_send(uv_udp_send_t *req, int status);

	static void uv_on_after_read(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags);

};

void UDPSocket::setUdp(uv_udp_t* udp)
{
	m_udp = udp;
}

uv_udp_t* UDPSocket::getUdp()
{
	return m_udp;
}

void UDPSocket::setReadCallback(const UDPReadCallback& call)
{
	m_readCall = std::move(call);
}


NS_NET_UV_END


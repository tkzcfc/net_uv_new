#pragma once


#include "Common.h"
#include "uv.h"
#include "Buffer.h"

NS_NET_UV_BEGIN

class Socket;
using SocketConnectCall = std::function<void(Socket*,int32_t)>;	//0:failed 1:succeeded 2:timed out
using SocketCloseCall = std::function<void(Socket*)>; 
using SocketRecvCall = std::function<void(char*, ssize_t)>;

class Socket
{
public:
	Socket();
	virtual ~Socket();
	
	virtual bool bind(const char* ip, uint32_t port) = 0;

	virtual bool bind6(const char* ip, uint32_t port) = 0;

	virtual bool listen(int32_t count) = 0;

	virtual bool connect(const char* ip, uint32_t port) = 0;

	virtual bool send(char* data, int32_t len) = 0;

	virtual void disconnect() = 0;

	inline void setConnectCallback(const SocketConnectCall& call);

	inline void setCloseCallback(const SocketCloseCall& call);

	inline void setRecvCallback(const SocketRecvCall& call);
public:

	inline std::string getIp();

	inline uint32_t getPort();

	inline bool isIPV6();

	inline void setIp(const std::string& ip);

	inline void setPort(uint32_t port);

	inline void setIsIPV6(bool isIPV6);

	inline void setUserdata(void* userdata);

	inline void* getUserdata();

	inline uv_loop_t* getLoop();
	
protected:

	static void uv_on_alloc_buffer(uv_handle_t* handle, size_t  size, uv_buf_t* buf);

	static void uv_on_free_buffer(uv_handle_t* handle, const uv_buf_t* buf);

protected:
	uv_loop_t* m_loop;
	
	std::string m_ip;
	uint32_t m_port;
	bool m_isIPV6;

	void* m_userdata;

	SocketConnectCall m_connectCall;
	SocketCloseCall m_closeCall;
	SocketRecvCall m_recvCall;
};

std::string Socket::getIp()
{
	return m_ip;
}

void Socket::setIp(const std::string& ip)
{
	m_ip = ip;
}

uint32_t Socket::getPort()
{
	return m_port;
}

void Socket::setPort(uint32_t port)
{
	m_port = port;
}

bool Socket::isIPV6()
{
	return m_isIPV6;
}

void Socket::setIsIPV6(bool isIPV6)
{
	m_isIPV6 = isIPV6;
}

void Socket::setConnectCallback(const SocketConnectCall& call)
{
	m_connectCall = std::move(call);
}

void Socket::setCloseCallback(const SocketCloseCall& call)
{
	m_closeCall = std::move(call);
}

void Socket::setRecvCallback(const SocketRecvCall& call)
{
	m_recvCall = std::move(call);
}

void Socket::setUserdata(void* userdata)
{
	m_userdata = userdata;
}

void* Socket::getUserdata()
{
	return m_userdata;
}

uv_loop_t* Socket::getLoop()
{
	return m_loop;
}


NS_NET_UV_END
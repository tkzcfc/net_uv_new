#include "Socket.h"
#include "Loop.h"

NS_NET_UV_BEGIN

Socket::Socket()
	: m_port(0)
	, m_isIPV6(false)
	, m_loop(nullptr)
	, m_connectCall(nullptr)
	, m_closeCall(nullptr)
	, m_userdata(nullptr)
	, m_recvCall(nullptr)
{
	m_recvCall = [](char*, ssize_t) 
	{
		assert(0);
		NET_UV_LOG(NET_UV_L_FATAL, "No recipient");
	};
}

Socket::~Socket()
{
}

void Socket::uv_on_alloc_buffer(uv_handle_t* handle, size_t  size, uv_buf_t* buf)
{
	LoopData* loopData = (LoopData*)handle->loop->data;
	
	loopData->uvLoop->alloc_buf(buf, size);
}

void Socket::uv_on_free_buffer(uv_handle_t* handle, const uv_buf_t* buf)
{
	LoopData* loopData = (LoopData*)handle->loop->data;

	loopData->uvLoop->free_buf(buf);
}

NS_NET_UV_END
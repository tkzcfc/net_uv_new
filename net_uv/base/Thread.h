#pragma once

#include "Common.h"

NS_NET_UV_BEGIN

class Thread
{
public:

	int create(uv_thread_cb cb, void* arg)
	{
		return uv_thread_create(&m_thread, cb, arg);
	}

	void join()
	{
		uv_thread_join(&m_thread);
	}

private:

	uv_thread_t m_thread;

};

NS_NET_UV_END

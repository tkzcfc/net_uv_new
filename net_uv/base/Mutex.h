#pragma once

#include "Common.h"

NS_NET_UV_BEGIN

class Mutex
{
public:
	Mutex();
	Mutex(const Mutex&) = delete;

	~Mutex();

	inline void lock();

	inline void unlock();

	inline int32_t trylock();

protected:
	uv_mutex_t m_uv_mutext;
};

void Mutex::lock()
{
	uv_mutex_lock(&m_uv_mutext);
}

void Mutex::unlock()
{
	uv_mutex_unlock(&m_uv_mutext);
}

int32_t Mutex::trylock()
{
	return uv_mutex_trylock(&m_uv_mutext);
}


NS_NET_UV_END


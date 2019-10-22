#include "Mutex.h"


NS_NET_UV_BEGIN

Mutex::Mutex()
{
	uv_mutex_init(&m_uv_mutext);
}

Mutex::~Mutex()
{
	uv_mutex_destroy(&m_uv_mutext);
}

NS_NET_UV_END
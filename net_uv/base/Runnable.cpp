#include "Runnable.h"

NS_NET_UV_BEGIN

Runnable::Runnable()
{
}

Runnable::~Runnable()
{
}

void Runnable::startThread()
{
	m_thread.create([](void* arg)
	{
		Runnable* runnable = (Runnable*)arg;
		runnable->run();
	}, this);
}

void Runnable::join()
{
	m_thread.join();
}

void Runnable::pushThreadMsg(NetThreadMsgType type, Session* session, char* data, uint32_t len)
{
	NetThreadMsg msg;
	msg.msgType = type;
	msg.data = data;
	msg.dataLen = len;
	msg.pSession = session;

	m_msgMutex.lock();
	m_msgQue.push(msg);
	m_msgMutex.unlock();
}

bool Runnable::getThreadMsg()
{
	if (m_msgMutex.trylock() != 0)
	{
		return false;
	}

	if (m_msgQue.empty())
	{
		m_msgMutex.unlock();
		return false;
	}

	while (!m_msgQue.empty())
	{
		m_msgDispatchQue.push(m_msgQue.front());
		m_msgQue.pop();
	}
	m_msgMutex.unlock();

	return true;
}

void Runnable::startIdle()
{
	m_idle.start(m_loop.ptr(), [](uv_idle_t* handle)
	{
		Runnable* self = (Runnable*)handle->data;
		self->onIdleRun();
	}, this);
}

void Runnable::stopIdle()
{
	m_idle.stop();
}

void Runnable::startTimerUpdate(uint32_t time)
{
	m_updateTimer.start(m_loop.ptr(), [](uv_timer_t* handle)
	{
		Runnable* svr = (Runnable*)handle->data;
		svr->onTimerUpdateRun();
	}, time, time, this);
}

void Runnable::stopTimerUpdate()
{
	m_updateTimer.stop();
}


NS_NET_UV_END
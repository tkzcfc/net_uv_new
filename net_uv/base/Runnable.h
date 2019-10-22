#pragma once

#include "Common.h"
#include "Loop.h"
#include "Thread.h"
#include "Mutex.h"
#include "../common/NetUVThreadMsg.h"

NS_NET_UV_BEGIN

class Runnable
{
public:
	Runnable();

	virtual ~Runnable();

	void startThread();

	void join();

	inline void* getUserData();

	inline void setUserData(void* userData);

protected:

	virtual void run() = 0;

	virtual void onIdleRun() = 0;

	virtual void onTimerUpdateRun() = 0;

protected:
	void startIdle();

	void stopIdle();

	void startTimerUpdate(uint32_t time);

	void stopTimerUpdate();

	virtual void pushThreadMsg(NetThreadMsgType type, Session* session, char* data = NULL, uint32_t len = 0);

	bool getThreadMsg();

protected:

	// thread message
	Mutex m_msgMutex;
	std::queue<NetThreadMsg> m_msgQue;
	std::queue<NetThreadMsg> m_msgDispatchQue;

	UVLoop m_loop;
	UVIdle m_idle;
	UVTimer m_updateTimer;

	Thread m_thread;

	void* m_userData;
};

void* Runnable::getUserData()
{
	return m_userData;
}

void Runnable::setUserData(void* userData)
{
	m_userData = userData;
}


NS_NET_UV_END


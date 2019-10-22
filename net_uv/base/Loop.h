#pragma once

#include "Common.h"


NS_NET_UV_BEGIN

class UVLoop;
struct LoopData
{
	UVLoop* uvLoop;
};

class UVLoop
{
public:

	UVLoop();

	~UVLoop();

	uv_loop_t* ptr();

	void run(uv_run_mode mode);

	void stop();

	void close();

	void alloc_buf(uv_buf_t* buf, size_t size);

	void free_buf(const uv_buf_t* buf);

private:

	void clearLoopData();

private:

	uv_loop_t m_loop;
	LoopData m_data;

	std::list<uv_buf_t> m_freeReadBufList;
};


class UVIdle
{
public:

	UVIdle();

	~UVIdle();

	uv_idle_t* ptr();
	
	void start(uv_loop_t* loop, uv_idle_cb cb, void* data);

	void stop();

	void close(uv_close_cb cb);

	bool isRunning();

private:

	uv_idle_t m_idle;

	bool m_start;
};

class UVTimer
{
public:

	UVTimer();

	~UVTimer();

	uv_timer_t* ptr();

	void start(uv_loop_t* loop, uv_timer_cb cb, uint64_t timeout, uint64_t repeat, void* data);

	void stop();

	void close(uv_close_cb cb);

	bool isRunning();

private:

	uv_timer_t m_timer;

	bool m_start;
};

NS_NET_UV_END


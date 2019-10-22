#pragma once

#ifdef __cplusplus
#define NS_NET_UV_BEGIN	namespace net_uv {
#define NS_NET_UV_END	}
#define NS_NET_UV_OPEN using namespace net_uv;
#else
#define NS_NET_UV_BEGIN	
#define NS_NET_UV_END	
#define NS_NET_UV_OPEN
#endif

#if defined (WIN32) || defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib,"Psapi.lib")
#pragma comment(lib, "Userenv.lib")
#define ThreadSleep(ms) Sleep(ms);

#elif defined __linux__

#include <unistd.h>
#define ThreadSleep(ms) usleep((ms) * 1000)

#endif


#define NET_UV_INET_ADDRSTRLEN         16
#define NET_UV_INET6_ADDRSTRLEN        46


#define FC_SAFE_FREE(p) do { if(p != NULL){ fc_free(p); p = NULL; } } while (0);

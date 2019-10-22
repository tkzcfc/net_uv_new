#pragma once


// Log level
#define NET_UV_L_HEART	 (0)
#define NET_UV_L_INFO	 (1)
#define NET_UV_L_WARNING (2)
#define NET_UV_L_ERROR	 (3)
#define NET_UV_L_FATAL	 (4)


#if _DEBUG 

// enable debug mode
#define OPEN_NET_UV_DEBUG 1
// enable memory detection
#define OPEN_NET_MEM_CHECK 1
// log level
#define NET_UV_L_DEFAULT_LEVEL NET_UV_L_INFO

#else

// enable debug mode
#define OPEN_NET_UV_DEBUG 0
// enable memory detection
#define OPEN_NET_MEM_CHECK 0
// log level
#define NET_UV_L_DEFAULT_LEVEL NET_UV_L_ERROR

#endif



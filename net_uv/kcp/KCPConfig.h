#pragma once

NS_NET_UV_BEGIN

// socket minimum receive cache size
#define KCP_UV_SOCKET_RECV_BUF_LEN (1024 * 10)
// socket minimum send cache size
#define KCP_UV_SOCKET_SEND_BUF_LEN (1024 * 10)

#define KCP_MAX_MSG_SIZE (2048)
#define KCP_RECV_BUF_SIZE (4096)

NS_NET_UV_END
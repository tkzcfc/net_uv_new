#pragma once

#include "Common.h"

NS_NET_UV_BEGIN

std::string net_getUVError(int32_t errcode);

std::string net_getTime();

void net_alloc_buffer(uv_handle_t* handle, size_t  size, uv_buf_t* buf);

void net_closehandle_defaultcallback(uv_handle_t* handle);

void net_closeHandle(uv_handle_t* handle, uv_close_cb closecb);

// Adjust socket buffer size
void net_adjustBuffSize(uv_handle_t* handle, int32_t minRecvBufSize, int32_t minSendBufSize);

//hash
uint32_t net_getBufHash(const void *buf, uint32_t len);

uint32_t net_getsockAddrIPAndPort(const struct sockaddr* addr, std::string& outIP, uint32_t& outPort);

struct sockaddr* net_getsocketAddr(const char* ip, uint32_t port, uint32_t* outAddrLen);

struct sockaddr* net_getsocketAddr_no(const char* ip, uint32_t port, bool isIPV6, uint32_t* outAddrLen);

uint32_t net_getsockAddrPort(const struct sockaddr* addr);

struct sockaddr* net_tcp_getAddr(const uv_tcp_t* handle);

uint32_t net_tcp_getListenPort(const uv_tcp_t* handle);

uint32_t net_getAddrPort(const struct sockaddr* addr);

uint32_t net_udp_getPort(uv_udp_t* handle);

bool net_compareAddress(const struct sockaddr* addr1, const struct sockaddr* addr2);

struct sockaddr* net_copyAddress(const struct sockaddr* addr);

NS_NET_UV_END

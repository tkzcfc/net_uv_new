#pragma once

#include "KCPCommon.h"
#include <time.h>
#include "ikcp.h"

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#include <windows.h>
#elif !defined(__unix)
#define __unix
#endif

#ifdef __unix
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#endif

NS_NET_UV_BEGIN

// value = random
std::string kcp_making_syn(uint32_t value);
bool kcp_is_syn(const char* data, uint32_t len);
uint32_t kcp_grab_syn_value(const char* data, uint32_t len);


// value = syn.value + 1
// conv = kcp conv
std::string kcp_making_syn_and_ack(uint32_t value, uint32_t conv);
bool kcp_is_syn_and_ack(const char* data, uint32_t len);
bool kcp_grab_syn_and_ack_value(const char* data, uint32_t len, uint32_t* pValue, uint32_t* pConv);

// value = conv + 1
std::string kcp_making_ack(uint32_t value);
bool kcp_is_ack(const char* data, uint32_t len);
uint32_t kcp_grab_ack_value(const char* data, uint32_t len);

// conv
std::string kcp_making_fin(uint32_t conv);
bool kcp_is_fin(const char* data, uint32_t len);
uint32_t kcp_grab_fin_value(const char* data, uint32_t len);

std::string kcp_making_heart_packet();
bool kcp_is_heart_packet(const char* data, size_t len);

std::string kcp_making_heart_back_packet();
bool kcp_is_heart_back_packet(const char* data, size_t len);

NS_NET_UV_END


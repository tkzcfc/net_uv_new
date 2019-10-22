#include "Misc.h"
#include <time.h>
#include "DNSCache.h"

NS_NET_UV_BEGIN


std::string net_getUVError(int32_t errcode)
{
	if (0 == errcode)
	{
		return "";
	}
	std::string err;
	auto tmpChar = uv_err_name(errcode);
	if (tmpChar)
	{
		err = tmpChar;
		err += ":";
	}
	else
	{
		char szCode[16];
		sprintf(szCode, "%d:", errcode);
		err = "unknown system errcode ";
		err.append(szCode);
	}
	tmpChar = uv_strerror(errcode);
	if (tmpChar)
	{
		err += tmpChar;
	}
	return std::move(err);
}

std::string net_getTime()
{
	time_t timep;
	time(&timep);
	char tmp[64];
	strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&timep));
	return tmp;
}

void net_alloc_buffer(uv_handle_t* handle, size_t  size, uv_buf_t* buf)
{
	buf->base = (char*)fc_malloc(size);
	buf->len = size;
}

void net_closehandle_defaultcallback(uv_handle_t* handle)
{
	fc_free(handle);
}

void net_closeHandle(uv_handle_t* handle, uv_close_cb closecb)
{
	if (handle && !uv_is_closing(handle))
	{
		uv_close(handle, closecb);
	}
}

// 调整socket缓冲区大小
void net_adjustBuffSize(uv_handle_t* handle, int32_t minRecvBufSize, int32_t minSendBufSize)
{
	int32_t len = 0;
	int32_t r = uv_recv_buffer_size(handle, &len);
	CHECK_UV_ASSERT(r);

	if (len < minRecvBufSize)
	{
		len = minRecvBufSize;
		r = uv_recv_buffer_size(handle, &len);
		CHECK_UV_ASSERT(r);
	}

	len = 0;
	r = uv_send_buffer_size(handle, &len);
	CHECK_UV_ASSERT(r);

	if (len < minSendBufSize)
	{
		len = minSendBufSize;
		r = uv_send_buffer_size(handle, &len);
		CHECK_UV_ASSERT(r);
	}
}

// hash
uint32_t net_getBufHash(const void *buf, uint32_t len)
{
	uint32_t seed = 131; // 31 131 1313 13131 131313 etc..
	uint32_t hash = 0;
	uint32_t i = 0;
	char *str = (char *)buf;
	while (i < len)
	{
		hash = hash * seed + (*str++);
		++i;
	}

	return (hash & 0x7FFFFFFF);
}

uint32_t net_getsockAddrIPAndPort(const struct sockaddr* addr, std::string& outIP, uint32_t& outPort)
{
	if (addr == NULL)
		return 0;

	std::string strip;
	uint32_t addrlen = 0;
	uint32_t port = 0;

	if (addr->sa_family == AF_INET6)
	{
		addrlen = sizeof(struct sockaddr_in6);

		const struct sockaddr_in6* addr_in = (const struct sockaddr_in6*) addr;

		char szIp[NET_UV_INET6_ADDRSTRLEN + 1] = { 0 };
		int32_t r = uv_ip6_name(addr_in, szIp, NET_UV_INET6_ADDRSTRLEN);
		if (r != 0)
		{
			return 0;
		}

		strip = szIp;
		port = ntohs(addr_in->sin6_port);
	}
	else
	{
		addrlen = sizeof(struct sockaddr_in);

		const struct sockaddr_in* addr_in = (const struct sockaddr_in*) addr;

		char szIp[NET_UV_INET_ADDRSTRLEN + 1] = { 0 };
		int32_t r = uv_ip4_name(addr_in, szIp, NET_UV_INET_ADDRSTRLEN);
		if (r != 0)
		{
			return 0;
		}

		strip = szIp;
		port = ntohs(addr_in->sin_port);
	}

	outIP = strip;
	outPort = port;

	return addrlen;
}

struct sockaddr* net_getsocketAddr(const char* ip, uint32_t port, uint32_t* outAddrLen)
{
	uint32_t outValueLen = 0;
	auto outValue = DNSCache::getInstance()->rand_get(ip, &outValueLen);
	if (outValue != NULL && outValueLen > 0)
	{
		if (outValueLen == sizeof(struct sockaddr_in))
		{
			((struct sockaddr_in*)outValue)->sin_port = htons(port); 
			struct sockaddr* addr = (struct sockaddr*)fc_malloc(sizeof(struct sockaddr_in));
			memcpy(addr, outValue, sizeof(struct sockaddr_in));
			return addr;
		}
		else if (outValueLen == sizeof(struct sockaddr_in6))
		{
			((struct sockaddr_in6*)outValue)->sin6_port = htons(port);
			struct sockaddr* addr = (struct sockaddr*)fc_malloc(sizeof(struct sockaddr_in6));
			memcpy(addr, outValue, sizeof(struct sockaddr_in6));
			return addr;
		}
	}

	struct addrinfo hints;
	struct addrinfo* ainfo;
	struct addrinfo* rp;
	struct sockaddr_in* addr4 = NULL;
	struct sockaddr_in6* addr6 = NULL;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_STREAM;

	int ret = getaddrinfo(ip, NULL, &hints, &ainfo);

	if (ret == 0)
	{
		DNSCache::getInstance()->add(ip, ainfo);

		for (rp = ainfo; rp; rp = rp->ai_next)
		{
			if (rp->ai_family == AF_INET)
			{
				addr4 = (struct sockaddr_in*)rp->ai_addr;
				addr4->sin_port = htons(port);
				if (outAddrLen != NULL)
				{
					*outAddrLen = sizeof(struct sockaddr_in);
				}
				break;

			}
			else if (rp->ai_family == AF_INET6)
			{
				addr6 = (struct sockaddr_in6*)rp->ai_addr;
				addr6->sin6_port = htons(port);
				if (outAddrLen != NULL)
				{
					*outAddrLen = sizeof(struct sockaddr_in6);
				}
				break;
			}
			else
			{
				continue;
			}
		}

		struct sockaddr* addr = NULL;
		if (addr4)
		{
			addr = (struct sockaddr*)fc_malloc(sizeof(struct sockaddr_in));
			memcpy(addr, addr4, sizeof(struct sockaddr_in));
		}
		if (addr6)
		{
			addr = (struct sockaddr*)fc_malloc(sizeof(struct sockaddr_in6));
			memcpy(addr, addr6, sizeof(struct sockaddr_in6));
		}
		freeaddrinfo(ainfo);

		return addr;
	}
	return NULL;
}

struct sockaddr* net_getsocketAddr_no(const char* ip, uint32_t port, bool isIPV6, uint32_t* outAddrLen)
{
	if (outAddrLen)
	{
		*outAddrLen = 0;
	}

	if (isIPV6)
	{
		struct sockaddr_in6* send_addr = (struct sockaddr_in6*)fc_malloc(sizeof(struct sockaddr_in6));
		int32_t r = uv_ip6_addr(ip, port, send_addr);

		if (r != 0)
		{
			fc_free(send_addr);
			return NULL;
		}

		if (outAddrLen)
		{
			*outAddrLen = sizeof(struct sockaddr_in6);
		}

		return (struct sockaddr*)send_addr;
	}
	struct sockaddr_in* send_addr = (struct sockaddr_in*)fc_malloc(sizeof(struct sockaddr_in));
	int32_t r = uv_ip4_addr(ip, port, send_addr);

	if (r != 0)
	{
		fc_free(send_addr);
		return NULL;
	}

	if (outAddrLen)
	{
		*outAddrLen = sizeof(struct sockaddr_in6);
	}

	return (struct sockaddr*)send_addr;
}

uint32_t net_getsockAddrPort(const struct sockaddr* addr)
{
	if (addr->sa_family == AF_INET6)
	{
		const struct sockaddr_in6* addr_in = (const struct sockaddr_in6*) addr;
		return ntohs(addr_in->sin6_port);
	}
	const struct sockaddr_in* addr_in = (const struct sockaddr_in*) addr;
	return ntohs(addr_in->sin_port);
}

struct sockaddr* net_tcp_getAddr(const uv_tcp_t* handle)
{

	//有的机子调用uv_tcp_getpeername报错
	//sockaddr_in client_addr;改为 sockaddr_in client_addr[2];
	//https://blog.csdn.net/readyisme/article/details/28249883
	//http://msdn.microsoft.com/en-us/library/ms737524(VS.85).aspx
	//
	//The buffer size for the local and remote address must be 16 bytes more than the size of the sockaddr structure for 
	//the transport protocol in use because the addresses are written in an internal format. For example, the size of a 
	//sockaddr_in (the address structure for TCP/IP) is 16 bytes. Therefore, a buffer size of at least 32 bytes must be 
	//specified for the local and remote addresses.

	int32_t client_addr_length = sizeof(sockaddr_in6) * 2;
	struct sockaddr* client_addr = (struct sockaddr*)fc_malloc(client_addr_length);
	memset(client_addr, 0, client_addr_length);

	int32_t r = uv_tcp_getpeername(handle, client_addr, &client_addr_length);

	if (r != 0)
	{
		fc_free(client_addr);
		return NULL;
	}
	return client_addr;
}

uint32_t net_tcp_getListenPort(const uv_tcp_t* handle)
{
	int32_t client_addr_length = sizeof(sockaddr_in6) * 2;
	struct sockaddr* client_addr = (struct sockaddr*)fc_malloc(client_addr_length);
	memset(client_addr, 0, client_addr_length);

	int32_t r = uv_tcp_getsockname(handle, client_addr, &client_addr_length);

	uint32_t outPort = 0;

	if (r == 0)
	{
		if (client_addr->sa_family == AF_INET6)
		{
			const struct sockaddr_in6* addr_in = (const struct sockaddr_in6*) client_addr;
			outPort = ntohs(addr_in->sin6_port);
		}
		else
		{
			const struct sockaddr_in* addr_in = (const struct sockaddr_in*) client_addr;
			outPort = ntohs(addr_in->sin_port);
		}
	}
	fc_free(client_addr);

	return outPort;
}

uint32_t net_getAddrPort(const struct sockaddr* addr)
{
	uint32_t outPort = 0;
	if (addr->sa_family == AF_INET6)
	{
		const struct sockaddr_in6* addr_in = (const struct sockaddr_in6*) addr;
		outPort = ntohs(addr_in->sin6_port);
	}
	else
	{
		const struct sockaddr_in* addr_in = (const struct sockaddr_in*) addr;
		outPort = ntohs(addr_in->sin_port);
	}
	return outPort;
}

uint32_t net_udp_getPort(uv_udp_t* handle)
{
	int32_t client_addr_length = sizeof(sockaddr_in6) * 2;
	struct sockaddr* client_addr = (struct sockaddr*)fc_malloc(client_addr_length);
	memset(client_addr, 0, client_addr_length);

	int32_t r = uv_udp_getsockname(handle, client_addr, &client_addr_length);
	CHECK_UV_ASSERT(r);

	uint32_t outPort = 0;

	if (r == 0)
	{
		if (client_addr->sa_family == AF_INET6)
		{
			const struct sockaddr_in6* addr_in = (const struct sockaddr_in6*) client_addr;
			outPort = ntohs(addr_in->sin6_port);
		}
		else
		{
			const struct sockaddr_in* addr_in = (const struct sockaddr_in*) client_addr;
			outPort = ntohs(addr_in->sin_port);
		}
	}
	fc_free(client_addr);

	return outPort;
}

bool net_compareAddress(const struct sockaddr* addr1, const struct sockaddr* addr2)
{
	if (addr1 == NULL || addr2 == NULL)
		return false;

	if (addr1 == addr2)
		return true;

	if (addr1->sa_family != addr2->sa_family)
	{
		return false;
	}

	if (addr1->sa_family == AF_INET6)
	{
		const struct sockaddr_in6* addr1_6 = (const struct sockaddr_in6*) addr1;
		const struct sockaddr_in6* addr2_6 = (const struct sockaddr_in6*) addr2;

		if (addr1_6->sin6_port != addr2_6->sin6_port)
		{
			return false;
		}

		return memcmp(&addr1_6->sin6_addr, &addr2_6->sin6_addr, sizeof(addr1_6->sin6_addr)) == 0;
	}
	else if (addr1->sa_family == AF_INET)
	{
		const struct sockaddr_in* addr_in_1 = (const struct sockaddr_in*) addr1;
		const struct sockaddr_in* addr_in_2 = (const struct sockaddr_in*) addr2;

		if (addr_in_1->sin_port != addr_in_2->sin_port)
		{
			return false;
		}

		return memcmp(&addr_in_1->sin_addr, &addr_in_2->sin_addr, sizeof(addr_in_1->sin_addr)) == 0;
	}
	else
	{
		return false;
		//return memcmp(addr1, addr2, sizeof(struct sockaddr)) == 0;
	}
}


struct sockaddr* net_copyAddress(const struct sockaddr* addr)
{
	if (addr->sa_family == AF_INET)
	{
		struct sockaddr* out = (struct sockaddr*)fc_malloc(sizeof(struct sockaddr_in));
		memcpy(out, addr, sizeof(struct sockaddr_in));
		return out;
	}
	else if (addr->sa_family == AF_INET6)
	{
		struct sockaddr* out = (struct sockaddr*)fc_malloc(sizeof(struct sockaddr_in6));
		memcpy(out, addr, sizeof(struct sockaddr_in6));
		return out;
	}
	return NULL;
}

NS_NET_UV_END

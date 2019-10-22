#pragma once

#include "Common.h"
#include "Mutex.h"
#include <unordered_map>

NS_NET_UV_BEGIN

class DNSCache
{
public:

	static DNSCache* getInstance();

	DNSCache();

	~DNSCache();
	
	void add(const std::string& addr, struct addrinfo* ainfo);

	struct sockaddr* rand_get(const std::string& addr, uint32_t* outAddrLen);

	void clearCache(const std::string& addr);

	void clearAll();

	inline void setEnable(bool isEnable);

	inline bool isEnable();

protected:

	struct AddrCache 
	{
		char addrData[64];
		uint32_t addrlen;
	};
	struct CacheData
	{
		std::vector<AddrCache> addrArr;
		uint64_t lastTime;
	};
	std::unordered_map<std::string, CacheData > m_cacheMap;

	Mutex m_cacheLock;

	uint64_t m_lastClearTime;

	bool m_isEnable;
};

void DNSCache::setEnable(bool isEnable)
{
	m_isEnable = isEnable;
}

bool DNSCache::isEnable()
{
	return m_isEnable;
}

NS_NET_UV_END

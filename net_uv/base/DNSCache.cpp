#include "DNSCache.h"
#include <time.h>
#include <stdlib.h>
#include <iostream>

NS_NET_UV_BEGIN

static DNSCache DNSInstance;

DNSCache* DNSCache::getInstance()
{
	return &DNSInstance;
}

DNSCache::DNSCache()
	: m_lastClearTime(0U)
	, m_isEnable(false)
{}

DNSCache::~DNSCache()
{}

void DNSCache::add(const std::string& addr, struct addrinfo* ainfo)
{
	if (!m_isEnable)
	{
		return;
	}

	uint64_t curtime = time(NULL);

	m_cacheLock.lock();
	auto it = m_cacheMap.find(addr);
	if (it == m_cacheMap.end())
	{
		CacheData cachedata;
		cachedata.addrArr.reserve(3);
		m_cacheMap.insert(std::make_pair(addr, cachedata));
	}
	it = m_cacheMap.find(addr);
	it->second.lastTime = curtime;
	m_cacheLock.unlock();

	struct addrinfo* rp;
	AddrCache cache;

	for (rp = ainfo; rp; rp = rp->ai_next)
	{
		memset(&cache, 0, sizeof(cache));

		if (rp->ai_family == AF_INET)
		{
			cache.addrlen = sizeof(struct sockaddr_in);
			memcpy(cache.addrData, rp->ai_addr, sizeof(struct sockaddr_in));

			m_cacheLock.lock();
			it->second.addrArr.emplace_back(cache);
			m_cacheLock.unlock();
		}
		else if (rp->ai_family == AF_INET6)
		{
			cache.addrlen = sizeof(struct sockaddr_in6);
			memcpy(cache.addrData, rp->ai_addr, sizeof(struct sockaddr_in6));

			m_cacheLock.lock();
			it->second.addrArr.emplace_back(cache);
			m_cacheLock.unlock();
		}
		else
		{
			continue;
		}
	}

	if (curtime - m_lastClearTime > 60)
	{
		m_lastClearTime = curtime;
		
		m_cacheLock.lock();
		for (auto it = m_cacheMap.begin(); it != m_cacheMap.end(); )
		{
			if (curtime - it->second.lastTime >= 600)
			{
				it = m_cacheMap.erase(it);
			}
			else
			{
				++it;
			}
		}
		m_cacheLock.unlock();
	}
}

struct sockaddr* DNSCache::rand_get(const std::string& addr, uint32_t* outAddrLen)
{
	if (!m_isEnable)
	{
		if (outAddrLen != NULL)
		{
			*outAddrLen = 0;
		}
		return NULL;
	}

	m_cacheLock.lock();

	auto it = m_cacheMap.find(addr);
	if (it == m_cacheMap.end() || it->second.addrArr.empty())
	{
		if (outAddrLen != NULL)
		{
			*outAddrLen = 0;
		}
		m_cacheLock.unlock();
		return NULL;
	}
	
	auto& data = it->second.addrArr[rand() % it->second.addrArr.size()];
	it->second.lastTime = time(NULL);

	m_cacheLock.unlock();

	if (outAddrLen != NULL)
	{
		*outAddrLen = data.addrlen;
	}
	return (struct sockaddr*)data.addrData;
}

void DNSCache::clearCache(const std::string& addr)
{
	m_cacheLock.lock();
	m_cacheMap.erase(addr);
	m_cacheLock.unlock();
}

void DNSCache::clearAll()
{
	m_cacheLock.lock();
	m_cacheMap.clear();
	m_cacheLock.unlock();
}

NS_NET_UV_END

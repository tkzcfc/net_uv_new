#include "Common.h"
#include "Misc.h"

#if OPEN_NET_MEM_CHECK == 1
#include "Mutex.h"
#endif

NS_NET_UV_BEGIN

typedef void(*uvOutputLoggerType)(int32_t, const char*);
uvOutputLoggerType uvOutputLogger = 0;
int32_t uvLoggerLevel = NET_UV_L_DEFAULT_LEVEL;


static const char* net_uv_log_name[NET_UV_L_FATAL + 1] =
{
	"HEART",
	"INFO",
	"WARNING",
	"ERROR",
	"FATAL"
};

void net_uvLog(int32_t level, const char* format, ...)
{
	if (level < uvLoggerLevel)
	{
		return;
	}

	va_list args;
	char buf[1024];

	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	if (uvOutputLogger != NULL)
	{
		uvOutputLogger(level, buf);
	}
	else
	{
		std::string str = net_getTime();
		str.append("[NET-UV]-[");
		str.append(net_uv_log_name[level]);
		str.append("] ");
		str.append(buf);
		str.append("\n");
		printf("%s", str.c_str());
	}

	//va_list list;
	//va_start(list, format);
	//vprintf(format, list);
	//va_end(list);
	//printf("\n");
}

void net_setLogLevel(int32_t level)
{
	uvLoggerLevel = level;
}

void setNetUVLogPrintFunc(void(*func)(int32_t, const char*))
{
	uvOutputLogger = func;
}




///////////////////////////////////////////////////////////////////////////////////////////////////
#if OPEN_NET_MEM_CHECK == 1
struct mallocBlockInfo
{
	uint32_t len;
	std::string file;
	int line;
};

Mutex block_mutex;
uint32_t block_size = 0;
std::map<void*, mallocBlockInfo> block_map;

void* fc_malloc_s(uint32_t len, const char* file, int32_t line)
{
	mallocBlockInfo info;
	info.file = file;
	info.line = line;
	info.len = len;

	void* p = malloc(len);

	if (p == NULL)
	{
		NET_UV_LOG(NET_UV_L_FATAL, "Failed to request memory!!!");
#if defined (WIN32) || defined(_WIN32)
		MessageBox(NULL, TEXT("Failed to request memory!!!"), TEXT("ERROR"), MB_OK);
		assert(0);
#else
		printf("Failed to request memory!!!\n");
		assert(0);
#endif
		return NULL;
	}

	block_mutex.lock();

	block_size++;
	block_map.insert(std::make_pair(p, info));

	block_mutex.unlock();

	return p;
}

void fc_free(void* p)
{
	if (p == NULL)
	{
		//assert(0);
		return;
	}
	block_mutex.lock();

	auto it = block_map.find(p);
	if (it == block_map.end())
	{
		//assert(p == NULL);
		NET_UV_LOG(NET_UV_L_WARNING, "fc_free: [%p] not find", p);
	}
	else
	{
		block_size--;
		block_map.erase(it);
	}
	free(p);

	block_mutex.unlock();
}



void printMemInfo()
{
	block_mutex.lock();
	NET_UV_LOG(NET_UV_L_INFO, "block size = %d\n", block_size);
	auto it = block_map.begin();
	for (; it != block_map.end(); ++it)
	{
		NET_UV_LOG(NET_UV_L_INFO, "[%p] : [%d] [%s  %d]\n", it->first, it->second.len, it->second.file.c_str(), it->second.line);
	}
	block_mutex.unlock();
}

#else
#endif

NS_NET_UV_END


#include "net_uv/net_uv.h"
#include <iostream>

#include "DbgHelp.h"
#pragma comment(lib, "DbgHelp.lib")



#define NET_UV_USE_LOCAL_SVR 1

// tcp
#if NET_UV_USE_LOCAL_SVR 
#define TCP_CONNECT_IP "127.0.0.1"
#else
#define TCP_CONNECT_IP "www.kurumi.xin"
#endif
#define TCP_CONNECT_PORT 1001

// kcp
#if NET_UV_USE_LOCAL_SVR 
#define KCP_CONNECT_IP "127.0.0.1"
#else
#define KCP_CONNECT_IP "www.kurumi.xin"
#endif
#define KCP_CONNECT_PORT 1002

// p2p
#if NET_UV_USE_LOCAL_SVR 
#define P2P_CONNECT_IP "127.0.0.1"
#else
#define P2P_CONNECT_IP "www.kurumi.xin"
#endif
#define P2P_CONNECT_PORT 1003

NS_NET_UV_OPEN

// dump文件
LPCWSTR DumpFileName = TEXT("test.dmp");

// 创建dump文件
void CreateDempFile(LPCWSTR lpstrDumpFilePathName, EXCEPTION_POINTERS* pException)
{
	HANDLE hDumpFile = CreateFile(lpstrDumpFilePathName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	//dump信息
	MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
	dumpInfo.ExceptionPointers = pException;
	dumpInfo.ThreadId = GetCurrentThreadId();
	dumpInfo.ClientPointers = TRUE;
	// 写入dump文件内容
	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &dumpInfo, NULL, NULL);
	CloseHandle(hDumpFile);
}

// 处理Unhandled Exception的回调函数
LONG ApplicationCrashHandler(EXCEPTION_POINTERS* pException)
{
	CreateDempFile(DumpFileName, pException);
	return EXCEPTION_EXECUTE_HANDLER;
}

//SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);

void* on_uv_malloc_func(size_t size)
{
	return fc_malloc(size);
}

void on_uv_free_func(void* ptr)
{
	fc_free(ptr);
}


#define REGISTER_EXCEPTION(FileName) SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler); \
	DumpFileName = TEXT(FileName)

//uv_replace_allocator(on_uv_malloc_func, realloc, calloc, on_uv_free_func); \


#define CMD_STRCMP(v) (strcmp(cmd, v) == 0)

#define CMD_STR_STR(v) (strstr(cmdBuf, v))

#define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1:0)


void sendString(Session* s, char* szMsg)
{
	s->send(szMsg, strlen(szMsg));
}
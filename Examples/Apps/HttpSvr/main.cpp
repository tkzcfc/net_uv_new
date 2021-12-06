#include "net_uv.h"
#include "Exception.h"

using namespace net_uv;

bool runLoop = true;

int main(int argc, const char*argv[])
{
	Exception::registerException("httpSvr.dmp");

	int32_t port = 1002;
	if (argc >= 2)
	{
		port = std::atoi(argv[1]);
	}

	HttpServer* svr = new HttpServer();
	svr->startServer("0.0.0.0", port, false, 0xffff);
	svr->setHttpCallback([=](const HttpRequest& request, HttpResponse* response) 
	{
		if (request.path() == "/close")
		{
			svr->stopServer();
		}
		printf("\n\n\n");
		printf("query:%s\n", request.query().c_str());
		printf("path:%s\n", request.path().c_str());

		for (auto& it : request.headers())
		{
			printf("header:%s=%s\n", it.field.c_str(), it.value.c_str());
		}

		response->setBody("ok");
	});

	svr->setCloseCallback([]() 
	{
		runLoop = false;
	});

	while (runLoop)
	{
		svr->updateFrame();
		Sleep(1);
	}

	delete svr;

	printMemInfo();

	system("pause");

	return 0;
}


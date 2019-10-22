#include "net_uv.h"
#include "Exception.h"

using namespace net_uv;

Session* controlClient = NULL;
bool gServerStop = false;
std::vector<Session*> allSession;

int main(int argc, const char*argv[])
{
	Exception::registerException("svr.dump");

	int32_t port = 1000;
	int32_t svrType = 0;
	if (argc >= 2)
	{
		port = std::atoi(argv[1]);
	}
	if (argc >= 3)
	{
		svrType = std::atoi(argv[2]);
	}

	Server* svr = NULL;
	
	if (svrType == 0)
	{
		svr = new TCPServer();
	}
	else
	{
		svr = new KCPServer();
	}

	svr->setCloseCallback([](Server* svr)
	{
		printf("svr closed\n");
		gServerStop = true;
	});

	svr->setNewConnectCallback([](Server* svr, Session* session)
	{
		allSession.push_back(session);
		printf("[%d] %s:%d join svr\n", session->getSessionID(), session->getIp().c_str(), session->getPort());
	});

	svr->setRecvCallback([=](Server* svr, Session* session, char* data, uint32_t len)
	{
		char* msg = (char*)fc_malloc(len + 1);
		memcpy(msg, data, len);
		msg[len] = '\0';

		if (strcmp(msg, "control") == 0)
		{
			session->send("conrol client", strlen("conrol client"));
			controlClient = session;
		}
		else if (controlClient == session && strcmp(msg, "close") == 0)
		{
			svr->stopServer();
		}
		else if (controlClient == session && strcmp(msg, "print") == 0)
		{
			printMemInfo();
		}
		else if (controlClient == session && strstr(msg, "send"))
		{
			char* sendbegin = strstr(msg, "send") + 4;
			for (auto &it : allSession)
			{
				if (it != controlClient)
				{
					it->send(sendbegin, strlen(sendbegin));
				}
			}
		}
		else
		{
			session->send(data, len);
		}
		fc_free(msg);

		//printf("%s:%d received %d bytes\n", session->getIp().c_str(), session->getPort(), len);
	});

	svr->setDisconnectCallback([=](Server* svr, Session* session)
	{
		auto it = std::find(allSession.begin(), allSession.end(), session);
		if (it != allSession.end())
		{
			allSession.erase(it);
		}
		printf("[%d] %s:%d leave svr\n", session->getSessionID(), session->getIp().c_str(), session->getPort());
		if (session == controlClient)
		{
			controlClient = NULL;
		}
	});

	bool issuc = svr->startServer("0.0.0.0", port, false, 0xFFFF);
	if (issuc)
	{
		printf("svr is started\n");
	}
	else
	{
		printf("svr failed to start\n");
		gServerStop = true;
	}

	while (!gServerStop)
	{
		svr->updateFrame();
		ThreadSleep(1);
	}

	delete svr;

	printf("-----------------------------\n");
	printMemInfo();
	printf("\n-----------------------------\n");

	system("pause");

	return 0;
}


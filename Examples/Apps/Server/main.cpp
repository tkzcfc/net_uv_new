#include "net_uv.h"
#include "Exception.h"

using namespace net_uv;

int32_t controlClient = -1;
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
	NetMsgMgr* msgMng = nullptr;
	
	if (svrType == 0)
	{
		svr = new TCPServer();
	}
	else
	{
		svr = new KCPServer();
	}


	msgMng = new NetMsgMgr();
	msgMng->setUserData(svr);
	msgMng->setCloseSctCallback([](NetMsgMgr* mgr, uint32_t sessionID)
	{
		((Server*)mgr->getUserData())->disconnect(sessionID);
	});

	msgMng->setSendCallback([](NetMsgMgr* mgr, uint32_t sessionID, char* data, uint32_t len)
	{
		((Server*)mgr->getUserData())->sendEx(sessionID, data, len);
	});

	msgMng->setOnMsgCallback([](NetMsgMgr* mgr, uint32_t sessionID, char* data, uint32_t len)
	{
		char* msg = (char*)fc_malloc(len + 1);
		memcpy(msg, data, len);
		msg[len] = '\0';

		if (strcmp(msg, "control") == 0)
		{
			mgr->sendMsg(sessionID, "conrol client", strlen("conrol client"));
			controlClient = sessionID;
		}
		else if (controlClient == sessionID && strcmp(msg, "close") == 0)
		{
			((Server*)mgr->getUserData())->stopServer();
		}
		else if (controlClient == sessionID && strcmp(msg, "print") == 0)
		{
			printMemInfo();
		}
		else if (controlClient == sessionID && strstr(msg, "send"))
		{
			char* sendbegin = strstr(msg, "send") + 4;
			for (auto &it : allSession)
			{
				if (it->getSessionID() != controlClient)
				{
					mgr->sendMsg(it->getSessionID(), sendbegin, strlen(sendbegin));
				}
			}
		}
		else
		{
			mgr->sendMsg(sessionID, data, len);
		}
		fc_free(msg);

		//printf("%s:%d received %d bytes\n", session->getIp().c_str(), session->getPort(), len);
	});


	svr->setCloseCallback([=](Server* svr)
	{
		printf("svr closed\n");
		gServerStop = true;
		delete msgMng;
	});

	svr->setNewConnectCallback([=](Server* svr, Session* session)
	{
		msgMng->onConnect(session->getSessionID());
		allSession.push_back(session);
		printf("[%d] %s:%d join svr\n", session->getSessionID(), session->getIp().c_str(), session->getPort());
	});

	svr->setRecvCallback([=](Server* svr, Session* session, char* data, uint32_t len)
	{
		msgMng->onBuff(session->getSessionID(), data, len);
	});

	svr->setDisconnectCallback([=](Server* svr, Session* session)
	{
		auto it = std::find(allSession.begin(), allSession.end(), session);
		if (it != allSession.end())
		{
			allSession.erase(it);
		}
		printf("[%d] %s:%d leave svr\n", session->getSessionID(), session->getIp().c_str(), session->getPort());
		if (session->getSessionID() == controlClient)
		{
			controlClient = -1;
		}
		msgMng->onDisconnect(session->getSessionID());
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


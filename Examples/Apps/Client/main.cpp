#include "net_uv.h"
#include "Exception.h"

using namespace net_uv;


bool autosend = true;
bool autoDisconnect = true;
uint32_t keyIndex = 0;
char szWriteBuf[1024] = { 0 };
std::string szIP = "127.0.0.1";
int32_t port = 1000;

// 命令解析
bool cmdResolve(char* cmd, uint32_t key);

Client* client = nullptr;
NetMsgMgr* msgMng = nullptr;

int main(int argc, const char*argv[])
{
	Exception::registerException("client.dmp");

	int32_t svrType = 0;
	if (argc >= 4)
	{
		szIP = argv[1];
		port = std::atoi(argv[2]);
		svrType = std::atoi(argv[3]);
	}

	if (svrType == 0)
	{
		client = new TCPClient();
	}
	else
	{
		client = new KCPClient();
	}

	msgMng = new NetMsgMgr();
	msgMng->setUserData(client);
	msgMng->setCloseSctCallback([](NetMsgMgr* mgr, uint32_t sessionID) 
	{
		((Client*)mgr->getUserData())->disconnect(sessionID);
	});

	msgMng->setSendCallback([](NetMsgMgr* mgr, uint32_t sessionID, char* data, uint32_t len) 
	{
		((Client*)mgr->getUserData())->sendEx(sessionID, data, len);
	});

	msgMng->setOnMsgCallback([](NetMsgMgr* mgr, uint32_t sessionID, char* data, uint32_t len) 
	{
		char* msg = (char*)fc_malloc(len + 1);
		memcpy(msg, data, len);
		msg[len] = '\0';

		if (!cmdResolve(msg, sessionID))
		{
			if (len < 100)
			{
				sprintf(szWriteBuf, "this is %d send data...", sessionID);
				if (strcmp(szWriteBuf, msg) != 0)
				{
					printf("Illegal message received:[%s]\n", msg);
				}
			}
			else
			{
				printf("[%d] received %d bytes\n", sessionID, len);
			}
		}
		/*	if (autoDisconnect && rand() % 100 == 0)
		{
		session->disconnect();
		}*/
		fc_free(msg);
	});



	client->setClientCloseCallback([](Client*)
	{
		printf("client closed\n");
		delete msgMng;
	});

	client->setConnectCallback([=](Client*, Session* session, int32_t status)
	{
		if (status == 0)
		{
			printf("[%d]connect failed\n", session->getSessionID());
		}
		else if (status == 1)
		{
			printf("[%d]connected\n", session->getSessionID());
			msgMng->onConnect(session->getSessionID());
		}
		else if (status == 2)
		{
			printf("[%d]connect timed out\n", session->getSessionID());
		}
	});

	client->setDisconnectCallback([=](Client*, Session* session) {
		printf("[%d]disconnect\n", session->getSessionID());
		msgMng->onDisconnect(session->getSessionID());
		//client->removeSession(session->getSessionID());
	});

	client->setRemoveSessionCallback([](Client*, Session* session) {
		printf("[%d]remove session\n", session->getSessionID());
	});

	client->setRecvCallback([](Client*, Session* session, char* data, uint32_t len)
	{
		msgMng->onBuff(session->getSessionID(), data, len);
	});

	for (int32_t i = 0; i < 10; ++i)
	{
		client->connect(szIP.c_str(), port, keyIndex++);
	}

	int32_t curCount = 0;
	while (!client->isCloseFinish())
	{
		client->updateFrame();

		//Auto Send
		if (autosend)
		{
			curCount++;
			if (curCount > 4000)
			{
				for (int32_t i = 0; i < keyIndex; ++i)
				{
					sprintf(szWriteBuf, "this is %d send data...", i);
					if (msgMng)
					{
						msgMng->sendMsg(i, szWriteBuf, (uint32_t)strlen(szWriteBuf));
					}
				}
				curCount = 0;
			}
		}

		ThreadSleep(1);
	}

	delete client;

	printMemInfo();

	system("pause");
	return 0;
}


#define CMD_STRCMP(v) (strcmp(cmd, v) == 0)

bool cmdResolve(char* cmd, uint32_t key)
{
	if (CMD_STRCMP("print"))
	{
		//Print memory information
		printMemInfo();
	}
	else if (CMD_STRCMP("dis"))
	{
		for (int32_t i = 0; i < keyIndex; ++i)
		{
			client->disconnect(i);
		}
	}
	else if (CMD_STRCMP("add"))
	{
		//新添加连接
		client->connect(szIP.c_str(), port, keyIndex++);
	}
	//else if (CMD_STRCMP("closea"))
	//{
	//	client->setAutoReconnect(false);
	//}
	//else if (CMD_STRCMP("opena"))
	//{
	//	client->setAutoReconnect(true);
	//}
	else if (CMD_STRCMP("close"))
	{
		client->closeClient();
	}
	else if (CMD_STRCMP("sendb"))
	{
		autosend = true;
	}
	else if (CMD_STRCMP("sende"))
	{
		autosend = false;
	}
	else if (CMD_STRCMP("closeclient"))
	{
		client->closeClient();
	}
	else if (CMD_STRCMP("autodisb"))
	{
		autoDisconnect = true;
	}
	else if (CMD_STRCMP("autodise"))
	{
		autoDisconnect = false;
	}
	else if (CMD_STRCMP("big"))
	{
		int32_t msgLen = 1024 * 100;
		char* szMsg = (char*)fc_malloc(msgLen);
		for (int32_t i = 0; i < msgLen; ++i)
		{
			szMsg[i] = 'A';
		}
		szMsg[msgLen - 1] = '\0';
		msgMng->sendMsg(key, szMsg, (uint32_t)strlen(szMsg));
		fc_free(szMsg);
	}
	else
	{
		return false;
	}
	return true;
}

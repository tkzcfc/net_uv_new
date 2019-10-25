#include "Application.h"
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <utility>


#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>


#include <chrono>
#include <thread>
#include <mutex>

#include "log/Logger.h"
#include "texture/TextureCache.h"
#include "net_uv.h"

using namespace net_uv;

Client* pClient = NULL;
NetMsgMgr* msgMng = nullptr;
bool show_main_logger = true;
bool show_imgui_demo = false;
static char szIP[256] = "127.0.0.1";
int32_t uPort = 1000;
bool startTag = false;
std::list<std::string> logList;
std::mutex logLock;
bool canConnect = false;
bool isLogin = false;

static const char* net_uv_log_name[NET_UV_L_FATAL + 1] =
{
	"HEART",
	"INFO",
	"WARNING",
	"ERROR",
	"FATAL"
};

const char* clientTypeItems[] = { "tcp", "kcp" };
static int clientTypeItemCurrent = 0;
static uint32_t ControlSessionID = 0;

void initClient()
{
	assert(pClient == NULL);

	if (clientTypeItemCurrent == 0)
	{
		pClient = new TCPClient();
	}
	else
	{
		pClient = new KCPClient();
	}
	pClient->setClientCloseCallback([](Client*)
	{
		delete pClient;
		pClient = NULL;
		startTag = false;
		net_uv::printMemInfo();
	});

	pClient->setConnectCallback([=](Client*, Session* session, int32_t status)
	{
		if (status == 0)
		{
			Logger::getInstance().addLog("[%d]connect failed\n", session->getSessionID());
		}
		else if (status == 1)
		{
			Logger::getInstance().addLog("[%d]Successful connection\n", session->getSessionID());
			msgMng->onConnect(session->getSessionID());
		}
		else if (status == 2)
		{
			Logger::getInstance().addLog("[%d]connection timed out\n", session->getSessionID());
		}
		canConnect = status != 1;
	});

	pClient->setDisconnectCallback([=](Client*, Session* session) {
		Logger::getInstance().addLog("[%d]Disconnect\n", session->getSessionID());
		canConnect = true;
		isLogin = false;
		msgMng->onDisconnect(session->getSessionID());
	});

	pClient->setRemoveSessionCallback([](Client*, Session* session) {
		Logger::getInstance().addLog("[%d]remove session\n", session->getSessionID());
	});

	pClient->setRecvCallback([](Client*, Session* session, char* data, uint32_t len)
	{
		msgMng->onBuff(session->getSessionID(), data, len);
	});

	pClient->connect(szIP, uPort, ControlSessionID);
	isLogin = false;



	msgMng = new NetMsgMgr();
	msgMng->setUserData(pClient);
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
		Logger::getInstance().addLog("[%d]recv data[%d]:%s\n", sessionID, len, std::string(data, len).c_str());

		const char szControlResult[] = "conrol client";
		if (len == sizeof(szControlResult) - 1 && memcmp(szControlResult, data, len) == 0)
		{
			isLogin = true;
		}
	});
}

const char* Application_GetName()
{
    return "Control";
}

void Application_Initialize()
{
	net_uv::setNetUVLogPrintFunc([](int32_t level, const char* buf)
	{
		std::string str = net_getTime();
		str.append("[NET-UV]-[");
		str.append(net_uv_log_name[level]);
		str.append("] ");
		str.append(buf);
		str.append("\n");

		logLock.lock();
		logList.emplace_back(str);
		logLock.unlock();
	});
}

void Application_Finalize()
{
	TextureCache::getInstance()->releaseAll();
	TextureCache::destroy();
}

void Application_Frame()
{
	auto& io = ImGui::GetIO();
	ImGui::NewLine();
	ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Tool"))
		{
			ImGui::MenuItem("Logger", "", &show_main_logger);
			ImGui::MenuItem("imgui demo", "", &show_imgui_demo);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if (show_imgui_demo)
	{
		ImGui::ShowDemoWindow(NULL);
	}
	if (show_main_logger)
	{
		Logger::getInstance().showAppLog(NULL);
	}

	ImGui::InputText("ip", szIP, 256, startTag ? ImGuiInputTextFlags_ReadOnly : 0);
	ImGui::InputInt("port", &uPort, 1, 1, startTag ? ImGuiInputTextFlags_ReadOnly : 0);

	if (!startTag)
	{
		ImGui::Combo("type", &clientTypeItemCurrent, clientTypeItems, IM_ARRAYSIZE(clientTypeItems));
	}

	if (startTag)
	{
		if (ImGui::Button("stop"))
		{
			pClient->closeClient();
		}

		if (canConnect)
		{
			if (ImGui::Button("connect"))
			{
				pClient->connect(szIP, uPort, ControlSessionID);
			}
		}
		else
		{
			if (isLogin)
			{
				static char buf[256] = { 0 };
				if (ImGui::InputText("input", buf, 256, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					msgMng->sendMsg(ControlSessionID, buf, strlen(buf));
					buf[0] = '\0';
				}
			}
			else
			{
				if (ImGui::Button("login"))
				{
					static const char szControlCMD[] = "control";
					msgMng->sendMsg(ControlSessionID, (char*)szControlCMD, sizeof(szControlCMD));
				}
			}
		}
	}
	else
	{
		if (ImGui::Button("start"))
		{
			startTag = true;
			initClient();
		}
	}

	if (pClient)
	{
		pClient->updateFrame();
	}

	logLock.lock();
	while (!logList.empty())
	{
		Logger::getInstance().addLog(logList.front().c_str());
		logList.pop_front();
	}
	logLock.unlock();
}


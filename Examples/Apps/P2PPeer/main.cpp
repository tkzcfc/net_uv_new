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
#include "Exception.h"

using namespace net_uv;

bool show_main_logger = true;
bool show_imgui_demo = false;
static char szIP[256] = "127.0.0.1";
int32_t uPort = 1001;
bool startTag = false;
bool connectToTurnTag = false;
std::list<std::string> logList;
std::mutex logLock;

static const char* net_uv_log_name[NET_UV_L_FATAL + 1] =
{
	"HEART",
	"INFO",
	"WARNING",
	"ERROR",
	"FATAL"
};


P2PPeer* pPeer = NULL;
std::vector<uint64_t> allConnectPeer;
std::string peerKey;
char szTarPeerKey[256] = {};

void startPeer()
{
	if (pPeer)
	{
		pPeer->start(szIP, uPort);
		return;
	}

	pPeer = new P2PPeer();
	pPeer->setStartCallback([=](bool isSuccess)
	{
		startTag = isSuccess;
		connectToTurnTag = false;
		Logger::getInstance().addLog("Startup %s\n", isSuccess ? "success" : "failure");
	});

	pPeer->setNewConnectCallback([=](uint64_t key)
	{
		Logger::getInstance().addLog("new connection [%llu] join\n", key);
		allConnectPeer.push_back(key);
	});

	pPeer->setConnectToPeerCallback([=](uint64_t key, int status)
	{
		// 0 找不到目标  1 成功 2 超时
		if (status == 1)
		{
			allConnectPeer.push_back(key);
		}
		const char* msg[] = { "Connection failed, error: target not found", "Connected", "Connection failed, error: timeout", "Connection failed, error: the node is already connected to this node as a client" };
		Logger::getInstance().addLog("[%llu]%s\n", key, msg[status]);
	});

	pPeer->setConnectToTurnCallback([=](bool isSuccess, uint64_t selfKey)
	{
		if (isSuccess)
		{
			Logger::getInstance().addLog("Successfully connected to turn，Native Key:[%llu]\n", selfKey);
			peerKey = std::to_string(selfKey);
			connectToTurnTag = true;
		}
		else
		{
			peerKey = "";
			Logger::getInstance().addLog("Failed to connect to turn\n");
			connectToTurnTag = false;
		}
	});

	pPeer->setDisConnectToTurnCallback([=]()
	{
		connectToTurnTag = false;
		peerKey = "";
		Logger::getInstance().addLog("Disconnect from turn\n");
	});

	pPeer->setDisConnectToPeerCallback([=](uint64_t key)
	{
		Logger::getInstance().addLog("[%llu] disconnected\n", key);
		allConnectPeer.erase(std::find(allConnectPeer.begin(), allConnectPeer.end(), key));
	});

	pPeer->setRecvCallback([=](uint64_t key, char* data, uint32_t len)
	{
		std::string recvstr(data, len);
		Logger::getInstance().addLog("[%llu]recv: %s\n", key, recvstr.c_str());
	});

	pPeer->setCloseCallback([=]()
	{
		Logger::getInstance().addLog("Peer closed\n");

		delete pPeer;
		pPeer = NULL;
		startTag = false;
		connectToTurnTag = false;
		net_uv::printMemInfo();
	});

	pPeer->start(szIP, uPort);
}

const char* Application_GetName()
{
	return "Peer";
}

void Application_Initialize()
{
	Exception::registerException("p2pPeer.dump");

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

	if (startTag)
	{
		if (ImGui::Button("stop"))
		{
			pPeer->stop();
		}

		ImGui::Separator();
		ImGui::InputText("peerKey", (char*)peerKey.c_str(), peerKey.size(), ImGuiInputTextFlags_ReadOnly);
		if (!connectToTurnTag)
		{
			if (ImGui::Button("connect turn") && pPeer)
			{
				pPeer->restartConnectTurn();
			}
		}

		ImGui::Separator();
		ImGui::InputText("target key", szTarPeerKey, 256);
		if (ImGui::Button("connect") && pPeer && strlen(szTarPeerKey) > 0)
		{
			uint64_t key = std::atoll(szTarPeerKey);
			pPeer->connectToPeer(key);
		}

		if (!allConnectPeer.empty())
		{
			ImGui::Separator();
			static char buf[4096] = { 0 };
			if (ImGui::InputText("input", buf, 4096, ImGuiInputTextFlags_EnterReturnsTrue) && pPeer)
			{
				for (auto &it : allConnectPeer)
				{
					pPeer->send(it, buf, strlen(buf));
				}
			}
			if (ImGui::Button("send") && pPeer)
			{
				for (auto &it : allConnectPeer)
				{
					pPeer->send(it, buf, strlen(buf));
				}
			}
		}
	}
	else
	{
		if (ImGui::Button("start"))
		{
			startTag = true;
			startPeer();
		}
	}

	if (pPeer)
	{
		pPeer->updateFrame();
	}

	logLock.lock();
	while (!logList.empty())
	{
		Logger::getInstance().addLog(logList.front().c_str());
		logList.pop_front();
	}
	logLock.unlock();
}

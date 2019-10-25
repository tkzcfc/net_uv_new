#include "TCPServer.h"

NS_NET_UV_BEGIN

enum 
{
	TCP_SVR_OP_STOP_SERVER,	// stop svr
	TCP_SVR_OP_SEND_DATA,	// send data
	TCP_SVR_OP_DIS_SESSION,	// disconnect session
	TCP_SVR_OP_SEND_DIS_SESSION_MSG_TO_MAIN_THREAD,
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////

TCPServer::TCPServer()
	: m_start(false)
	, m_server(NULL)
	, m_sessionID(0)
{
}

TCPServer::~TCPServer()
{
	stopServer();
	this->join();
	clearData();

	NET_UV_LOG(NET_UV_L_INFO, "TCPServer destroy...");
}

bool TCPServer::startServer(const char* ip, uint32_t port, bool isIPV6, int32_t maxCount)
{
	if (m_serverStage != ServerStage::STOP || m_start)
	{
		return false;
	}

	Server::startServer(ip, port, isIPV6, maxCount);

	m_server = (TCPSocket*)fc_malloc(sizeof(TCPSocket));
	if (m_server == NULL)
	{
		return false;
	}
	new (m_server) TCPSocket(m_loop.ptr());
	m_server->setCloseCallback(std::bind(&TCPServer::onServerSocketClose, this, std::placeholders::_1));
	m_server->setNewConnectionCallback(std::bind(&TCPServer::onNewConnect, this, std::placeholders::_1, std::placeholders::_2));

	bool ok = true;
	if (m_isIPV6)
	{
		ok = m_server->bind6(m_ip.c_str(), m_port);
	}
	else
	{
		ok = m_server->bind(m_ip.c_str(), m_port);
	}

	if (!ok)
	{
		startFailureLogic();
		return false;
	}
	
	bool suc = m_server->listen(maxCount);
	if (!suc)
	{
		startFailureLogic();
		return false;
	}

	m_start = true;
	m_serverStage = ServerStage::RUN;

	setListenPort(m_server->getPort());
	NET_UV_LOG(NET_UV_L_INFO, "TCPServer %s:%u start-up...", m_ip.c_str(), getListenPort());

	startThread();

	return true;
}

bool TCPServer::stopServer()
{
	if (!m_start)
		return false;
	m_start = false;
	pushOperation(TCP_SVR_OP_STOP_SERVER, NULL, NULL, 0U);
	return true;
}

void TCPServer::updateFrame()
{
	if (!getThreadMsg())
	{
		return;
	}

	bool closeServerTag = false;
	while (!m_msgDispatchQue.empty())
	{
		const NetThreadMsg& Msg = m_msgDispatchQue.front();

		switch (Msg.msgType)
		{
		case NetThreadMsgType::RECV_DATA:
		{
			m_recvCall(this, Msg.pSession, Msg.data, Msg.dataLen);
			fc_free(Msg.data);
		}break;
		case NetThreadMsgType::NEW_CONNECT:
		{
			m_newConnectCall(this, Msg.pSession);
		}break;
		case NetThreadMsgType::DIS_CONNECT:
		{
			m_disconnectCall(this, Msg.pSession);
			pushOperation(TCP_SVR_OP_SEND_DIS_SESSION_MSG_TO_MAIN_THREAD, NULL, 0, Msg.pSession->getSessionID());
		}break;
		case NetThreadMsgType::EXIT_LOOP:
		{
			closeServerTag = true;
		}break;
		default:
			break;
		}
		m_msgDispatchQue.pop();
	}
	if (closeServerTag && m_closeCall != nullptr)
	{
		m_closeCall(this);
	}
}

void TCPServer::send(uint32_t sessionID, char* data, uint32_t len)
{
	if (!m_start || data == NULL || len <= 0)
		return;

	char* pdata = (char*)fc_malloc(len);
	memcpy(pdata, data, len);
	pushOperation(TCP_SVR_OP_SEND_DATA, pdata, len, sessionID);
}

void TCPServer::sendEx(uint32_t sessionID, char* data, uint32_t len)
{
	if (!m_start || data == NULL || len <= 0)
		return;

	pushOperation(TCP_SVR_OP_SEND_DATA, data, len, sessionID);
}

void TCPServer::disconnect(uint32_t sessionID)
{
	pushOperation(TCP_SVR_OP_DIS_SESSION, NULL, 0, sessionID);
}

void TCPServer::run()
{
	startIdle();

	m_loop.run(UV_RUN_DEFAULT);

	m_server->~TCPSocket();
	fc_free(m_server);
	m_server = NULL;
	
	m_loop.close();
	
	m_serverStage = ServerStage::STOP;
	pushThreadMsg(NetThreadMsgType::EXIT_LOOP, NULL);
}


void TCPServer::onNewConnect(uv_stream_t* server, int32_t status)
{
	TCPSocket* client = m_server->accept(server, status);
	if (client != NULL)
	{
		TCPSession* session = TCPSession::createSession(this, client);
		if (session == NULL)
		{
			NET_UV_LOG(NET_UV_L_ERROR, "The server failed to create a new session. It may be out of memory");
		}
		else
		{
			session->setSessionRecvCallback(std::bind(&TCPServer::onSessionRecvData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
			session->setSessionClose(std::bind(&TCPServer::onSessionClose, this, std::placeholders::_1));
			session->setSessionID(m_sessionID);
			session->setIsOnline(true);

			m_sessionID++;

			addNewSession(session);
		}
	}
	else
	{
		NET_UV_LOG(NET_UV_L_ERROR, "Failed to accept new connection");
	}
}

void TCPServer::onServerSocketClose(Socket* svr)
{
	m_serverStage = ServerStage::CLEAR;
}

void TCPServer::startFailureLogic()
{
	m_server->~TCPSocket();
	fc_free(m_server);
	m_server = NULL;

	m_loop.run(UV_RUN_DEFAULT);
	m_loop.close();

	m_serverStage = ServerStage::STOP;
}

void TCPServer::addNewSession(TCPSession* session)
{
	if (session == NULL)
	{
		return;
	}
	serverSessionData data;
	data.isInvalid = false;
	data.session = session;

	m_allSession.insert(std::make_pair(session->getSessionID(), data));

	pushThreadMsg(NetThreadMsgType::NEW_CONNECT, session);
}

void TCPServer::onSessionClose(Session* session)
{
	if (session == NULL)
	{
		return;
	}
	auto it = m_allSession.find(session->getSessionID());
	if (it != m_allSession.end())
	{
		it->second.isInvalid = true;
		pushThreadMsg(NetThreadMsgType::DIS_CONNECT, session);
	}
}

void TCPServer::onSessionRecvData(Session* session, char* data, uint32_t len)
{
	pushThreadMsg(NetThreadMsgType::RECV_DATA, session, data, len);
}

void TCPServer::executeOperation()
{
	if (m_operationMutex.trylock() != 0)
	{
		return;
	}

	if (m_operationQue.empty())
	{
		m_operationMutex.unlock();
		return;
	}

	while (!m_operationQue.empty())
	{
		m_operationDispatchQue.push(m_operationQue.front());
		m_operationQue.pop();
	}
	m_operationMutex.unlock();

	while (!m_operationDispatchQue.empty())
	{
		auto & curOperation = m_operationDispatchQue.front();
		switch (curOperation.operationType)
		{
		case TCP_SVR_OP_SEND_DATA :
		{
			auto it = m_allSession.find(curOperation.sessionID);
			if (it != m_allSession.end())
			{
				it->second.session->executeSend((char*)curOperation.operationData, curOperation.operationDataLen);
			}
			else
			{
				//invalid session
				fc_free(curOperation.operationData);
			}
		}break;
		case TCP_SVR_OP_DIS_SESSION:
		{
			auto it = m_allSession.find(curOperation.sessionID);
			if (it != m_allSession.end())
			{
				it->second.session->executeDisconnect();
			}
		}break;
		case TCP_SVR_OP_SEND_DIS_SESSION_MSG_TO_MAIN_THREAD:
		{
			auto it = m_allSession.find(curOperation.sessionID);
			if (it != m_allSession.end())
			{
				it->second.session->~TCPSession();
				fc_free(it->second.session);
				it = m_allSession.erase(it);
			}
		}break;
		case TCP_SVR_OP_STOP_SERVER:
		{
			for (auto & it : m_allSession)
			{
				if (!it.second.isInvalid)
				{
					it.second.session->executeDisconnect();
				}
			}
			m_server->disconnect();
			m_serverStage = ServerStage::WAIT_CLOSE_SERVER_SOCKET;

			stopTimerUpdate();
		}break;
		default:
			break;
		}
		m_operationDispatchQue.pop();
	}
}

void TCPServer::clearData()
{
	m_msgMutex.lock();
	while (!m_msgQue.empty())
	{
		if (m_msgQue.front().data)
		{
			fc_free(m_msgQue.front().data);
		}
		m_msgQue.pop();
	}
	m_msgMutex.unlock();
	while (!m_operationQue.empty())
	{
		if (m_operationQue.front().operationType == TCP_SVR_OP_SEND_DATA)
		{
			fc_free(m_operationQue.front().operationData);
		}
		m_operationQue.pop();
	}
}

void TCPServer::onIdleRun()
{
	executeOperation();
	switch (m_serverStage)
	{
	case ServerStage::CLEAR:
	{
		for (auto& it : m_allSession)
		{
			if (!it.second.isInvalid)
			{
				it.second.session->executeDisconnect();
			}
		}
		m_serverStage = ServerStage::WAIT_SESSION_CLOSE;
	}
	break;
	case ServerStage::WAIT_SESSION_CLOSE:
	{
		if (m_allSession.empty())
		{
			stopIdle();
			m_loop.stop();
		}
	}
	break;
	default:
		break;
	}
	ThreadSleep(1);
}

NS_NET_UV_END
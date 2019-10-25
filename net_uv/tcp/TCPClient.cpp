#include "TCPClient.h"

NS_NET_UV_BEGIN

enum 
{
	TCP_CLI_OP_CONNECT,			// connect
	TCP_CLI_OP_SENDDATA,		// send data
	TCP_CLI_OP_DISCONNECT,		// disconnect
	TCP_CLI_OP_SET_KEEP_ALIVE,	// set keep alive
	TCP_CLI_OP_SET_NO_DELAY,	// set no delay
	TCP_CLI_OP_CLIENT_CLOSE,	// close
	TCP_CLI_OP_REMOVE_SESSION,	// remove session
	TCP_CLI_OP_DELETE_SESSION,	// delete session
};

struct TCPClientConnectOperation
{
	TCPClientConnectOperation() {}
	~TCPClientConnectOperation() {}
	std::string ip;
	uint32_t port;
	uint32_t sessionID;
};


//////////////////////////////////////////////////////////////////////////////////
TCPClient::TCPClient()
	: m_enableNoDelay(true)
	, m_enableKeepAlive(false)
	, m_keepAliveDelay(10)
	, m_isStop(false)
{	
	m_clientStage = clientStage::START;
	
	this->startIdle();
	this->startTimerUpdate(100);
	this->startThread();
}

TCPClient::~TCPClient()
{
	this->closeClient();
	this->join();
	this->clearData();
}


void TCPClient::connect(const char* ip, uint32_t port, uint32_t sessionId)
{
	if (m_isStop)
		return;

	assert(ip != NULL);

	TCPClientConnectOperation* opData = (TCPClientConnectOperation*)fc_malloc(sizeof(TCPClientConnectOperation));
	new (opData)TCPClientConnectOperation();

	opData->ip = ip;
	opData->port = port;
	opData->sessionID = sessionId;
	pushOperation(TCP_CLI_OP_CONNECT, opData, 0U, 0U);
}

void TCPClient::closeClient()
{
	if (m_isStop)
		return;
	m_isStop = true;
	pushOperation(TCP_CLI_OP_CLIENT_CLOSE, NULL, 0U, 0U);
}

void TCPClient::updateFrame()
{
	if (!getThreadMsg())
	{
		return;
	}

	bool closeClientTag = false;
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
		case NetThreadMsgType::CONNECT_FAIL:
		{
			if (m_connectCall != nullptr)
			{
				m_connectCall(this, Msg.pSession, 0);
			}
		}break;
		case NetThreadMsgType::CONNECT:
		{
			if (m_connectCall != nullptr)
			{
				m_connectCall(this, Msg.pSession, 1);
			}
		}break;
		case NetThreadMsgType::CONNECT_TIMOUT:
		{
			if (m_connectCall != nullptr)
			{
				m_connectCall(this, Msg.pSession, 2);
			}
		}break;
		case NetThreadMsgType::DIS_CONNECT:
		{
			if (m_disconnectCall != nullptr)
			{
				m_disconnectCall(this, Msg.pSession);
			}
		}break;
		case NetThreadMsgType::EXIT_LOOP:
		{
			closeClientTag = true;
		}break;
		case NetThreadMsgType::REMOVE_SESSION:
		{
			if (m_removeSessionCall != nullptr)
			{
				m_removeSessionCall(this, Msg.pSession);
			}
			pushOperation(TCP_CLI_OP_DELETE_SESSION, NULL, 0U, Msg.pSession->getSessionID());
		}break;
		default:
			break;
		}
		m_msgDispatchQue.pop();
	}
	if (closeClientTag && m_clientCloseCall != nullptr)
	{
		m_clientCloseCall(this);
	}
}

void TCPClient::removeSession(uint32_t sessionId)
{
	pushOperation(TCP_CLI_OP_REMOVE_SESSION, NULL, 0U, sessionId);
}

/// SessionManager

void TCPClient::disconnect(uint32_t sessionId)
{
	if (m_isStop)
		return;

	pushOperation(TCP_CLI_OP_DISCONNECT, NULL, 0U, sessionId);
}

void TCPClient::send(uint32_t sessionId, char* data, uint32_t len)
{
	if (m_isStop)
		return;

	if (data == 0 || len <= 0)
		return;

	char* pdata = (char*)fc_malloc(len);
	memcpy(pdata, data, len);
	pushOperation(TCP_CLI_OP_SENDDATA, pdata, len, sessionId);
}

void TCPClient::sendEx(uint32_t sessionId, char* data, uint32_t len)
{
	if (m_isStop)
		return;

	if (data == 0 || len <= 0)
		return;

	pushOperation(TCP_CLI_OP_SENDDATA, data, len, sessionId);
}

/// TCPClient

bool TCPClient::setSocketNoDelay(bool enable)
{
	if (m_isStop)
		return false;

	m_enableNoDelay = enable;
	pushOperation(TCP_CLI_OP_SET_NO_DELAY, NULL, 0U, 0U);
	return true;
}

bool TCPClient::setSocketKeepAlive(int32_t enable, uint32_t delay)
{
	if (m_isStop)
		return false;

	m_enableKeepAlive = enable;
	m_keepAliveDelay = delay;
	
	pushOperation(TCP_CLI_OP_SET_KEEP_ALIVE, NULL, 0U, 0U);
	return true;
}

/// Runnable
void TCPClient::run()
{
	m_loop.run(UV_RUN_DEFAULT);
	m_loop.close();

	m_clientStage = clientStage::STOP;

	this->pushThreadMsg(NetThreadMsgType::EXIT_LOOP, NULL);
}

/// SessionManager
void TCPClient::executeOperation()
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
		case TCP_CLI_OP_SENDDATA:		// send data
		{
			auto sessionData = getClientSessionDataBySessionId(curOperation.sessionID);
			if (sessionData && !sessionData->removeTag)
			{
				sessionData->session->executeSend((char*)curOperation.operationData, curOperation.operationDataLen);
			}
			else
			{
				fc_free(curOperation.operationData);
			}
		}break;
		case TCP_CLI_OP_DISCONNECT:	// disconnect
		{
			auto sessionData = getClientSessionDataBySessionId(curOperation.sessionID);
			if (sessionData && sessionData->connectState == CONNECT)
			{
				sessionData->connectState = DISCONNECTING;
				sessionData->session->executeDisconnect();
			}
		}break;
		case TCP_CLI_OP_CONNECT:	// connect
		{
			if (curOperation.operationData)
			{
				createNewConnect(curOperation.operationData);
				((TCPClientConnectOperation*)curOperation.operationData)->~TCPClientConnectOperation();
				fc_free(curOperation.operationData);
			}
		}break;
		case TCP_CLI_OP_SET_KEEP_ALIVE: //set keep alive
		{
			for (auto& it : m_allSessionMap)
			{
				auto socket = it.second->session->getTCPSocket();
				if (socket && !it.second->removeTag)
				{
					socket->setKeepAlive(m_enableKeepAlive, m_keepAliveDelay);
				}
			}
		}break;
		case TCP_CLI_OP_SET_NO_DELAY:// set nodelay
		{
			for (auto& it : m_allSessionMap)
			{
				auto socket = it.second->session->getTCPSocket();
				if (socket && !it.second->removeTag)
				{
					socket->setNoDelay(m_enableNoDelay);
				}
			}
		}break;
		case TCP_CLI_OP_CLIENT_CLOSE:// client close
		{
			m_clientStage = clientStage::CLEAR_SESSION;
		}break;
		case TCP_CLI_OP_REMOVE_SESSION:
		{
			auto sessionData = getClientSessionDataBySessionId(curOperation.sessionID);
			if (sessionData)
			{
				if (sessionData->connectState != DISCONNECT)
				{
					sessionData->removeTag = true;
					sessionData->session->executeDisconnect();
				}
				else
				{
					if (!sessionData->removeTag)
					{
						sessionData->removeTag = true;
						pushThreadMsg(NetThreadMsgType::REMOVE_SESSION, sessionData->session);
					}
				}
			}
		}break;
		case TCP_CLI_OP_DELETE_SESSION:// delete session
		{
			auto it = m_allSessionMap.find(curOperation.sessionID);
			if (it != m_allSessionMap.end() && it->second->removeTag)
			{
				it->second->session->~TCPSession();
				fc_free(it->second->session);
				it->second->~clientSessionData();
				fc_free(it->second);
				m_allSessionMap.erase(it);
			}
		}break;
		default:
			break;
		}
		m_operationDispatchQue.pop();
	}
}

void TCPClient::onIdleRun()
{
	executeOperation();
	ThreadSleep(1);
}

void TCPClient::onTimerUpdateRun()
{
	if (m_clientStage == clientStage::CLEAR_SESSION)
	{
		clientSessionData* data = NULL;
		for (auto& it : m_allSessionMap)
		{
			data = it.second;

			if (data->connectState == CONNECT)
			{
				data->removeTag = true;
				data->session->executeDisconnect();
			}
			else if (data->connectState == DISCONNECT)
			{
				if (!data->removeTag)
				{
					data->removeTag = true;
					pushThreadMsg(NetThreadMsgType::REMOVE_SESSION, data->session);
				}
			}
		}
		if (m_allSessionMap.empty())
		{
			m_clientStage = clientStage::WAIT_EXIT;
		}
	}
	else if (m_clientStage == clientStage::WAIT_EXIT)
	{
		stopIdle();
		stopTimerUpdate();
		m_loop.stop();
	}
}

/// TCPClient
void TCPClient::onSocketConnect(Socket* socket, int32_t status)
{
	Session* pSession = NULL;
	bool isSuc = (status == 1);

	for (auto& it : m_allSessionMap)
	{
		if (it.second->session->getTCPSocket() == socket)
		{
			pSession = it.second->session;
			it.second->session->setIsOnline(isSuc);
			it.second->connectState = isSuc ? CONNECTSTATE::CONNECT : CONNECTSTATE::DISCONNECT;

			if (isSuc)
			{
				if (m_clientStage != clientStage::START)
				{
					it.second->session->executeDisconnect();
					pSession = NULL;
				}
				else
				{
					if (it.second->removeTag)
					{
						it.second->session->executeDisconnect();
						pSession = NULL;
					}
					else
					{
						it.second->session->getTCPSocket()->setNoDelay(m_enableNoDelay);
						it.second->session->getTCPSocket()->setKeepAlive(m_enableKeepAlive, m_keepAliveDelay);
					}
				}
			}
			break;
		}
	}

	if (pSession)
	{
		if (status == 0)
		{
			pushThreadMsg(NetThreadMsgType::CONNECT_FAIL, pSession);
		}
		else if (status == 1)
		{
			pushThreadMsg(NetThreadMsgType::CONNECT, pSession);
		}
		else if (status == 2)
		{
			pushThreadMsg(NetThreadMsgType::CONNECT_TIMOUT, pSession);
		}
	}
}

void TCPClient::onSessionClose(Session* session)
{
	auto sessionData = getClientSessionDataBySession(session);
	if (sessionData)
	{
		sessionData->connectState = CONNECTSTATE::DISCONNECT;
		pushThreadMsg(NetThreadMsgType::DIS_CONNECT, sessionData->session);

		if (sessionData->removeTag)
		{
			pushThreadMsg(NetThreadMsgType::REMOVE_SESSION, sessionData->session);
		}
	}
}

void TCPClient::createNewConnect(void* data)
{
	if (data == NULL)
		return;
	TCPClientConnectOperation* opData = (TCPClientConnectOperation*)data;

	auto it = m_allSessionMap.find(opData->sessionID);
	if (it != m_allSessionMap.end())
	{
		if (it->second->removeTag)
			return;

		if (it->second->connectState == CONNECTSTATE::DISCONNECT)
		{
			it->second->ip = opData->ip;
			it->second->port = opData->port;

			if (it->second->session->executeConnect(opData->ip.c_str(), opData->port))
			{
				it->second->connectState = CONNECTSTATE::CONNECTING;
			}
			else
			{
				it->second->connectState = CONNECTSTATE::DISCONNECT;
				it->second->session->executeDisconnect();
				pushThreadMsg(NetThreadMsgType::CONNECT_FAIL, it->second->session);
			}
		}
	}
	else
	{
		TCPSocket* socket = (TCPSocket*)fc_malloc(sizeof(TCPSocket));
		new (socket) TCPSocket(m_loop.ptr()); 
		socket->setConnectCallback(std::bind(&TCPClient::onSocketConnect, this, std::placeholders::_1, std::placeholders::_2));

		TCPSession* session = TCPSession::createSession(this, socket);

		if (session == NULL)
		{
			NET_UV_LOG(NET_UV_L_FATAL, "Failed to create session, may be out of memory!!!");
			return;
		}
		session->setSessionRecvCallback(std::bind(&TCPClient::onSessionRecvData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		session->setSessionClose(std::bind(&TCPClient::onSessionClose, this, std::placeholders::_1));
		session->setSessionID(opData->sessionID);
		session->setIsOnline(false);
		
		clientSessionData* cs = (clientSessionData*)fc_malloc(sizeof(clientSessionData));
		new (cs) clientSessionData();
		cs->removeTag = false;
		cs->ip = opData->ip;
		cs->port = opData->port;
		cs->session = session;
		cs->connectState = CONNECTSTATE::CONNECTING;

		m_allSessionMap.insert(std::make_pair(opData->sessionID, cs));
		
		if (socket->connect(opData->ip.c_str(), opData->port))
		{
			cs->connectState = CONNECTSTATE::CONNECTING;
		}
		else
		{
			cs->connectState = CONNECTSTATE::DISCONNECT;
			pushThreadMsg(NetThreadMsgType::CONNECT_FAIL, session);
		}
	}
}

void TCPClient::onSessionRecvData(Session* session, char* data, uint32_t len)
{
	pushThreadMsg(NetThreadMsgType::RECV_DATA, session, data, len);
}

TCPClient::clientSessionData* TCPClient::getClientSessionDataBySessionId(uint32_t sessionId)
{
	auto it = m_allSessionMap.find(sessionId);
	if (it != m_allSessionMap.end())
		return it->second;
	return NULL;
}

TCPClient::clientSessionData* TCPClient::getClientSessionDataBySession(Session* session)
{
	for (auto &it : m_allSessionMap)
	{
		if (it.second->session == session)
		{
			return it.second;
		}
	}
	return NULL;
}

void TCPClient::clearData()
{
	for (auto & it : m_allSessionMap)
	{
		it.second->session->~TCPSession();
		fc_free(it.second->session);
		it.second->~clientSessionData();
		fc_free(it.second);
	}
	m_allSessionMap.clear();

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
		auto & curOperation = m_operationQue.front();
		switch (curOperation.operationType)
		{
		case TCP_CLI_OP_SENDDATA:
		{
			if (curOperation.operationData)
			{
				fc_free(curOperation.operationData);
			}
		}break;
		case TCP_CLI_OP_CONNECT:
		{
			if (curOperation.operationData)
			{
				((TCPClientConnectOperation*)curOperation.operationData)->~TCPClientConnectOperation();
				fc_free(curOperation.operationData);
			}
		}break;
		}
		m_operationQue.pop();
	}
}


NS_NET_UV_END

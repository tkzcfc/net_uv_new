#include "SessionManager.h"

NS_NET_UV_BEGIN

SessionManager::SessionManager()
{}

SessionManager::~SessionManager()
{}

void SessionManager::pushOperation(int32_t type, void* data, uint32_t len, uint32_t sessionID)
{
	SessionManager::SessionOperation operationData;
	operationData.operationType = type;
	operationData.operationData = data;
	operationData.operationDataLen = len;
	operationData.sessionID = sessionID;

	m_operationMutex.lock();
	m_operationQue.push(operationData);
	m_operationMutex.unlock();
}


NS_NET_UV_END


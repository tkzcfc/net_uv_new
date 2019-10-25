#pragma once

#include "Common.h"
#include "Session.h"
#include "Mutex.h"

NS_NET_UV_BEGIN

class SessionManager
{
public:

	SessionManager();

	virtual ~SessionManager();

	virtual void send(uint32_t sessionID, char* data, uint32_t len) = 0;

	virtual void sendEx(uint32_t sessionID, char* data, uint32_t len) = 0;

	virtual void disconnect(uint32_t sessionID) = 0;
	
protected:

	void pushOperation(int32_t type, void* data, uint32_t len, uint32_t sessionID);

	virtual void executeOperation() = 0;
	
protected:

	struct SessionOperation
	{
		int32_t operationType;
		void* operationData;
		uint32_t operationDataLen;
		uint32_t sessionID;
	};

protected:
	Mutex m_operationMutex;
	std::queue<SessionOperation> m_operationQue;
	std::queue<SessionOperation> m_operationDispatchQue;
};
NS_NET_UV_END

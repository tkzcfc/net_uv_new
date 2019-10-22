#include "Client.h"

NS_NET_UV_BEGIN

Client::Client()
	: m_connectCall(nullptr)
	, m_disconnectCall(nullptr)
	, m_recvCall(nullptr)
	, m_clientStage(clientStage::STOP)
{

}

Client::~Client()
{}

bool Client::isCloseFinish()
{
	return (m_clientStage == clientStage::STOP);
}


NS_NET_UV_END
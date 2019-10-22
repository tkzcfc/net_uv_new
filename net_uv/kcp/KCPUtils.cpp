#include "KcpUtils.h"

NS_NET_UV_BEGIN

#define NET_KCP_PACKET_END_FLAG "$%^^&"

#define NET_KCP_SYN_PACKET "kcp_syn_packet:"
#define NET_KCP_SYN_AND_ACK_PACKET "kcp_syn&ack_packet:"
#define NET_KCP_ACK_PACKET "kcp_ack_packet:"
#define NET_KCP_FIN_PACKET "kcp_fin_packet:"

#define NET_KCP_HEART_PACKET "kcp_heart_package"
#define NET_KCP_HEART_BACK_PACKET "kcp_heart_back_package"


static std::string get_kcp_packet_info(const char* data, uint32_t len, const char* head)
{
	uint32_t headlen = strlen(head);
	if (headlen >= len)
	{
		return "";
	}

	const char* str = data + headlen;
	const char* substr = strstr(str, NET_KCP_PACKET_END_FLAG);

	std::string out;

	for (auto i = 0U; i < len; ++i)
	{
		if (str + i == substr) break;
		out.push_back(*(str + i));
	}

	if (out.size() > len - headlen - strlen(NET_KCP_PACKET_END_FLAG))
	{
		return "";
	}

	return out;
}

static uint32_t a2l_safe(const std::string& str)
{
	if (str.empty())
	{
		return 0;
	}

	for (auto i = 0U; i < str.size(); ++i)
	{
		if (str[i] <= '0' && '9' <= str[i] && ' ' != str[i])
		{
			return 0;
		}
	}
	return (uint32_t)::atol(str.c_str());
}

// value = random
std::string kcp_making_syn(uint32_t value)
{
	char str_send_back_conv[128] = "";
	size_t n = snprintf(str_send_back_conv, sizeof(str_send_back_conv), "%s%u%s", NET_KCP_SYN_PACKET, value, NET_KCP_PACKET_END_FLAG);
	return std::string(str_send_back_conv, n);
}

bool kcp_is_syn(const char* data, uint32_t len)
{
	return (len > sizeof(NET_KCP_SYN_PACKET) &&
		memcmp(data, NET_KCP_SYN_PACKET, sizeof(NET_KCP_SYN_PACKET) - 1) == 0);
}

uint32_t kcp_grab_syn_value(const char* data, uint32_t len)
{
	std::string str = get_kcp_packet_info(data, len, NET_KCP_SYN_PACKET);
	return a2l_safe(str);
}

// value = syn.value + 1
// conv = kcp conv
std::string kcp_making_syn_and_ack(uint32_t value, uint32_t conv)
{
	char str_send_back_conv[128] = "";
	size_t n = snprintf(str_send_back_conv, sizeof(str_send_back_conv), "%s value:%u conv:%u%s", NET_KCP_SYN_AND_ACK_PACKET, value, conv, NET_KCP_PACKET_END_FLAG);
	return std::string(str_send_back_conv, n);
}

bool kcp_is_syn_and_ack(const char* data, uint32_t len)
{
	return (len > sizeof(NET_KCP_SYN_AND_ACK_PACKET) &&
		memcmp(data, NET_KCP_SYN_AND_ACK_PACKET, sizeof(NET_KCP_SYN_AND_ACK_PACKET) - 1) == 0);
}

bool kcp_grab_syn_and_ack_value(const char* data, uint32_t len, uint32_t* pValue, uint32_t* pConv)
{
	if (pValue) *pValue = 0;
	if (pConv) *pConv = 0;

	std::string str = get_kcp_packet_info(data, len, NET_KCP_SYN_AND_ACK_PACKET);
	if (str.empty())
	{
		return false;
	}

	static const char* valueKey = " value:";
	static const char* convKey = " conv:";

	uint32_t valueKeyLen = strlen(valueKey);
	uint32_t convKeyLen = strlen(convKey);

	auto valuePos = str.find(valueKey);
	auto convPos = str.find(convKey);

	if (valuePos == std::string::npos || convPos == std::string::npos || convPos < valueKeyLen)
	{
		return false;
	}

	if (pValue) *pValue = a2l_safe(std::string(str.c_str() + valueKeyLen, convPos - valueKeyLen));
	if (pConv) *pConv = a2l_safe(std::string(str.c_str() + convPos + convKeyLen));

	return true;
}

// value = conv + 1
std::string kcp_making_ack(uint32_t value)
{
	char str_send_back_conv[128] = "";
	size_t n = snprintf(str_send_back_conv, sizeof(str_send_back_conv), "%s%u%s", NET_KCP_ACK_PACKET, value, NET_KCP_PACKET_END_FLAG);
	return std::string(str_send_back_conv, n);
}

bool kcp_is_ack(const char* data, uint32_t len)
{
	return (len > sizeof(NET_KCP_ACK_PACKET) &&
		memcmp(data, NET_KCP_ACK_PACKET, sizeof(NET_KCP_ACK_PACKET) - 1) == 0);
}

uint32_t kcp_grab_ack_value(const char* data, uint32_t len)
{
	std::string str = get_kcp_packet_info(data, len, NET_KCP_ACK_PACKET);
	return a2l_safe(str);
}

// conv
std::string kcp_making_fin(uint32_t conv)
{
	char str_send_back_conv[128] = "";
	size_t n = snprintf(str_send_back_conv, sizeof(str_send_back_conv), "%s%u%s", NET_KCP_FIN_PACKET, conv, NET_KCP_PACKET_END_FLAG);
	return std::string(str_send_back_conv, n);
}

bool kcp_is_fin(const char* data, uint32_t len)
{
	return (len > sizeof(NET_KCP_FIN_PACKET) &&
		memcmp(data, NET_KCP_FIN_PACKET, sizeof(NET_KCP_FIN_PACKET) - 1) == 0);
}

uint32_t kcp_grab_fin_value(const char* data, uint32_t len)
{
	std::string str = get_kcp_packet_info(data, len, NET_KCP_FIN_PACKET);
	return a2l_safe(str);
}

std::string kcp_making_heart_packet()
{
	return std::string(NET_KCP_HEART_PACKET, sizeof(NET_KCP_HEART_PACKET));
}

bool kcp_is_heart_packet(const char* data, size_t len)
{
	return (len == sizeof(NET_KCP_HEART_PACKET) &&
		memcmp(data, NET_KCP_HEART_PACKET, sizeof(NET_KCP_HEART_PACKET) - 1) == 0);
}

std::string kcp_making_heart_back_packet()
{
	return std::string(NET_KCP_HEART_BACK_PACKET, sizeof(NET_KCP_HEART_BACK_PACKET));
}

bool kcp_is_heart_back_packet(const char* data, size_t len)
{
	return (len == sizeof(NET_KCP_HEART_BACK_PACKET) &&
		memcmp(data, NET_KCP_HEART_BACK_PACKET, sizeof(NET_KCP_HEART_BACK_PACKET) - 1) == 0);
}

NS_NET_UV_END

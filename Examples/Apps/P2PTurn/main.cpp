#include "net_uv.h"
#include "Exception.h"

using namespace net_uv;

int main(int argc, const char*argv[])
{
	Exception::registerException("turn.dump");

	int32_t port = 1001;
	if (argc >= 2)
	{
		port = std::atoi(argv[1]);
	}

	P2PTurn* turn = new P2PTurn();
	turn->start("0.0.0.0", port);

	while (true)
	{
		Sleep(1);
	}
	delete turn;

	printMemInfo();

	system("pause");

	return 0;
}


#if _WIN32
#include "windows.h"
#endif

#include <string>

class Exception
{
public:
	static void registerException(const char* dumpFileName);

	static std::string DumpFileName;
};

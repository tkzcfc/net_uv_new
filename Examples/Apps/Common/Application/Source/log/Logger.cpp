#include "log/Logger.h"





//////////////////////////////////////////////////////////////////////////

Logger& Logger::getInstance()
{
	static Logger log;
	return log;
}

Logger* Logger::getInstancePtr()
{
	return &Logger::getInstance();
}

Logger::Logger()
{
	static uint32_t log_unique = 0;
	uniqueId = log_unique++;

	char buf[32] = {};
	sprintf(buf, "Logger##%d", uniqueId);
	m_title = buf;
}

void Logger::addLog(const char* fmt, ...)
{
	int old_size = appLog.Buf.size();
	va_list args;
	va_start(args, fmt);
	appLog.Buf.appendfv(fmt, args);
	va_end(args);
	for (int new_size = appLog.Buf.size(); old_size < new_size; old_size++)
		if (appLog.Buf[old_size] == '\n')
			appLog.LineOffsets.push_back(old_size + 1);
	if (appLog.AutoScroll)
		appLog.ScrollToBottom = true;
}

// Demonstrate creating a simple log window with basic filtering.
void Logger::showAppLog(bool* p_open)
{
	// For the demo: add a debug button _BEFORE_ the normal log window contents
	// We take advantage of a rarely used feature: multiple calls to Begin()/End() are appending to the _same_ window.
	// Most of the contents of the window will be added by the log.Draw() call.
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

	// Actually call in the regular Log helper (which will Begin() into the same window as we just did)
	appLog.Draw(m_title.c_str(), p_open);
}

void Logger::clear()
{
	appLog.Clear();
}

void Logger::setTitle(const char* title)
{
	m_title = title;

	char buf[32] = {};
	sprintf(buf, "##%d", uniqueId);
	m_title.append(buf);
}


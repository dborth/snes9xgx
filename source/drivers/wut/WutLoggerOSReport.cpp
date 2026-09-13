/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * WutLoggerOSReport.cpp
 ***************************************************************************/
#include "WutLoggerOSReport.h"
#include <coreinit/debug.h>

bool WutLoggerOSReport::init(const LogConfig &) { return true; }
void WutLoggerOSReport::shutdown() { }

void WutLoggerOSReport::write(LogLevel, const char * line, size_t)
{
	OSReport("%s", line);
}

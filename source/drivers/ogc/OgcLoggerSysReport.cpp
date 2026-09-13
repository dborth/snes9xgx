/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * OgcLoggerSysReport.cpp
 ***************************************************************************/
#include "OgcLoggerSysReport.h"
#include <ogc/system.h>

// No handle to open/close and nothing that can meaningfully fail - always
// considered active once selected.
bool OgcLoggerSysReport::init(const LogConfig & /*config*/) { return true; }
void OgcLoggerSysReport::shutdown() { }

void OgcLoggerSysReport::write(LogLevel, const char * line, size_t)
{
	// line is already NUL-terminated by Logger::log() - no format
	// string risk from passing it as the format argument here, but "%s"
	// is used anyway so a future caller can never introduce one.
	SYS_Report("%s", line);
}

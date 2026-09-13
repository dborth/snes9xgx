/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * WutLoggerOSReport.h
 *
 * Wraps OSReport(), picked up by Cemu's log window and Decaf's console
 * capture. Always safe to register - no hardware/network dependency.
 ***************************************************************************/
#pragma once

#include "../Logger.h"

class WutLoggerOSReport : public LoggingDriver
{
	public:
		bool init(const LogConfig & config) override;
		void shutdown() override;
		void write(LogLevel level, const char * line, size_t len) override;
		const char * name() const override { return "OSReport"; }
};

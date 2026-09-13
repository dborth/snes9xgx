/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * OgcLoggerSysReport.h
 *
 * Wraps SYS_Report(), the libogc console-output call intercepted by
 * Dolphin's log window and picked up over a real console's debug UART.
 * Shared, unmodified, by both GameCubePlatform and WiiPlatform - SYS_Report
 * itself doesn't differ between HW_DOL and HW_RVL.
 ***************************************************************************/
#pragma once

#include "../Logger.h"

class OgcLoggerSysReport : public LoggingDriver
{
	public:
		bool init(const LogConfig & config) override;
		void shutdown() override;
		void write(LogLevel level, const char * line, size_t len) override;
		const char * name() const override { return "SYS_Report"; }
};

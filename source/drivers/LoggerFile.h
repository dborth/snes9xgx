/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * LoggerFile.h
 *
 * Generic file logging - a single stdio-based implementation covers every
 * platform. There is nothing platform-specific here.
 *
 * Register this only after FileSystemDriver::init() (and, if the target
 * device is removable, a successful mountStorageDevice() call) has run -
 * Logger::init()/reconfigure() can be called again later once storage
 * becomes available if it wasn't at platform startup.
 ***************************************************************************/
#pragma once

#include <stdio.h>
#include <stdint.h>
#include "Logger.h"

class LoggerFile : public LoggingDriver
{
	public:
		LoggerFile() = default;
		~LoggerFile() override { shutdown(); }

		bool init(const LogConfig & config) override;
		void shutdown() override;
		void write(LogLevel level, const char * line, size_t len) override;
		const char * name() const override { return "File"; }

	private:
		FILE *         file = nullptr;
		LogFlushPolicy flushPolicy = LogFlushPolicy::Immediate;
		uint32_t       flushEveryNWrites = 16;
		uint32_t       writesSinceFlush = 0;
};

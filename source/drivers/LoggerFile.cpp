/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * LoggerFile.cpp
 ***************************************************************************/
#include "LoggerFile.h"

bool LoggerFile::init(const LogConfig & config)
{
	shutdown();

	// Append-only - never truncates a prior run's log
	file = fopen(config.filePath, "a");
	if (!file)
		return false;

	// Line-buffer rather than the default full-buffering libc picks for
	// a regular file - keeps a torn/half-written line out of the file if
	// a flush is skipped (see below) and the console loses power.
	setvbuf(file, nullptr, _IOLBF, 0);

	flushPolicy = config.flushPolicy;
	flushEveryNWrites = config.flushEveryNWrites > 0 ? config.flushEveryNWrites : 1;
	writesSinceFlush = 0;
	return true;
}

void LoggerFile::shutdown()
{
	if (file)
	{
		fflush(file);
		fclose(file);
		file = nullptr;
	}
	writesSinceFlush = 0;
}

void LoggerFile::write(LogLevel, const char * line, size_t len)
{
	if (!file)
		return;

	fwrite(line, 1, len, file);

	switch (flushPolicy)
	{
		case LogFlushPolicy::Immediate:
			fflush(file);
			break;

		case LogFlushPolicy::EveryNWrites:
			if (++writesSinceFlush >= flushEveryNWrites)
			{
				fflush(file);
				writesSinceFlush = 0;
			}
			break;

		case LogFlushPolicy::Never:
		default:
			break;
	}
}

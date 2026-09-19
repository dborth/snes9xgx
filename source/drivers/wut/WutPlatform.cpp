/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutPlatform.cpp
 ***************************************************************************/
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sysapp/launch.h>
#include <proc_ui/procui.h>
#include <coreinit/systeminfo.h>
#include <coreinit/memory.h>
#include <malloc.h>

#include "WutPlatform.h"

void WutPlatform::init(int width, int height)
{
	WHBProcInit();

	this->threadDriver = new WutThreadDriver();
	this->threadDriver->init();

	this->videoDriver = new WutVideoDriver();
	this->videoDriver->init(width, height);

	this->audioDriver = new WutAudioDriver();
	this->audioDriver->init();

	this->inputDriver = new WutInputDriver();
	this->inputDriver->init();

	this->fileSystemDriver = new WutFileSystemDriver();
	this->fileSystemDriver->init();

#if LOGGING_ENABLED
	this->logger = new Logger();
	this->logger->registerBackend(LOGGER_OSREPORT,	new WutLoggerOSReport());
	this->logger->registerBackend(LOGGER_UDP,		new WutLoggerUdp());
	this->logger->registerBackend(LOGGER_SERIAL,	new WutLoggerUsbSerial());
	this->logger->registerBackend(LOGGER_FILE,		new LoggerFile());

	LogConfig config;
	static const int deviceCandidates[] = { DEVICE_SD };

	const char * mountPath = FindFirstMountedPath(this->fileSystemDriver, deviceCandidates, 1);

	if(mountPath[0] != '\0') {
		// mountPath already ends in "/" (eg. "/vol/external01/") - no separator needed.
		snprintf(config.filePath, sizeof(config.filePath), "%sdebug.log", mountPath);
	}
	this->logger->init(config);
#endif
}

void WutPlatform::shutdown()
{
	if (logger) {
		logger->shutdown();
		delete logger;
		logger = nullptr;
	}

	if(fileSystemDriver)
	{
		fileSystemDriver->shutdown();
		delete fileSystemDriver;
		fileSystemDriver = nullptr;
	}

	if(inputDriver)
	{
		inputDriver->shutdown();
		delete inputDriver;
		inputDriver = nullptr;
	}

	if(audioDriver)
	{
		audioDriver->shutdown();
		delete audioDriver;
		audioDriver = nullptr;
	}

	if(videoDriver)
	{
		videoDriver->shutdown();
		delete videoDriver;
		videoDriver = nullptr;
	}

	if(threadDriver)
	{
		threadDriver->shutdown();
		delete threadDriver;
		threadDriver = nullptr;
	}
}

//! Polls Cafe OS process events. Transitions permanently to Exiting once
//! WHBProcIsRunning() returns false, and tracks Paused vs Running via ProcUIInForeground().
SystemEvent WutPlatform::getSystemEvent()
{
	// Once latched in Exiting, always return ShutdownRequested
	if (status == Status::Exiting)
		return SystemEvent::ShutdownRequested;

	// WHBProcIsRunning() pumps the ProcUI message queue - only call this once per frame
	if (!WHBProcIsRunning())
	{
		status = Status::Exiting;
		return SystemEvent::ShutdownRequested;
	}

	// Fast in-memory check for focus/foreground state
	if (!ProcUIInForeground())
	{
		status = Status::Paused;
	}
	else
	{
		status = Status::Running;
	}

	return SystemEvent::None;
}

/****************************************************************************
 * Console/memory info
 ***************************************************************************/

// Espresso's nominal core clock (1.24325GHz) - used only if OSGetSystemInfo()
// ever returns a bogus/zero reading.
#define WIIU_FALLBACK_CORE_CLOCK_MHZ 1243

static uint32_t GetCPUSpeedMHz() {
	OSSystemInfo * info = OSGetSystemInfo();

	if (info && info->coreClockSpeed > 0)
		return info->coreClockSpeed / 1000000;

	return WIIU_FALLBACK_CORE_CLOCK_MHZ;
}

const char* WutPlatform::getConsoleDetails() {
	static char description[64];
	uint32_t mhz = GetCPUSpeedMHz();

	char speedStr[16];
	if (mhz >= 1000) {
		snprintf(speedStr, sizeof(speedStr), "%.2f GHz", mhz / 1000.0f);
	} else {
		snprintf(speedStr, sizeof(speedStr), "%u MHz", mhz);
	}

	snprintf(description, sizeof(description), "Wii U (%s)", speedStr);

	return description;
}

const char* WutPlatform::getMemoryFreeInfo() {
	static char memoryFreeInfo[50];

	uint32_t mem2Addr = 0, mem2TotalBytes = 0;
	OSGetMemBound(OS_MEM2, &mem2Addr, &mem2TotalBytes);

	struct mallinfo mi = mallinfo();
	uint32_t usedBytes = (uint32_t)mi.uordblks;

	uint32_t freeBytes = (mem2TotalBytes > usedBytes) ? (mem2TotalBytes - usedBytes) : 0;
	float free_mb = (float)freeBytes / (1024.0f * 1024.0f);

	snprintf(memoryFreeInfo, sizeof(memoryFreeInfo), "MEM free: %.2fMB", free_mb);

	return memoryFreeInfo;
}

// Either WHBProcIsRunning() returned false already and we're leaving
// because the OS requested it (eg: Close button was used in the Wii U
// menu), or the user explicitly exited from within the app - in which
// case we're still in the foreground and need to tell Cafe OS we're
// ready to shut down.
void WutPlatform::requestExit(int, bool)
{
	// If the exit was user-initiated, Cafe OS has not been notified yet.
	// SYSLaunchMenu() tells Cafe OS to switch back to the system menu or loader.
	if(ProcUIIsRunning()) {
		SYSLaunchMenu();

		// On real hardware this resolves within a frame or two. Cemu doesn't
		// implement SYSLaunchMenu(), so ProcUI never leaves the foreground and
		// this would spin forever - cap the wait so we can still exit cleanly there.
		const int timeoutMs = 2000;
		for (int waited = 0; WHBProcIsRunning() && waited < timeoutMs; waited++) {
			usleep(1000);
		}
	}
	this->shutdown();
	WHBProcShutdown();
	exit(0);
}

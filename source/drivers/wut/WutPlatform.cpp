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
#include <coreinit/foreground.h>
#include <coreinit/systeminfo.h>
#include <coreinit/memory.h>
#include <malloc.h>

#include "WutPlatform.h"
#include "../../libgui/GuiImageDataCache.h"
#include "imagelist.h"

void WutPlatform::init(const PlatformConfig& config)
{
	this->config = config;

	WHBProcInit();

	// WHBProcInit()'s save callback only acknowledges the OS. Use ours, so the app can save first
	ProcUISetSaveCallback(&WutPlatform::procSaveCallback, this);

	this->threadDriver = new WutThreadDriver();
	this->threadDriver->init();

	this->videoDriver = new WutVideoDriver();
	this->videoDriver->init(config.canvasWidth, config.canvasHeight);

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

	LogConfig logConfig;
	static const int deviceCandidates[] = { DEVICE_SD };

	const char * mountPath = FindFirstMountedPath(this->fileSystemDriver, deviceCandidates, 1);

	if(mountPath[0] != '\0') {
		// mountPath already ends in "/" (eg. "/vol/external01/") - no separator needed.
		snprintf(logConfig.filePath, sizeof(logConfig.filePath), "%sdebug.log", mountPath);
	}
	this->logger->init(logConfig);
#endif

	GuiImageDataCache::preload(guiImageAssets, guiImageAssetCount);
}

void WutPlatform::shutdown()
{
	GuiImageDataCache::shutdown();

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

//! Called by ProcUI, from inside the WHBProcIsRunning() pump, each time the OS
//! is about to take the foreground away - not only when closing (HOME menu then
//! resume lands here too), so this must only save, never tear anything down.
//! The OS doesn't take the foreground until this returns. The release callbacks
//! (which free GX2/MEM1) have already run, so nothing may be drawn.
uint32_t WutPlatform::procSaveCallback(void * context)
{
	WutPlatform * self = static_cast<WutPlatform *>(context);

	// We're losing the foreground. (Already Exiting means the app asked for this and has the foreground until we return)
	if (self->status == Status::Running)
		self->status = Status::Paused;

	if (self->saveHandler)
		self->saveHandler();

	OSSavesDone_ReadyToRelease();
	return 0;
}

//! Polls Cafe OS process events. Transitions permanently to Exiting (or Closed,
//! if we had already lost the foreground) once WHBProcIsRunning() returns false,
//! and tracks Paused vs Running via ProcUIInForeground().
SystemEvent WutPlatform::getSystemEvent()
{
	// Once latched in Exiting/Closed, always return ShutdownRequested
	if (isExiting())
		return SystemEvent::ShutdownRequested;

	// WHBProcIsRunning() pumps the ProcUI message queue - only call this once per frame
	if (!WHBProcIsRunning())
	{
		procExited = true;

		// The OS only asks us to close from the background: Paused here means the save callback has run
		status = (status == Status::Paused) ? Status::Closed : Status::Exiting;
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
	if(!procExited) {
		SYSLaunchMenu();

		// On real hardware this resolves within a frame or two. Cemu doesn't
		// implement SYSLaunchMenu(), so ProcUI never leaves the foreground and
		// this would spin forever - cap the wait so we can still exit cleanly there.
		const int timeoutMs = 2000;
		for (int waited = 0; waited < timeoutMs; waited++) {
			if (!WHBProcIsRunning()) {
				procExited = true;
				break;
			}
			usleep(1000);
		}
	}

	if(!procExited) {
		ProcUIShutdown();
	}

	this->shutdown();
	WHBProcShutdown();
	exit(0);
}

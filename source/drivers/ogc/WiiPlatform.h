/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * WiiPlatform.h
 ***************************************************************************/
#pragma once

#include <stdint.h>

#include "../Platform.h"
#include "OgcVideoDriver.h"
#include "OgcInputDriver.h"
#include "OgcThreadDriver.h"
#include "WiiAudioDriver.h"
#include "WiiFileSystemDriver.h"
#include "OgcLoggerSysReport.h"
#include "OgcLoggerUdp.h"
#include "OgcLoggerUsbGecko.h"
#include "../LoggerFile.h"

enum {
	EXITACTION_WII_AUTO = 0,
	EXITACTION_WII_RETURN_TO_MENU,
	EXITACTION_WII_POWER_OFF,
	EXITACTION_WII_RETURN_TO_LOADER,
	EXITACTION_WII_LENGTH
};

bool SupportedIOS(uint32_t ios);
bool SaneIOS(uint32_t ios);

void NotifyWiiShutdownRequested();

class WiiPlatform : public Platform
{
	public:
		WiiPlatform() {}

		void init(int width, int height) override;
		void shutdown() override;

		SystemEvent getSystemEvent() override;
		Status getStatus() const override { return status; }
		void triggerExit() override { status = Status::Exiting; }

		const char* getConsoleDetails() override;
		const char* getMemoryFreeInfo() override;

		void requestExit(int exitAction, bool autoloadedGame) override;
		
		AudioDriver* getAudio() override { return audioDriver; }
		VideoDriver* getVideo() override { return videoDriver; }
		InputDriver* getInput() override { return inputDriver; }
		FileSystemDriver* getFileSystem() override { return fileSystemDriver; }
		ThreadDriver* getThread() override { return threadDriver; }
		Logger* getLogger() override { return logger; }

	private:
		Status status = Status::Running;
		WiiAudioDriver* audioDriver = nullptr;
		OgcVideoDriver* videoDriver = nullptr;
		OgcInputDriver* inputDriver = nullptr;
		WiiFileSystemDriver* fileSystemDriver = nullptr;
		OgcThreadDriver* threadDriver = nullptr;
		Logger* logger = nullptr;
};

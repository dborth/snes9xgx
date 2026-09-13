/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * GameCubePlatform.h
 ***************************************************************************/
#pragma once

#include "../Platform.h"
#include "OgcVideoDriver.h"
#include "OgcInputDriver.h"
#include "OgcThreadDriver.h"
#include "GameCubeAudioDriver.h"
#include "GameCubeFileSystemDriver.h"
#include "OgcLoggerSysReport.h"
#include "OgcLoggerUsbGecko.h"
#include "../LoggerFile.h"

enum {
	EXITACTION_GC_RETURN_TO_LOADER = 0,
	EXITACTION_GC_REBOOT,
	EXITACTION_GC_LENGTH
};

class GameCubePlatform : public Platform
{
	public:
		GameCubePlatform() {}

		void init(int width, int height) override;
		void shutdown() override;
		
		SystemEvent getSystemEvent() override { return SystemEvent::None; }
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
		GameCubeAudioDriver* audioDriver = nullptr;
		OgcVideoDriver* videoDriver = nullptr;
		OgcInputDriver* inputDriver = nullptr;
		GameCubeFileSystemDriver* fileSystemDriver = nullptr;
		OgcThreadDriver* threadDriver = nullptr;
		Logger* logger = nullptr;
};

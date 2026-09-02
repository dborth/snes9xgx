/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcPlatform.h
 ***************************************************************************/
#pragma once

#include "../Platform.h"
#include "OgcVideoDriver.h"
#include "OgcInputDriver.h"
#include "OgcThreadDriver.h"
#ifdef HW_DOL
#include "GameCubeAudioDriver.h"
#include "GameCubeFileSystemDriver.h"
#else
#include "WiiAudioDriver.h"
#include "WiiFileSystemDriver.h"
#endif

class OgcPlatform : public Platform
{
	public:
		OgcPlatform()
			: audioDriver(nullptr), videoDriver(nullptr), inputDriver(nullptr), fileSystemDriver(nullptr), threadDriver(nullptr) {}

		void init(int width, int height) override
		{
			this->threadDriver = new OgcThreadDriver();
			this->threadDriver->init();

			this->videoDriver = new OgcVideoDriver();
			this->videoDriver->init(width, height);

#ifdef HW_DOL
			this->audioDriver = new GameCubeAudioDriver();
#else
			this->audioDriver = new WiiAudioDriver();
#endif
			this->audioDriver->init();

			this->inputDriver = new OgcInputDriver();
			this->inputDriver->init();

#ifdef HW_DOL
			this->fileSystemDriver = new GameCubeFileSystemDriver();
#else
			this->fileSystemDriver = new WiiFileSystemDriver();
#endif
			this->fileSystemDriver->init();
		}

		void shutdown() override
		{
			if (fileSystemDriver) {
				fileSystemDriver->shutdown();
				delete fileSystemDriver;
				fileSystemDriver = nullptr;
			}

			if (inputDriver) {
				inputDriver->shutdown();
				delete inputDriver;
				inputDriver = nullptr;
			}

			if (audioDriver) {
				audioDriver->shutdown();
				delete audioDriver;
				audioDriver = nullptr;
			}

			if (videoDriver) {
				videoDriver->shutdown();
				delete videoDriver;
				videoDriver = nullptr;
			}

			if (threadDriver) {
				threadDriver->shutdown();
				delete threadDriver;
				threadDriver = nullptr;
			}
			exit(0);
		}

		bool shutdownRequested() override
		{
			return false;
		}

		AudioDriver* getAudio() override { return audioDriver; }
		VideoDriver* getVideo() override { return videoDriver; }
		InputDriver* getInput() override { return inputDriver; }
		FileSystemDriver* getFileSystem() override { return fileSystemDriver; }
		ThreadDriver* getThread() override { return threadDriver; }

	private:
		AudioDriver* audioDriver;
		OgcVideoDriver* videoDriver;
		OgcInputDriver* inputDriver;
		FileSystemDriver* fileSystemDriver;
		OgcThreadDriver* threadDriver;
};

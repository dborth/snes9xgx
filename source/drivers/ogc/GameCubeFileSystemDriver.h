/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * GameCubeFileSystemDriver.h
 ***************************************************************************/
#pragma once
#include "../FileSystemDriver.h"

class GameCubeFileSystemDriver : public FileSystemDriver
{
	public:
		void init() override;
		void shutdown() override;

		int enumerateStorageDevices(StorageDevice outDevices[MAX_STORAGE_DEVICES]) override;
		MountResult mountStorageDevice(int deviceId) override;
		const char * mountResultMessage(int deviceId, MountResult result) override;
		void invalidateStorageDevice(int deviceId) override;
		void pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged) override;
		bool hasRemovableStorageDevices() const override { return false; } // card slots/GC Loader aren't polled for removal

	private:
		MountResult mountFAT(int deviceId);
		MountResult mountDVD();
};

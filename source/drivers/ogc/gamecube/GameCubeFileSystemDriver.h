/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * GameCubeFileSystemDriver.h
 ***************************************************************************/
#pragma once
#include "../../FileSystemDriver.h"
#include "../OgcSmbDriver.h"

//!GameCube FileSystemDriver: memory card slots, GC Loader, and DVD, plus
//!DEVICE_SMB (broadband adapter) via the shared OgcSmbDriver.
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
		bool hasRemovableStorageDevices() const override { return true; }
		bool isDevicePresent(int deviceId) const override;

		const char * getMountPath(int device) const override;
		const int * getValidLoadDevices(int & outCount) const override;
		const int * getValidSaveDevices(int & outCount) const override;

		SmbDriver * getSmb() override { return &smbDriver; }

	private:
		MountResult mountFAT(int deviceId);
		MountResult mountDVD();

		OgcSmbDriver smbDriver;
};

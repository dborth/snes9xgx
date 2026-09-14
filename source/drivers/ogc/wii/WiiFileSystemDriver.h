/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * WiiFileSystemDriver.h
 ***************************************************************************/
#pragma once
#include "../../FileSystemDriver.h"
#include "../OgcSmbDriver.h"

//!Wii FileSystemDriver: hot-pluggable SD and up to 3 concurrent USB MSD
// plus DVD (ISO9660) and DEVICE_SMB via the shared OgcSmbDriver.
class WiiFileSystemDriver : public FileSystemDriver
{
	public:
		void init() override;
		void shutdown() override;

		int enumerateStorageDevices(StorageDevice outDevices[MAX_STORAGE_DEVICES]) override;
		MountResult mountStorageDevice(int deviceId) override;
		const char * mountResultMessage(int deviceId, MountResult result) override;
		void invalidateStorageDevice(int deviceId) override;
		void pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged) override;
		bool hasRemovableStorageDevices() const override { return true; } // SD/USB/DVD can all be pulled
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

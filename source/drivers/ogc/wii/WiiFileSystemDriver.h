/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * WiiFileSystemDriver.h
 ***************************************************************************/
#pragma once
#include "../OgcFileSystemDriver.h"

//!Wii FileSystemDriver: hot-pluggable SD and up to 3 concurrent USB MSD
//!plus DVD (ISO9660) and DEVICE_SMB.
class WiiFileSystemDriver : public OgcFileSystemDriver
{
	public:
		void init() override;

		const int * getValidLoadDevices(int & outCount) const override;
		const int * getValidSaveDevices(int & outCount) const override;

	protected:
		void shutdownPlatform() override;
		bool pollPlatformExtra() override;
		void prepareMount(int deviceId) override;
};

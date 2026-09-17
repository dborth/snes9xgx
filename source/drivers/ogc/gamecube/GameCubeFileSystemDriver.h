/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * GameCubeFileSystemDriver.h
 ***************************************************************************/
#pragma once
#include "../OgcFileSystemDriver.h"

//!GameCube FileSystemDriver: memory card slots, GC Loader, and DVD, plus
//!DEVICE_SMB (broadband adapter). Enumeration/mount/poll logic itself
//!lives in the shared OgcFileSystemDriver base.
class GameCubeFileSystemDriver : public OgcFileSystemDriver
{
	public:
		void init() override;

		const int * getValidLoadDevices(int & outCount) const override;
		const int * getValidSaveDevices(int & outCount) const override;
};

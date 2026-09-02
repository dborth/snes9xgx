/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * FileSystemDriver.h
 ***************************************************************************/
#pragma once

#define MAX_STORAGE_DEVICES 16

struct StorageDevice
{
	int  id;
	char name[16];
	char prefix[16];
	bool removable;          //!< can this device disappear at runtime? (polled by the device-checking thread)
	bool autoMountAtStartup; //!< silently attempted at boot (eg. Wii's SD/USB)
};

//! Result of a single mount attempt. Deliberately has no retry/backoff behavior baked in
enum class MountResult
{
	Success,
	DeviceNotFound, //!< not physically present / not inserted
	MountFailed     //!< present, but couldn't be mounted (eg. unrecognized format)
};

class FileSystemDriver
{
	public:
		virtual ~FileSystemDriver() = default;

		virtual void init() = 0;
		virtual void shutdown() = 0;

		//! Fills outDevices (size MAX_STORAGE_DEVICES) and returns the device count.
		virtual int enumerateStorageDevices(StorageDevice outDevices[MAX_STORAGE_DEVICES]) = 0;

		//! Attempts to mount deviceId exactly once. No retry, no prompts,
		//! no-ops (returns Success) if already mounted.
		virtual MountResult mountStorageDevice(int deviceId) = 0;

		//! A short, user-displayable reason for a non-Success MountResult
		//! (eg. "SD card not found!"). Never returns nullptr.
		virtual const char * mountResultMessage(int deviceId, MountResult result) = 0;

		//! Marks deviceId as needing a fresh mount next time
		//! mountStorageDevice() is called, eg. after a read/write failure
		//! suggests the underlying media went away. Does no I/O itself.
		virtual void invalidateStorageDevice(int deviceId) = 0;

		//! Called once per device-checking thread cycle.
		//! removedIds/outRemovedCount: devices that were mounted and have
		//! now disappeared (already invalidated internally - callers just
		//! need to react, eg. abort an in-progress directory parse).
		//! deviceListChanged: true if enumerateStorageDevices() should be
		//! re-run because the device table itself changed shape.
		virtual void pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged) = 0;

		//! Whether the device-checking thread should run on this platform
		virtual bool hasRemovableStorageDevices() const = 0;
};

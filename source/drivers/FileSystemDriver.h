/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * FileSystemDriver.h
 ***************************************************************************/
#pragma once
#include <stddef.h>
#include <stdio.h>

#include "SmbDriver.h"

#define MAX_STORAGE_DEVICES 16

//!Storage device kind, shared by every platform's FileSystemDriver
//!All platforms are limited to exactly one mount per device type
enum Device
{
	DEVICE_AUTO = 0,
	DEVICE_SD,
	DEVICE_USB,
	DEVICE_DVD,
	DEVICE_SMB,
	DEVICE_SD_SLOTA,     //!< GameCube memory card slot A
	DEVICE_SD_SLOTB,     //!< GameCube memory card slot B
	DEVICE_SD_PORT2,     //!< GameCube SD Gecko in memory card slot B
	DEVICE_SD_GCLOADER,
	DEVICE_LENGTH
};

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

		//! devoptab-style mount path for device (eg. "sd:/"), or "" if
		//! device isn't recognized or currently mounted on this platform.
		virtual const char * getMountPath(int device) const = 0;

		//! Writes getMountPath(device) + suffix into out (bounds-checked to
		//! sizeof(out) via the array-reference template parameter N).
		template<size_t N>
		void getPath(char (&out)[N], int device, const char * suffix) const
		{
			getPath(out, N, device, suffix);
		}

		//! Joins a folder and a filename with '/' after the mount path.
		template<size_t N>
		void getPath(char (&out)[N], int device, const char * folder, const char * file) const
		{
			getPath(out, N, device, folder, file);
		}

		//! Explicit-size equivalents of the two templates above, for call
		//! sites where the destination buffer arrives as a `char *`
		//! function parameter rather than a fixed array.
		void getPath(char * out, size_t outSize, int device, const char * suffix) const
		{
			snprintf(out, outSize, "%s%s", getMountPath(device), suffix ? suffix : "");
		}

		void getPath(char * out, size_t outSize, int device, const char * folder, const char * file) const
		{
			snprintf(out, outSize, "%s%s/%s", getMountPath(device), folder ? folder : "", file ? file : "");
		}

		virtual const int * getValidLoadDevices(int & outCount) const = 0;
		virtual const int * getValidSaveDevices(int & outCount) const = 0;

		virtual SmbDriver * getSmb() = 0;
};

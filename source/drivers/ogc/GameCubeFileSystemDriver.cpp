/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * GameCubeFileSystemDriver.cpp
 *
 * GameCube storage device enumeration + mounting: SD Gecko slots A/B,
 * SD2SP2 (port2), GC Loader, and DVD. None of these are polled.
 ***************************************************************************/
#include <string.h>
#include <fat.h>
#include <sdcard/gcsd.h>
#include <ogc/dvd.h>
#include <iso9660.h>

#include "GameCubeFileSystemDriver.h"
#include "OgcDeviceTypes.h"

static DISC_INTERFACE* dvd      = &__io_gcdvd;
static DISC_INTERFACE* gcloader = &__io_gcode;

static bool isMounted[MAX_STORAGE_DEVICES]       = { false };
static bool unmountRequired[MAX_STORAGE_DEVICES] = { false };

void GameCubeFileSystemDriver::init()
{
	DVD_Init();
}

void GameCubeFileSystemDriver::shutdown()
{
	fatUnmount("port2:");
	fatUnmount("carda:");
	fatUnmount("cardb:");
	fatUnmount("gcloader:");
}

int GameCubeFileSystemDriver::enumerateStorageDevices(StorageDevice outDevices[MAX_STORAGE_DEVICES])
{
	int count = 0;
	outDevices[count++] = StorageDevice{ DEVICE_SD_SLOTA,    "carda",    "carda:/",    false, false };
	outDevices[count++] = StorageDevice{ DEVICE_SD_SLOTB,    "cardb",    "cardb:/",    false, false };
	outDevices[count++] = StorageDevice{ DEVICE_SD_PORT2,    "port2",    "port2:/",    false, false };
	outDevices[count++] = StorageDevice{ DEVICE_SD_GCLOADER, "gcloader", "gcloader:/", false, false };
	outDevices[count++] = StorageDevice{ DEVICE_DVD,         "",         "dvd:/",      false, false };
	return count;
}

static const char * FatDeviceName(int deviceId, char name[10], char mountPoint[10])
{
	switch(deviceId)
	{
		case DEVICE_SD_SLOTA:    strcpy(name, "carda");    strcpy(mountPoint, "carda:");    return name;
		case DEVICE_SD_SLOTB:    strcpy(name, "cardb");    strcpy(mountPoint, "cardb:");    return name;
		case DEVICE_SD_PORT2:    strcpy(name, "port2");    strcpy(mountPoint, "port2:");    return name;
		case DEVICE_SD_GCLOADER: strcpy(name, "gcloader"); strcpy(mountPoint, "gcloader:"); return name;
		default: return nullptr;
	}
}

static DISC_INTERFACE * FatDisc(int deviceId)
{
	switch(deviceId)
	{
		case DEVICE_SD_SLOTA:    return get_io_gcsda();
		case DEVICE_SD_SLOTB:    return get_io_gcsdb();
		case DEVICE_SD_PORT2:    return get_io_gcsd2();
		case DEVICE_SD_GCLOADER: return gcloader;
		default: return nullptr;
	}
}

MountResult GameCubeFileSystemDriver::mountFAT(int deviceId)
{
	char name[10], mountPoint[10];

	if(!FatDeviceName(deviceId, name, mountPoint))
		return MountResult::DeviceNotFound;

	DISC_INTERFACE * disc = FatDisc(deviceId);

	if(unmountRequired[deviceId])
	{
		unmountRequired[deviceId] = false;
		fatUnmount(mountPoint);
		disc->shutdown(disc);
		isMounted[deviceId] = false;
	}

	bool mounted = fatMountSimple(name, disc);
	isMounted[deviceId] = mounted;
	return mounted ? MountResult::Success : MountResult::DeviceNotFound;
}

MountResult GameCubeFileSystemDriver::mountDVD()
{
	if(unmountRequired[DEVICE_DVD])
	{
		unmountRequired[DEVICE_DVD] = false;
		ISO9660_Unmount("dvd:");
	}

	DVD_Mount();

	if(!dvd->isInserted(dvd))
	{
		isMounted[DEVICE_DVD] = false;
		return MountResult::DeviceNotFound;
	}

	if(!ISO9660_Mount("dvd", dvd))
	{
		isMounted[DEVICE_DVD] = false;
		return MountResult::MountFailed;
	}

	isMounted[DEVICE_DVD] = true;
	return MountResult::Success;
}

MountResult GameCubeFileSystemDriver::mountStorageDevice(int deviceId)
{
	if(isMounted[deviceId])
		return MountResult::Success;

	switch(deviceId)
	{
		case DEVICE_SD_SLOTA:
		case DEVICE_SD_SLOTB:
		case DEVICE_SD_PORT2:
		case DEVICE_SD_GCLOADER:
			return mountFAT(deviceId);
		case DEVICE_DVD:
			return mountDVD();
		default:
			return MountResult::DeviceNotFound; // not ours - eg. DEVICE_SMB is network, handled by fileop.cpp directly
	}
}

const char * GameCubeFileSystemDriver::mountResultMessage(int deviceId, MountResult result)
{
	if(result == MountResult::MountFailed)
		return "Unrecognized DVD format.";

	switch(deviceId)
	{
		case DEVICE_SD_SLOTA:
		case DEVICE_SD_SLOTB:
		case DEVICE_SD_PORT2:
		case DEVICE_SD_GCLOADER:
			return "SD card not found!";
		case DEVICE_DVD: return "No disc inserted!";
		default:         return "Device not found!";
	}
}

void GameCubeFileSystemDriver::invalidateStorageDevice(int deviceId)
{
	if(deviceId < 0 || deviceId >= MAX_STORAGE_DEVICES)
		return;

	isMounted[deviceId] = false;
	unmountRequired[deviceId] = true;
}

void GameCubeFileSystemDriver::pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged)
{
	outRemovedCount = 0;
	deviceListChanged = false;
}

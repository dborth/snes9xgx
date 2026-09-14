/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * GameCubeFileSystemDriver.cpp
 *
 * GameCube storage device enumeration + mounting: SD Gecko slots A/B,
 * SD2SP2 (port2), GC Loader, and DVD. Slots A/B and port2 are polled for
 * hotplug via a real EXI presence probe (see isPresentCache below); GC
 * Loader and DVD are not.
 ***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <fat.h>
#include <sdcard/gcsd.h>
#include <ogc/dvd.h>
#include <iso9660.h>

#include "GameCubeFileSystemDriver.h"

static DISC_INTERFACE* dvd      = &__io_gcdvd;
static DISC_INTERFACE* gcloader = &__io_gcode;

static bool isMounted[MAX_STORAGE_DEVICES]       = { false };
static bool unmountRequired[MAX_STORAGE_DEVICES] = { false };
static char volumeLabel[MAX_STORAGE_DEVICES][16] = { { 0 } };

// Cached hardware-presence per device, refreshed once at init() and then
// every pollStorageDevices() cycle - see isDevicePresent().
static bool isPresentCache[MAX_STORAGE_DEVICES] = { false };

void GameCubeFileSystemDriver::init()
{
	DVD_Init();
	smbDriver.init();

	isPresentCache[DEVICE_SD_SLOTA] = get_io_gcsda()->isInserted(get_io_gcsda());
	isPresentCache[DEVICE_SD_SLOTB] = get_io_gcsdb()->isInserted(get_io_gcsdb());
	isPresentCache[DEVICE_SD_PORT2] = get_io_gcsd2()->isInserted(get_io_gcsd2());
	isPresentCache[DEVICE_DVD]      = dvd->isInserted(dvd);

	StorageDevice devices[MAX_STORAGE_DEVICES];
	int count = enumerateStorageDevices(devices);

	for(int i = 0; i < count; i++)
		if(devices[i].autoMountAtStartup)
			mountStorageDevice(devices[i].id);
}

void GameCubeFileSystemDriver::shutdown()
{
	smbDriver.shutdown();
	fatUnmount("port2:");
	fatUnmount("carda:");
	fatUnmount("cardb:");
	fatUnmount("gcloader:");
}

static void CopyLabel(StorageDevice & out, int deviceId)
{
	snprintf(out.label, sizeof(out.label), "%s", volumeLabel[deviceId]);
}

int GameCubeFileSystemDriver::enumerateStorageDevices(StorageDevice outDevices[MAX_STORAGE_DEVICES])
{
	int count = 0;
	// autoMountAtStartup: true for carda/cardb/port2 - all three go through
	// the same cheap, safe EXI presence probe.
	outDevices[count] = StorageDevice{ DEVICE_SD_SLOTA,    "SD Gecko Slot A", "carda:/",    false, true,  0, 0, 0, false, false, "", false }; CopyLabel(outDevices[count], DEVICE_SD_SLOTA);    count++;
	outDevices[count] = StorageDevice{ DEVICE_SD_SLOTB,    "SD Gecko Slot B", "cardb:/",    false, true,  0, 0, 0, false, false, "", false }; CopyLabel(outDevices[count], DEVICE_SD_SLOTB);    count++;
	outDevices[count] = StorageDevice{ DEVICE_SD_PORT2,    "SD in SP2",       "port2:/",    false, true,  0, 0, 0, false, false, "", false }; CopyLabel(outDevices[count], DEVICE_SD_PORT2);    count++;
	outDevices[count] = StorageDevice{ DEVICE_SD_GCLOADER, "GC Loader",       "gcloader:/", false, false, 0, 0, 0, false, false, "", true  }; CopyLabel(outDevices[count], DEVICE_SD_GCLOADER); count++;
	outDevices[count] = StorageDevice{ DEVICE_DVD,         "Data DVD",        "dvd:/",      false, false, 0, 0, 0, false, false, "", true  }; count++;
	outDevices[count] = StorageDevice{ DEVICE_SMB,         "Network Share",   "smb:/",      false, false, 0, 0, 0, false, false, "", true  }; count++;

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

	// Distinguish "nothing there" from "something's there but we can't read it"
	if(!disc->startup(disc) || !disc->isInserted(disc))
	{
		isMounted[deviceId] = false;
		volumeLabel[deviceId][0] = '\0';
		return MountResult::DeviceNotFound;
	}

	bool mounted = fatMountSimple(name, disc);
	isMounted[deviceId] = mounted;

	if(mounted)
		fatGetVolumeLabel(mountPoint, volumeLabel[deviceId]);
	else
		volumeLabel[deviceId][0] = '\0';

	return mounted ? MountResult::Success : MountResult::MountFailed;
}

MountResult GameCubeFileSystemDriver::mountDVD()
{
	if(unmountRequired[DEVICE_DVD])
	{
		unmountRequired[DEVICE_DVD] = false;
		ISO9660_Unmount("dvd:");
	}

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
	if(deviceId == DEVICE_SMB)
		return smbDriver.isConnected() ? MountResult::Success : MountResult::DeviceNotFound;

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
			return MountResult::DeviceNotFound;
	}
}

const char * GameCubeFileSystemDriver::mountResultMessage(int deviceId, MountResult result)
{
	if(result == MountResult::MountFailed)
	{
		switch(deviceId)
		{
			case DEVICE_SD_SLOTA:
			case DEVICE_SD_SLOTB:
			case DEVICE_SD_PORT2:
			case DEVICE_SD_GCLOADER:
				return "Unsupported format - please use FAT32/exFAT.";
			default: 
				return "Unrecognized DVD format.";
		}
	}

	switch(deviceId)
	{
		case DEVICE_SD_SLOTA:
		case DEVICE_SD_SLOTB:
		case DEVICE_SD_PORT2:
		case DEVICE_SD_GCLOADER:
			return "SD card not found!";
		case DEVICE_DVD: return "No disc inserted!";
		case DEVICE_SMB: return "Network share not connected!";
		default:         return "Device not found!";
	}
}

void GameCubeFileSystemDriver::invalidateStorageDevice(int deviceId)
{
	if(deviceId < 0 || deviceId >= MAX_STORAGE_DEVICES)
		return;

	isMounted[deviceId] = false;
	unmountRequired[deviceId] = true;
	volumeLabel[deviceId][0] = '\0';
}

void GameCubeFileSystemDriver::pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged)
{
	outRemovedCount = 0;
	deviceListChanged = false;

	DISC_INTERFACE * discA = get_io_gcsda();
	DISC_INTERFACE * discB = get_io_gcsdb();
	DISC_INTERFACE * discP2 = get_io_gcsd2();

	bool slotAPresent = discA->isInserted(discA);
	bool slotBPresent = discB->isInserted(discB);
	bool port2Present = discP2->isInserted(discP2);
	bool dvdPresent    = dvd->isInserted(dvd);

	if(slotAPresent != isPresentCache[DEVICE_SD_SLOTA])
	{
		isPresentCache[DEVICE_SD_SLOTA] = slotAPresent;
		deviceListChanged = true;
	}
	if(slotBPresent != isPresentCache[DEVICE_SD_SLOTB])
	{
		isPresentCache[DEVICE_SD_SLOTB] = slotBPresent;
		deviceListChanged = true;
	}
	if(port2Present != isPresentCache[DEVICE_SD_PORT2])
	{
		isPresentCache[DEVICE_SD_PORT2] = port2Present;
		deviceListChanged = true;
	}
	isPresentCache[DEVICE_DVD] = dvdPresent;

	if(isMounted[DEVICE_SD_SLOTA] && !slotAPresent)
	{
		invalidateStorageDevice(DEVICE_SD_SLOTA);
		removedIds[outRemovedCount++] = DEVICE_SD_SLOTA;
	}
	if(isMounted[DEVICE_SD_SLOTB] && !slotBPresent)
	{
		invalidateStorageDevice(DEVICE_SD_SLOTB);
		removedIds[outRemovedCount++] = DEVICE_SD_SLOTB;
	}
	if(isMounted[DEVICE_SD_PORT2] && !port2Present)
	{
		invalidateStorageDevice(DEVICE_SD_PORT2);
		removedIds[outRemovedCount++] = DEVICE_SD_PORT2;
	}
	if(isMounted[DEVICE_DVD] && !dvdPresent)
	{
		invalidateStorageDevice(DEVICE_DVD);
		removedIds[outRemovedCount++] = DEVICE_DVD;
	}
}

bool GameCubeFileSystemDriver::isDevicePresent(int deviceId) const
{
	switch(deviceId)
	{
		case DEVICE_SD_SLOTA:    return isPresentCache[DEVICE_SD_SLOTA];
		case DEVICE_SD_SLOTB:    return isPresentCache[DEVICE_SD_SLOTB];
		case DEVICE_SD_PORT2:    return isPresentCache[DEVICE_SD_PORT2];
		case DEVICE_SD_GCLOADER: return false;
		case DEVICE_DVD:         return isPresentCache[DEVICE_DVD]; // informational only - DVD is alwaysListed
		case DEVICE_SMB:         return smbDriver.isConnected();    // informational only - SMB is alwaysListed
		default:                 return false;
	}
}

//!Mount-path lookup, keyed by the shared Device enum.
static const char * const mountPath[DEVICE_LENGTH] =
{
	"",         // DEVICE_AUTO
	"",         // DEVICE_SD
	"",         // DEVICE_USB
	"",         // DEVICE_USB2
	"",         // DEVICE_USB3
	"dvd:/",    // DEVICE_DVD
	"",         // DEVICE_SMB
	"carda:/",  // DEVICE_SD_SLOTA
	"cardb:/",  // DEVICE_SD_SLOTB
	"port2:/",  // DEVICE_SD_PORT2
	"gcloader:/" // DEVICE_SD_GCLOADER
};

const char * GameCubeFileSystemDriver::getMountPath(int device) const
{
	if(device == DEVICE_SMB)
		return smbDriver.getMountPath();

	if(device < 0 || device >= DEVICE_LENGTH || !isMounted[device])
		return "";
	return mountPath[device];
}

const int * GameCubeFileSystemDriver::getValidLoadDevices(int & outCount) const
{
	static const int devices[] = { DEVICE_AUTO, DEVICE_SD_SLOTA, DEVICE_SD_SLOTB, DEVICE_SD_PORT2, DEVICE_SD_GCLOADER, DEVICE_DVD, DEVICE_SMB };
	outCount = sizeof(devices) / sizeof(devices[0]);
	return devices;
}

const int * GameCubeFileSystemDriver::getValidSaveDevices(int & outCount) const
{
	static const int devices[] = { DEVICE_AUTO, DEVICE_SD_SLOTA, DEVICE_SD_SLOTB, DEVICE_SD_PORT2, DEVICE_SD_GCLOADER, DEVICE_SMB };
	outCount = sizeof(devices) / sizeof(devices[0]);
	return devices;
}

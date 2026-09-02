/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcFileSystemDriver.cpp
 *
 * Wii/GameCube storage device enumeration + mounting
 ***************************************************************************/
#include <string.h>
#include <fat.h>
#ifdef HW_RVL
#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>
#include <di/di.h>
#else
#include <sdcard/gcsd.h>
#endif
#include <ogc/dvd.h>
#include <iso9660.h>

#include "OgcFileSystemDriver.h"
#include "OgcDeviceTypes.h"

#ifdef HW_RVL
	static DISC_INTERFACE* sd  = &__io_wiisd;
	static DISC_INTERFACE* usb = &__io_usbstorage;
	static DISC_INTERFACE* dvd = &__io_wiidvd;
#else
	static DISC_INTERFACE* dvd      = &__io_gcdvd;
	static DISC_INTERFACE* gcloader = &__io_gcode;
#endif

static bool isMounted[MAX_STORAGE_DEVICES]       = { false };
static bool unmountRequired[MAX_STORAGE_DEVICES] = { false };

void OgcFileSystemDriver::init()
{

}

void OgcFileSystemDriver::shutdown()
{
#ifdef HW_RVL
	fatUnmount("sd:");
	fatUnmount("usb:");
	DI_Close();
#else
	fatUnmount("port2:");
	fatUnmount("carda:");
	fatUnmount("cardb:");
	fatUnmount("gcloader:");
#endif
}

int OgcFileSystemDriver::enumerateStorageDevices(StorageDevice outDevices[MAX_STORAGE_DEVICES])
{
	int count = 0;

#ifdef HW_RVL
	outDevices[count++] = StorageDevice{ DEVICE_SD,  "sd",  "sd:/",  true,  true  };
	outDevices[count++] = StorageDevice{ DEVICE_USB, "usb", "usb:/", true,  true  };
	outDevices[count++] = StorageDevice{ DEVICE_DVD, "",    "dvd:/", true,  false };
#else
	outDevices[count++] = StorageDevice{ DEVICE_SD_SLOTA,    "carda",    "carda:/",    false, false };
	outDevices[count++] = StorageDevice{ DEVICE_SD_SLOTB,    "cardb",    "cardb:/",    false, false };
	outDevices[count++] = StorageDevice{ DEVICE_SD_PORT2,    "port2",    "port2:/",    false, false };
	outDevices[count++] = StorageDevice{ DEVICE_SD_GCLOADER, "gcloader", "gcloader:/", false, false };
	outDevices[count++] = StorageDevice{ DEVICE_DVD,         "",         "dvd:/",      false, false };
#endif

	return count;
}

bool OgcFileSystemDriver::hasRemovableStorageDevices() const
{
#ifdef HW_RVL
	return true;
#else
	return false;
#endif
}

// Looks up the libfat short name + libfat mount point ("sd" / "sd:") for
// a FAT-mountable device id. Returns nullptr for ids this driver doesn't
// handle as FAT (eg. DEVICE_DVD, DEVICE_SMB).
static const char * FatDeviceName(int deviceId, char name[10], char mountPoint[10])
{
	switch(deviceId)
	{
#ifdef HW_RVL
		case DEVICE_SD:  strcpy(name, "sd");  strcpy(mountPoint, "sd:");  return name;
		case DEVICE_USB: strcpy(name, "usb"); strcpy(mountPoint, "usb:"); return name;
#else
		case DEVICE_SD_SLOTA:    strcpy(name, "carda");    strcpy(mountPoint, "carda:");    return name;
		case DEVICE_SD_SLOTB:    strcpy(name, "cardb");    strcpy(mountPoint, "cardb:");    return name;
		case DEVICE_SD_PORT2:    strcpy(name, "port2");    strcpy(mountPoint, "port2:");    return name;
		case DEVICE_SD_GCLOADER: strcpy(name, "gcloader"); strcpy(mountPoint, "gcloader:"); return name;
#endif
		default: return nullptr;
	}
}

static DISC_INTERFACE * FatDisc(int deviceId)
{
	switch(deviceId)
	{
#ifdef HW_RVL
		case DEVICE_SD:  return sd;
		case DEVICE_USB: return usb;
#else
		case DEVICE_SD_SLOTA:    return get_io_gcsda();
		case DEVICE_SD_SLOTB:    return get_io_gcsdb();
		case DEVICE_SD_PORT2:    return get_io_gcsd2();
		case DEVICE_SD_GCLOADER: return gcloader;
#endif
		default: return nullptr;
	}
}

MountResult OgcFileSystemDriver::mountFAT(int deviceId)
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

MountResult OgcFileSystemDriver::mountDVD()
{
	if(unmountRequired[DEVICE_DVD])
	{
		unmountRequired[DEVICE_DVD] = false;
		ISO9660_Unmount("dvd:");
	}

#ifdef HW_DOL
	DVD_Mount();
#endif

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

MountResult OgcFileSystemDriver::mountStorageDevice(int deviceId)
{
	if(isMounted[deviceId])
		return MountResult::Success;

	switch(deviceId)
	{
#ifdef HW_RVL
		case DEVICE_SD:
		case DEVICE_USB:
#else
		case DEVICE_SD_SLOTA:
		case DEVICE_SD_SLOTB:
		case DEVICE_SD_PORT2:
		case DEVICE_SD_GCLOADER:
#endif
			return mountFAT(deviceId);
		case DEVICE_DVD:
			return mountDVD();
		default:
			return MountResult::DeviceNotFound; // not ours - eg. DEVICE_SMB is network, handled by fileop.cpp directly
	}
}

const char * OgcFileSystemDriver::mountResultMessage(int deviceId, MountResult result)
{
	if(result == MountResult::MountFailed)
		return "Unrecognized DVD format.";

	switch(deviceId)
	{
#ifdef HW_RVL
		case DEVICE_SD:  return "SD card not found!";
		case DEVICE_USB: return "USB drive not found!";
#else
		case DEVICE_SD_SLOTA:
		case DEVICE_SD_SLOTB:
		case DEVICE_SD_PORT2:
		case DEVICE_SD_GCLOADER:
			return "SD card not found!";
#endif
		case DEVICE_DVD: return "No disc inserted!";
		default:         return "Device not found!";
	}
}

void OgcFileSystemDriver::invalidateStorageDevice(int deviceId)
{
	if(deviceId < 0 || deviceId >= MAX_STORAGE_DEVICES)
		return;

	isMounted[deviceId] = false;
	unmountRequired[deviceId] = true;
}

void OgcFileSystemDriver::pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged)
{
	outRemovedCount = 0;
	deviceListChanged = false; // Wii/GC's device table never changes shape

#ifdef HW_RVL
	if(isMounted[DEVICE_SD] && !sd->isInserted(sd))
	{
		invalidateStorageDevice(DEVICE_SD);
		removedIds[outRemovedCount++] = DEVICE_SD;
	}

	if(isMounted[DEVICE_USB] && !usb->isInserted(usb))
	{
		invalidateStorageDevice(DEVICE_USB);
		removedIds[outRemovedCount++] = DEVICE_USB;
	}

	if(isMounted[DEVICE_DVD] && !dvd->isInserted(dvd))
	{
		invalidateStorageDevice(DEVICE_DVD);
		removedIds[outRemovedCount++] = DEVICE_DVD;
	}
#endif
}

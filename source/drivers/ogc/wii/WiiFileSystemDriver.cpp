/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * WiiFileSystemDriver.cpp
 *
 * Wii storage device enumeration + mounting: SD, USB, DVD. All three are
 * hot-pluggable.
 ***************************************************************************/
#include <string.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>
#include <di/di.h>
#include <ogc/dvd.h>
#include <iso9660.h>

#include "WiiFileSystemDriver.h"

static DISC_INTERFACE* sd  = &__io_wiisd;
static DISC_INTERFACE* usb = &__io_usbstorage;
static DISC_INTERFACE* dvd = &__io_wiidvd;

static bool isMounted[MAX_STORAGE_DEVICES]       = { false };
static bool unmountRequired[MAX_STORAGE_DEVICES] = { false };

void WiiFileSystemDriver::init()
{
	DI_Init();
	USBStorage_Initialize();
	smbDriver.init();
}

void WiiFileSystemDriver::shutdown()
{
	smbDriver.shutdown();
	fatUnmount("sd:");
	fatUnmount("usb:");
	USBStorage_Deinitialize();
	DI_Close();
}

int WiiFileSystemDriver::enumerateStorageDevices(StorageDevice outDevices[MAX_STORAGE_DEVICES])
{
	int count = 0;
	outDevices[count++] = StorageDevice{ DEVICE_SD,  "sd",  "sd:/",  true, true  };
	outDevices[count++] = StorageDevice{ DEVICE_USB, "usb", "usb:/", true, true  };
	outDevices[count++] = StorageDevice{ DEVICE_DVD, "",    "dvd:/", true, false };
	outDevices[count++] = StorageDevice{ DEVICE_SMB, "network", "smb:/", false, false }; // not polled for removal, never auto-mounted at boot
	return count;
}

static const char * FatDeviceName(int deviceId, char name[10], char mountPoint[10])
{
	switch(deviceId)
	{
		case DEVICE_SD:  strcpy(name, "sd");  strcpy(mountPoint, "sd:");  return name;
		case DEVICE_USB: strcpy(name, "usb"); strcpy(mountPoint, "usb:"); return name;
		default: return nullptr;
	}
}

static DISC_INTERFACE * FatDisc(int deviceId)
{
	switch(deviceId)
	{
		case DEVICE_SD:  return sd;
		case DEVICE_USB: return usb;
		default: return nullptr;
	}
}

MountResult WiiFileSystemDriver::mountFAT(int deviceId)
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

MountResult WiiFileSystemDriver::mountDVD()
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

MountResult WiiFileSystemDriver::mountStorageDevice(int deviceId)
{
	// DEVICE_SMB isn't mounted here - actually connecting requires
	// credentials (host/share/user/password) that this generic interface
	// has no way to be handed, so ChangeInterface() calls
	// getSmb()->connect() directly with settings from the app instead.
	// This just reports current connection state, same as any other
	// already-mounted device.
	if(deviceId == DEVICE_SMB)
		return smbDriver.isConnected() ? MountResult::Success : MountResult::DeviceNotFound;

	if(isMounted[deviceId])
		return MountResult::Success;

	switch(deviceId)
	{
		case DEVICE_SD:
		case DEVICE_USB:
			return mountFAT(deviceId);
		case DEVICE_DVD:
			return mountDVD();
		default:
			return MountResult::DeviceNotFound;
	}
}

const char * WiiFileSystemDriver::mountResultMessage(int deviceId, MountResult result)
{
	if(result == MountResult::MountFailed)
		return "Unrecognized DVD format.";

	switch(deviceId)
	{
		case DEVICE_SD:  return "SD card not found!";
		case DEVICE_USB: return "USB drive not found!";
		case DEVICE_DVD: return "No disc inserted!";
		case DEVICE_SMB: return "Network share not connected!";
		default:         return "Device not found!";
	}
}

void WiiFileSystemDriver::invalidateStorageDevice(int deviceId)
{
	if(deviceId == DEVICE_SMB)
	{
		smbDriver.disconnect();
		return;
	}

	if(deviceId < 0 || deviceId >= MAX_STORAGE_DEVICES)
		return;

	isMounted[deviceId] = false;
	unmountRequired[deviceId] = true;
}

void WiiFileSystemDriver::pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged)
{
	outRemovedCount = 0;
	deviceListChanged = false;

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
}

//!Mount-path lookup, keyed by the shared Device enum. DEVICE_SMB isn't
//!here - its path depends on live connection state, so getMountPath()
//!below asks smbDriver directly rather than a fixed table entry.
static const char * const kMountPath[DEVICE_LENGTH] =
{
	"",       // DEVICE_AUTO
	"sd:/",   // DEVICE_SD
	"usb:/",  // DEVICE_USB
	"dvd:/",  // DEVICE_DVD
	"",       // DEVICE_SMB (unused - see above)
	"", "", "", ""
};

const char * WiiFileSystemDriver::getMountPath(int device) const
{
	if(device == DEVICE_SMB)
		return smbDriver.getMountPath();

	if(device < 0 || device >= DEVICE_LENGTH)
		return "";
	return kMountPath[device];
}

const int * WiiFileSystemDriver::getValidLoadDevices(int & outCount) const
{
	static const int devices[] = { DEVICE_AUTO, DEVICE_SD, DEVICE_USB, DEVICE_DVD, DEVICE_SMB };
	outCount = sizeof(devices) / sizeof(devices[0]);
	return devices;
}

const int * WiiFileSystemDriver::getValidSaveDevices(int & outCount) const
{
	static const int devices[] = { DEVICE_AUTO, DEVICE_SD, DEVICE_USB, DEVICE_SMB };
	outCount = sizeof(devices) / sizeof(devices[0]);
	return devices;
}

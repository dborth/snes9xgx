/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * WiiFileSystemDriver.cpp
 *
 * Wii storage device enumeration + mounting: SD, up to 3 concurrent USB
 * mass storage devices (via WiiUsbMulti / IOS58), and DVD. All are
 * hot-pluggable.
 ***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>
#include <di/di.h>
#include <ogc/dvd.h>
#include <iso9660.h>

#include "WiiFileSystemDriver.h"
#include "WiiUsbMulti.h"

static DISC_INTERFACE* sd  = &__io_wiisd;
static DISC_INTERFACE* dvd = &__io_wiidvd;

static bool isMounted[MAX_STORAGE_DEVICES]       = { false };
static bool unmountRequired[MAX_STORAGE_DEVICES] = { false };
static char volumeLabel[MAX_STORAGE_DEVICES][16] = { { 0 } };

// Cached hardware-presence per device, refreshed once at init() and then
// every pollStorageDevices() cycle - see isDevicePresent().
static bool isPresentCache[MAX_STORAGE_DEVICES] = { false };

static DISC_INTERFACE * FatDisc(int deviceId)
{
	switch(deviceId)
	{
		case DEVICE_SD:   return sd;
		case DEVICE_USB:  return WiiUsbMulti::getInterface(0);
		case DEVICE_USB2: return WiiUsbMulti::getInterface(1);
		case DEVICE_USB3: return WiiUsbMulti::getInterface(2);
		default: return nullptr;
	}
}

void WiiFileSystemDriver::init()
{
	DI_Init();
	USBStorage_Initialize();
	WiiUsbMulti::init();
	WiiUsbMulti::scan(); // populate initial slot state before the auto-mount pass below
	smbDriver.init();
	
	isPresentCache[DEVICE_SD]  = sd->isInserted(sd);
	isPresentCache[DEVICE_USB] = FatDisc(DEVICE_USB)->isInserted(FatDisc(DEVICE_USB));
	isPresentCache[DEVICE_USB2] = FatDisc(DEVICE_USB2)->isInserted(FatDisc(DEVICE_USB2));
	isPresentCache[DEVICE_USB3] = FatDisc(DEVICE_USB3)->isInserted(FatDisc(DEVICE_USB3));
	isPresentCache[DEVICE_DVD] = dvd->isInserted(dvd);

	StorageDevice devices[MAX_STORAGE_DEVICES];
	int count = enumerateStorageDevices(devices);

	for(int i = 0; i < count; i++)
		if(devices[i].autoMountAtStartup)
			mountStorageDevice(devices[i].id);
}

void WiiFileSystemDriver::shutdown()
{
	smbDriver.shutdown();
	fatUnmount("sd:");
	fatUnmount("usb:");
	fatUnmount("usb2:");
	fatUnmount("usb3:");
	WiiUsbMulti::shutdown();
	USBStorage_Deinitialize();
	DI_Close();
}

static void CopyLabel(StorageDevice & out, int deviceId)
{
	snprintf(out.label, sizeof(out.label), "%s", volumeLabel[deviceId]);
}

int WiiFileSystemDriver::enumerateStorageDevices(StorageDevice outDevices[MAX_STORAGE_DEVICES])
{
	int count = 0;
	outDevices[count] = StorageDevice{ DEVICE_SD,   "SD Card",   "sd:/",   true, true, 0, 0, 0, false, false, "", false }; CopyLabel(outDevices[count], DEVICE_SD);   count++;
	outDevices[count] = StorageDevice{ DEVICE_USB,  "USB Mass Storage",  "usb:/",  true, true, 0, 0, 0, false, false, "", false }; CopyLabel(outDevices[count], DEVICE_USB);  count++;
	outDevices[count] = StorageDevice{ DEVICE_USB2, "USB Mass Storage 2", "usb2:/", true, true, 0, 0, 0, false, false, "", false }; CopyLabel(outDevices[count], DEVICE_USB2); count++;
	outDevices[count] = StorageDevice{ DEVICE_USB3, "USB Mass Storage 3", "usb3:/", true, true, 0, 0, 0, false, false, "", false }; CopyLabel(outDevices[count], DEVICE_USB3); count++;
	outDevices[count] = StorageDevice{ DEVICE_DVD, "Data DVD",    "dvd:/", true, false, 0, 0, 0, false, false, "", true }; count++;
	outDevices[count] = StorageDevice{ DEVICE_SMB, "Network Share", "smb:/", false, false, 0, 0, 0, false, false, "", true }; count++;
	return count;
}

static const char * FatDeviceName(int deviceId, char name[10], char mountPoint[10])
{
	switch(deviceId)
	{
		case DEVICE_SD:   strcpy(name, "sd");   strcpy(mountPoint, "sd:");   return name;
		case DEVICE_USB:  strcpy(name, "usb");  strcpy(mountPoint, "usb:");  return name;
		case DEVICE_USB2: strcpy(name, "usb2"); strcpy(mountPoint, "usb2:"); return name;
		case DEVICE_USB3: strcpy(name, "usb3"); strcpy(mountPoint, "usb3:"); return name;
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

	// Distinguish "nothing there" from "something's there but we can't read
	// it" (eg. exFAT/NTFS - libfat only understands FAT12/16/32) so the UI
	// can tell the user to reformat rather than just "not found".
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
	if(deviceId == DEVICE_SMB)
		return smbDriver.isConnected() ? MountResult::Success : MountResult::DeviceNotFound;

	if(isMounted[deviceId])
		return MountResult::Success;

	switch(deviceId)
	{
		case DEVICE_SD:
		case DEVICE_USB:
		case DEVICE_USB2:
		case DEVICE_USB3:
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
	{
		switch(deviceId)
		{
			case DEVICE_SD:
			case DEVICE_USB:
			case DEVICE_USB2:
			case DEVICE_USB3: return "Unsupported format - FAT32 is recommended.";
			default:          return "Unrecognized DVD format.";
		}
	}

	switch(deviceId)
	{
		case DEVICE_SD:   return "SD card not found!";
		case DEVICE_USB:  return "USB drive not found!";
		case DEVICE_USB2: return "USB drive 2 not found!";
		case DEVICE_USB3: return "USB drive 3 not found!";
		case DEVICE_DVD:  return "No disc inserted!";
		case DEVICE_SMB:  return "Network share not connected!";
		default:          return "Device not found!";
	}
}

void WiiFileSystemDriver::invalidateStorageDevice(int deviceId)
{
	if(deviceId < 0 || deviceId >= MAX_STORAGE_DEVICES)
		return;

	isMounted[deviceId] = false;
	unmountRequired[deviceId] = true;
	volumeLabel[deviceId][0] = '\0';
}

void WiiFileSystemDriver::pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged)
{
	outRemovedCount = 0;

	// Rescan attached USB mass storage devices and capture topology changes
	deviceListChanged = WiiUsbMulti::scan();

	// Check SD card presence and update cache
	bool sdPresent = sd->isInserted(sd);
	if(sdPresent != isPresentCache[DEVICE_SD])
	{
		isPresentCache[DEVICE_SD] = sdPresent;
		deviceListChanged = true;
	}

	// Probe all 3 USB slots and update presence cache
	static const int usbSlot[WII_USB_MAX_DEVICES] = { DEVICE_USB, DEVICE_USB2, DEVICE_USB3 };
	for(int slot = 0; slot < WII_USB_MAX_DEVICES; slot++)
	{
		int deviceId = usbSlot[slot];
		DISC_INTERFACE * disc = WiiUsbMulti::getInterface(slot);
		bool usbPresent = disc->isInserted(disc);

		if(usbPresent != isPresentCache[deviceId])
		{
			isPresentCache[deviceId] = usbPresent;
			deviceListChanged = true;
		}
	}

	// Update DVD presence cache
	bool dvdPresent = dvd->isInserted(dvd);
	isPresentCache[DEVICE_DVD] = dvdPresent;

	// Invalidate and queue unmount events for disconnected mounted devices
	if(isMounted[DEVICE_SD] && !sdPresent)
	{
		invalidateStorageDevice(DEVICE_SD);
		removedIds[outRemovedCount++] = DEVICE_SD;
	}

	for(int slot = 0; slot < WII_USB_MAX_DEVICES; slot++)
	{
		int deviceId = usbSlot[slot];
		if(isMounted[deviceId] && !isPresentCache[deviceId])
		{
			invalidateStorageDevice(deviceId);
			removedIds[outRemovedCount++] = deviceId;
		}
	}

	if(isMounted[DEVICE_DVD] && !dvdPresent)
	{
		invalidateStorageDevice(DEVICE_DVD);
		removedIds[outRemovedCount++] = DEVICE_DVD;
	}
}

bool WiiFileSystemDriver::isDevicePresent(int deviceId) const
{
	switch(deviceId)
	{
		case DEVICE_SD:  return isPresentCache[DEVICE_SD];
		case DEVICE_USB: return isPresentCache[DEVICE_USB];
		case DEVICE_USB2: return isPresentCache[DEVICE_USB2];
		case DEVICE_USB3: return isPresentCache[DEVICE_USB3];
		case DEVICE_DVD: return isPresentCache[DEVICE_DVD]; // informational only - DVD is alwaysListed
		case DEVICE_SMB: return smbDriver.isConnected();    // informational only - SMB is alwaysListed
		default:         return false;
	}
}

//!Mount-path lookup, keyed by the shared Device enum. DEVICE_SMB isn't
//!here - its path depends on live connection state, so getMountPath()
//!below asks smbDriver directly rather than a fixed table entry.
static const char * const mountPath[DEVICE_LENGTH] =
{
	"",        // DEVICE_AUTO
	"sd:/",    // DEVICE_SD
	"usb:/",   // DEVICE_USB
	"usb2:/",  // DEVICE_USB2
	"usb3:/",  // DEVICE_USB3
	"dvd:/",   // DEVICE_DVD
	"",        // DEVICE_SMB (unused - see above)
	"", "", "", "",
};

const char * WiiFileSystemDriver::getMountPath(int device) const
{
	if(device == DEVICE_SMB)
		return smbDriver.getMountPath();

	if(device < 0 || device >= DEVICE_LENGTH || !isMounted[device])
		return "";
	return mountPath[device];
}

const int * WiiFileSystemDriver::getValidLoadDevices(int & outCount) const
{
	static const int devices[] = { DEVICE_AUTO, DEVICE_SD, DEVICE_USB, DEVICE_USB2, DEVICE_USB3, DEVICE_DVD, DEVICE_SMB };
	outCount = sizeof(devices) / sizeof(devices[0]);
	return devices;
}

const int * WiiFileSystemDriver::getValidSaveDevices(int & outCount) const
{
	static const int devices[] = { DEVICE_AUTO, DEVICE_SD, DEVICE_USB, DEVICE_USB2, DEVICE_USB3, DEVICE_SMB };
	outCount = sizeof(devices) / sizeof(devices[0]);
	return devices;
}

/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutFileSystemDriver.cpp
 *
 * Wii U storage device enumeration + mounting: SD and USB1/2/3 all go
 * through libmocha's raw disc interface + libdvm (see dvm_wut.c/h)
 * identically when Mocha is available; SD falls back to a plain
 * WHBMountSdCard() FSA mount, once, if it isn't - see mountSdFallback().
 ***************************************************************************/
#include <whb/sdcard.h>
#include <mocha/mocha.h>
#include <mocha/disc_interface.h>
#include <string.h>
#include <stdio.h>

#include "WutFileSystemDriver.h"
#include "dvm_wut.h"
#include "../Logger.h"

//! Normalizes WHBGetSdCardMountPath()'s runtime FS path (typically
//! "/vol/external01") into a devoptab-style prefix with a trailing slash.
//! Shared between init() and mountSdFallback() - the only two places the
//! non-Mocha SD path is ever (re-)established.
static void NormalizeSdFallbackPrefix(char prefix[32])
{
	const char * sdPath = WHBGetSdCardMountPath();

	if(sdPath && sdPath[0])
	{
		strncpy(prefix, sdPath, 32 - 2); // leave room for '/' + NUL
		prefix[32 - 2] = '\0';

		size_t len = strlen(prefix);
		if(len == 0 || prefix[len - 1] != '/')
		{
			prefix[len] = '/';
			prefix[len + 1] = '\0';
		}
	}
	else
	{
		strncpy(prefix, "sdmc:/", 32 - 1);
		prefix[32 - 1] = '\0';
	}
}

void WutFileSystemDriver::init()
{
	memset(devices, 0, sizeof(devices));
	deviceCount = 0;

	// USB (and, if this succeeds, SD too) go through Mocha's raw
	// DISC_INTERFACE + libdvm. If this fails - not booted under
	// Aroma/compatible CFW, or Mocha not installed (it's optional there) -
	// USB is unavailable entirely and SD falls back to mountSdFallback().
	mochaReady = (Mocha_InitLibrary() == MOCHA_RESULT_SUCCESS);
	sdUsesMochaPath = mochaReady;

	// Independent of Mocha - dvmWutInit() just registers libdvm's vfat/exfat/ntfs filesystem drivers, which don't touch hardware themselves.
	dvmWutInit();

	storageSlots[slotSD]   = { &Mocha_sdio_disc_interface, "sd",   0, 0 };
	storageSlots[slotUSB1] = { &Mocha_usb1_disc_interface, "usb1", 0, 0 };
	storageSlots[slotUSB2] = { &Mocha_usb2_disc_interface, "usb2", 0, 0 };
	storageSlots[slotUSB3] = { &Mocha_usb3_disc_interface, "usb3", 0, 0 };

	WutDeviceState & sd = devices[slotSD];
	memset(&sd, 0, sizeof(sd));
	sd.id = DEVICE_SD;
	strcpy(sd.name, "SD Card");

	if(sdUsesMochaPath)
	{
		snprintf(sd.stablePrefix, sizeof(sd.stablePrefix), "%s:/", storageSlots[slotSD].mountName);
	}
	else
	{
		bool mounted = WHBMountSdCard();
		if(mounted)
		{
			NormalizeSdFallbackPrefix(sd.stablePrefix);
			strcpy(sd.prefix, sd.stablePrefix);
			sd.isPresent = true;
			sd.isMounted = true;
		}
	}

	// USB 1/2/3 setup
	WutDeviceState & usb1 = devices[slotUSB1];
	memset(&usb1, 0, sizeof(usb1));
	usb1.id = DEVICE_USB;
	strcpy(usb1.name, "USB Storage 1");
	snprintf(usb1.stablePrefix, sizeof(usb1.stablePrefix), "%s:/", storageSlots[slotUSB1].mountName);

	WutDeviceState & usb2 = devices[slotUSB2];
	memset(&usb2, 0, sizeof(usb2));
	usb2.id = DEVICE_USB2;
	strcpy(usb2.name, "USB Storage 2");
	snprintf(usb2.stablePrefix, sizeof(usb2.stablePrefix), "%s:/", storageSlots[slotUSB2].mountName);

	WutDeviceState & usb3 = devices[slotUSB3];
	memset(&usb3, 0, sizeof(usb3));
	usb3.id = DEVICE_USB3;
	strcpy(usb3.name, "USB Storage 3");
	snprintf(usb3.stablePrefix, sizeof(usb3.stablePrefix), "%s:/", storageSlots[slotUSB3].mountName);

	smbDriver.init();

	WutDeviceState & smb = devices[slotSMB];
	memset(&smb, 0, sizeof(smb));
	smb.id = DEVICE_SMB;
	strcpy(smb.name, "Network Share");
	strcpy(smb.stablePrefix, "smb:/");

	deviceCount = slotCount;
}

void WutFileSystemDriver::shutdown()
{
	smbDriver.shutdown();

	if(sdUsesMochaPath)
		unmountStorageSlot(slotSD);
	else
		WHBUnmountSdCard();

	// unmountStorageSlot() only touches whichever slots were actually
	// mounted. Mocha_*_shutdown() is a safe no-op on an interface that
	// was never started - including SD's here when it never used the
	// Mocha path at all (!sdUsesMochaPath) - so no special-casing needed.
	for(int i = slotUSB1; i <= slotUSB3; i++)
		unmountStorageSlot(i);

	for(int i = 0; i < storageSlotCount; i++)
		if(storageSlots[i].iface)
			storageSlots[i].iface->shutdown();

	if(mochaReady)
	{
		Mocha_DeInitLibrary();
		mochaReady = false;
	}

	memset(devices, 0, sizeof(devices));
	deviceCount = 0;
}

int WutFileSystemDriver::findDeviceIndex(int deviceId) const
{
	for(int i = 0; i < deviceCount; i++)
		if(devices[i].id == deviceId)
			return i;
	return -1;
}

void WutFileSystemDriver::getVolumeLabel(WutDeviceState & dev, const char * dvmName)
{
	// Only ever looked up once per mount
	if(dev.labelFetched)
		return;

	dev.volumeLabel[0] = '\0';

	if(dvmName) // null on the non-Mocha SD fallback - no label source there
	{
		char path[16];
		snprintf(path, sizeof(path), "%s:", dvmName);
		dvmWutGetVolumeLabel(path, dev.volumeLabel, sizeof(dev.volumeLabel));
	}

	dev.labelFetched = true;
}

bool WutFileSystemDriver::tryMountStorageSlot(int slotIdx)
{
	if(slotIdx < 0 || slotIdx >= storageSlotCount)
		return false;

	WutDeviceState & dev = devices[slotIdx];
	WutStorageSlot & slot = storageSlots[slotIdx];

	if(dev.isMounted)
		return true;

	if(dev.unmountRequired)
		unmountStorageSlot(slotIdx);

	if(!mochaReady)
		return false;

	// Backing off after repeated failures against an unremoved device
	if(slot.backoffPollsLeft > 0)
	{
		slot.backoffPollsLeft--;
		return false;
	}

	// dvmWutMountVolume() takes a non-const DISC_INTERFACE* but never mutates it
	if(dvmWutMountVolume(slot.mountName, (DISC_INTERFACE *) slot.iface, cachePages, sectorsPerPage))
	{
		slot.failCount = 0;
		dev.isPresent = true;
		dev.isMounted = true;
		dev.unmountRequired = false;
		snprintf(dev.prefix, sizeof(dev.prefix), "%s:/", slot.mountName);
		getVolumeLabel(dev, slot.mountName);
		return true;
	}

	// Mount failed. Always leave the interface shutdown() here rather than leaving the fd open across attempts.
	slot.iface->shutdown();

	slot.failCount++;
	if(slot.failCount < maxQuickRetries)
		slot.backoffPollsLeft = 0; // could be transient (eg. drive still spinning up) - try again next cycle
	else
		slot.backoffPollsLeft = backoffPolls; // give up on this device for a while - never permanently

	dev.isPresent = false;
	dev.isMounted = false;
	return false;
}

void WutFileSystemDriver::unmountStorageSlot(int slotIdx)
{
	if(slotIdx < 0 || slotIdx >= storageSlotCount)
		return;

	WutDeviceState & dev = devices[slotIdx];
	WutStorageSlot & slot = storageSlots[slotIdx];

	if(dev.isMounted)
		dvmWutUnmountVolume(slot.mountName);

	slot.failCount = 0;
	slot.backoffPollsLeft = 0;
	dev.isPresent = false;
	dev.isMounted = false;
	dev.unmountRequired = false;
	dev.labelFetched = false;
	dev.prefix[0] = '\0';
	dev.volumeLabel[0] = '\0';
}

bool WutFileSystemDriver::slotStillPresent(int slotIdx)
{
	if(slotIdx < 0 || slotIdx >= storageSlotCount || !mochaReady)
		return false;

	WutDeviceState & dev = devices[slotIdx];
	if(!dev.isMounted)
		return false;

	// Forces a genuine, uncached raw sector read through the mounted disc
	return dvmWutVolumeStillPresent(storageSlots[slotIdx].mountName);
}

void WutFileSystemDriver::refreshSmbSlot()
{
	WutDeviceState & smb = devices[slotSMB];
	bool connected = smbDriver.isConnected();
	smb.isPresent = true;
	smb.isMounted = connected;

	if(connected)
	{
		strncpy(smb.prefix, smbDriver.getMountPath(), sizeof(smb.prefix) - 1);
		smb.prefix[sizeof(smb.prefix) - 1] = '\0';
	}
	else
	{
		smb.prefix[0] = '\0';
		smb.volumeLabel[0] = '\0';
	}
}

int WutFileSystemDriver::enumerateStorageDevices(StorageDevice outDevices[MAX_STORAGE_DEVICES])
{
	refreshSmbSlot();

	int count = 0;
	for(int i = 0; i < deviceCount && count < MAX_STORAGE_DEVICES; i++)
	{
		if(!devices[i].isPresent)
			continue;

		StorageDevice & out = outDevices[count];
		out.id = devices[i].id;
		strncpy(out.name, devices[i].name, sizeof(out.name) - 1);
		out.name[sizeof(out.name) - 1] = '\0';
		strncpy(out.volumeLabel, devices[i].volumeLabel, sizeof(out.volumeLabel) - 1);
		out.volumeLabel[sizeof(out.volumeLabel) - 1] = '\0';
		strncpy(out.prefix, devices[i].prefix, sizeof(out.prefix) - 1);
		out.prefix[sizeof(out.prefix) - 1] = '\0';
		out.removable = (devices[i].id != DEVICE_SMB);
		out.autoMountAtStartup = (devices[i].id != DEVICE_SMB); // SMB needs explicit getSmb()->connect() first
		out.alwaysListed = (devices[i].id == DEVICE_SMB);
		count++;
	}
	return count;
}

bool WutFileSystemDriver::isDevicePresent(int deviceId) const
{
	if(deviceId == DEVICE_SMB)
		return smbDriver.isConnected(); // informational only - SMB is alwaysListed

	int idx = findDeviceIndex(deviceId);
	return idx >= 0 && devices[idx].isPresent;
}

MountResult WutFileSystemDriver::mountStorageDevice(int deviceId)
{
	int idx = findDeviceIndex(deviceId);
	if(idx < 0)
		return MountResult::DeviceNotFound; // not ours

	if(deviceId == DEVICE_SMB)
	{
		refreshSmbSlot();
		return smbDriver.isConnected() ? MountResult::Success : MountResult::DeviceNotFound;
	}

	if(deviceId == DEVICE_SD && !sdUsesMochaPath)
		return mountSdFallback();

	// SD (Mocha path) and USB1/2/3 - idx lines up with storageSlots[] 1:1
	return tryMountStorageSlot(idx) ? MountResult::Success : MountResult::DeviceNotFound;
}

MountResult WutFileSystemDriver::mountSdFallback()
{
	WutDeviceState & sd = devices[slotSD];

	if(sd.isMounted)
		return MountResult::Success;

	// No cheap presence probe without Mocha
	if(!WHBMountSdCard())
	{
		sd.isPresent = false;
		return MountResult::DeviceNotFound;
	}

	NormalizeSdFallbackPrefix(sd.prefix);
	sd.isPresent = true;
	sd.isMounted = true;
	return MountResult::Success;
}

const char * WutFileSystemDriver::mountResultMessage(int deviceId, MountResult result)
{
	if(result == MountResult::MountFailed)
		return "Unable to mount device.";

	if(deviceId == DEVICE_SD)
		return "SD card not found!";
	else if(deviceId == DEVICE_USB || deviceId == DEVICE_USB2 || deviceId == DEVICE_USB3)
		return "USB drive not found!";
	else if(deviceId == DEVICE_SMB)
		return "Network share not connected!";

	return "Storage device not found!";
}

void WutFileSystemDriver::invalidateStorageDevice(int deviceId)
{
	if(deviceId == DEVICE_SMB)
	{
		smbDriver.disconnect();
		refreshSmbSlot();
		return;
	}

	int idx = findDeviceIndex(deviceId);
	if(idx < 0)
		return;

	devices[idx].isMounted = false;
	devices[idx].unmountRequired = true;
	devices[idx].labelFetched = false;
}

void WutFileSystemDriver::pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged)
{
	outRemovedCount = 0;
	deviceListChanged = false;

	// SD without Mocha has no ongoing hot-plug detection at all (see
	// mountSdFallback()) - nothing to poll here; its last known state
	// stands until an explicit mount attempt or an I/O failure
	// invalidates it.

	// Cheap, read-only nsysuhs scan across USB only - SD is a fixed slot
	// with no attach-order topology to rescan. Detects a real hardware-
	// level attach/detach independently of whether Mocha's mount attempt
	// has succeeded. On a change, a fresh insertion should be tried this
	// same cycle.
	UsbHardwareSignature currentHwSig;
	ScanUsbHardwareSignature(currentHwSig);

	if(UsbHardwareSignatureChanged(currentHwSig, usbHwSignature))
	{
		// Only backoffPollsLeft, not failCount: a real hardware change
		// anywhere is worth one fresh look at every USB slot right now
		// (that's what backoffPollsLeft=0 buys), but a slot sitting on a
		// permanently-unmountable device (eg. an unsupported filesystem)
		// still needs to reach maxQuickRetries and back off properly.
		for(int i = slotUSB1; i <= slotUSB3; i++)
			storageSlots[i].backoffPollsLeft = 0;
	}

	usbHwSignature = currentHwSig;

	int firstSlot = sdUsesMochaPath ? slotSD : slotUSB1;

	for(int slotIdx = firstSlot; slotIdx <= slotUSB3; slotIdx++)
	{
		WutDeviceState & dev = devices[slotIdx];

		if(dev.isMounted)
		{
			if(!slotStillPresent(slotIdx))
			{
				unmountStorageSlot(slotIdx);

				if(outRemovedCount < MAX_STORAGE_DEVICES)
					removedIds[outRemovedCount++] = dev.id;
				deviceListChanged = true;
			}
		}
		else if(tryMountStorageSlot(slotIdx))
		{
			deviceListChanged = true;
		}
	}
}

const char * WutFileSystemDriver::getDevicePrefix(int device) const
{
	int idx = findDeviceIndex(device);
	return idx < 0 ? "" : devices[idx].stablePrefix;
}

const char * WutFileSystemDriver::getMountPath(int device) const
{
	if(device == DEVICE_SMB)
		const_cast<WutFileSystemDriver *>(this)->refreshSmbSlot();

	int idx = findDeviceIndex(device);
	if(idx < 0 || !devices[idx].isMounted || devices[idx].prefix[0] == '\0')
		return "";
	return devices[idx].prefix;
}

const int * WutFileSystemDriver::getValidLoadDevices(int & outCount) const
{
	static const int devices[] = { DEVICE_AUTO, DEVICE_SD, DEVICE_USB, DEVICE_USB2, DEVICE_USB3, DEVICE_SMB };
	outCount = sizeof(devices) / sizeof(devices[0]);
	return devices;
}

const int * WutFileSystemDriver::getValidSaveDevices(int & outCount) const
{
	static const int devices[] = { DEVICE_AUTO, DEVICE_SD, DEVICE_USB, DEVICE_USB2, DEVICE_USB3, DEVICE_SMB };
	outCount = sizeof(devices) / sizeof(devices[0]);
	return devices;
}

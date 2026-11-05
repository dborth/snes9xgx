/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutFileSystemDriver.cpp
 *
 * Wii U storage device enumeration + mounting: SD via a plain
 * WHBMountSdCard() FSA mount, done exactly once in init() and treated as
 * permanently present from then on
 * USB1/2/3 via libmocha's raw disc interface + libdvm (see dvm_wut.c/h).
 ***************************************************************************/
#include <whb/sdcard.h>
#include <mocha/mocha.h>
#include <mocha/disc_interface.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "WutFileSystemDriver.h"
#include "WutFilename.h"
#include "dvm_wut.h"
#include "../Logger.h"

//! Fallback SD prefix if WHBGetSdCardMountPath() has nothing to report
static const char * const SD_FALLBACK_PREFIX = "/vol/external01/";

//! One-time SD mount, tried a few times because it can fail transiently
static const int SD_MOUNT_ATTEMPTS = 3;
static const useconds_t SD_MOUNT_RETRY_DELAY_US = 100000;

//! Normalizes WHBGetSdCardMountPath()'s runtime FS path (typically
//! "/vol/external01") into a devoptab-style prefix with a trailing slash.
static void NormalizeSdPrefix(char prefix[32])
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
		strncpy(prefix, SD_FALLBACK_PREFIX, 32 - 1);
		prefix[32 - 1] = '\0';
	}
}

void WutFileSystemDriver::init()
{
	memset(devices, 0, sizeof(devices));
	deviceCount = 0;

	// USB goes through Mocha's raw DISC_INTERFACE + libdvm. If this fails -
	// not booted under Aroma/compatible CFW, or Mocha not installed (it's
	// optional there) - USB is unavailable entirely.
	mochaReady = (Mocha_InitLibrary() == MOCHA_RESULT_SUCCESS);

	// Independent of Mocha - dvmWutInit() just registers libdvm's vfat/exfat/ntfs filesystem drivers, which don't touch hardware themselves.
	dvmWutInit();

	// storageSlots[] is USB only: [0..2] = USB1..3, i.e. devices[slotUSB1 + i]
	storageSlots[0] = { &Mocha_usb1_disc_interface, "usb1", 0, 0 };
	storageSlots[1] = { &Mocha_usb2_disc_interface, "usb2", 0, 0 };
	storageSlots[2] = { &Mocha_usb3_disc_interface, "usb3", 0, 0 };

	// SD: plain FSA mount, attempted once
	WutDeviceState & sd = devices[slotSD];
	memset(&sd, 0, sizeof(sd));
	sd.id = DEVICE_SD;
	strcpy(sd.name, "SD Card");

	bool sdMounted = false;
	for(int attempt = 0; attempt < SD_MOUNT_ATTEMPTS && !sdMounted; attempt++)
	{
		if(attempt > 0)
			usleep(SD_MOUNT_RETRY_DELAY_US);
		sdMounted = WHBMountSdCard();
	}

	NormalizeSdPrefix(sd.stablePrefix);
	strcpy(sd.prefix, sd.stablePrefix);
	sd.isPresent = true;
	sd.isMounted = true;

	// USB 1/2/3 setup
	WutDeviceState & usb1 = devices[slotUSB1];
	memset(&usb1, 0, sizeof(usb1));
	usb1.id = DEVICE_USB;
	strcpy(usb1.name, "USB Storage 1");
	snprintf(usb1.stablePrefix, sizeof(usb1.stablePrefix), "%s:/", storageSlots[0].mountName);

	WutDeviceState & usb2 = devices[slotUSB2];
	memset(&usb2, 0, sizeof(usb2));
	usb2.id = DEVICE_USB2;
	strcpy(usb2.name, "USB Storage 2");
	snprintf(usb2.stablePrefix, sizeof(usb2.stablePrefix), "%s:/", storageSlots[1].mountName);

	WutDeviceState & usb3 = devices[slotUSB3];
	memset(&usb3, 0, sizeof(usb3));
	usb3.id = DEVICE_USB3;
	strcpy(usb3.name, "USB Storage 3");
	snprintf(usb3.stablePrefix, sizeof(usb3.stablePrefix), "%s:/", storageSlots[2].mountName);

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

	WHBUnmountSdCard();

	// unmountStorageSlot() only touches whichever slots were actually
	// mounted. Mocha_usbN_shutdown() is a safe no-op on an interface that
	// was never started, so no special-casing needed.
	for(int i = 0; i < storageSlotCount; i++)
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

void WutFileSystemDriver::readVolumeLabel(const char * dvmName, char (&out)[16])
{
	out[0] = '\0';

	if(dvmName) // null when there's no dvm volume behind the device - no label source then
	{
		char path[16];
		snprintf(path, sizeof(path), "%s:", dvmName);
		dvmWutGetVolumeLabel(path, out, sizeof(out));
	}
}

bool WutFileSystemDriver::tryMountStorageSlot(int slotIdx)
{
	if(slotIdx < 0 || slotIdx >= storageSlotCount)
		return false;

	WutDeviceState & dev = devices[slotUSB1 + slotIdx];
	WutStorageSlot & slot = storageSlots[slotIdx];

	bool needUnmount;

	{
		MutexLock guard(stateLock);

		if(dev.isMounted)
			return true;

		needUnmount = dev.unmountRequired;
	}

	// Takes the lock itself, and calls into libdvm without it
	if(needUnmount)
		unmountStorageSlot(slotIdx);

	if(!mochaReady)
		return false;

	{
		MutexLock guard(stateLock);

		// A readable device with nothing mountable on it: probing it again can't change that
		if(slot.unmountable)
			return false;

		// Backing off after repeated failures against an unremoved device
		if(slot.backoffPollsLeft > 0)
		{
			slot.backoffPollsLeft--;
			return false;
		}
	}

	// The mount and the label lookup below can take seconds (spin-up, probing
	// every partition), so neither runs under stateLock. dvmWutMountVolumeEx()
	// takes a non-const DISC_INTERFACE* but never mutates it.
	const DvmWutMountResult result = dvmWutMountVolumeEx(slot.mountName, (DISC_INTERFACE *) slot.iface, cachePages, sectorsPerPage);

	if(result == DVM_WUT_MOUNTED)
	{
		char label[16];
		readVolumeLabel(slot.mountName, label);

		MutexLock guard(stateLock);
		slot.failCount = 0;
		dev.isPresent = true;
		dev.unmountRequired = false;
		snprintf(dev.prefix, sizeof(dev.prefix), "%s:/", slot.mountName);
		memcpy(dev.volumeLabel, label, sizeof(dev.volumeLabel));
		dev.labelFetched = true;
		dev.isMounted = true;	// last: everything above is in place when a reader sees it
		return true;
	}

	// Mount failed. Always leave the interface shutdown() here rather than leaving the fd open across attempts.
	slot.iface->shutdown();

	if(result == DVM_WUT_UNMOUNTABLE)
	{
		// The disc was read fine and nothing on it is recognised (no MBR/VBR - eg. a WFS drive - or no
		// supported filesystem). Retrying can't help, so stop: cleared only by a hardware change.
		{
			MutexLock guard(stateLock);

			slot.unmountable = true;
			slot.failCount = 0;
			slot.backoffPollsLeft = 0;
			dev.isPresent = false;
			dev.isMounted = false;
		}

		// Not under the lock: the file logger does storage I/O
		LOG_INFO("%s: device is readable but has nothing mountable (no MBR/VBR, or no FAT/exFAT/NTFS partition) - not retrying until the USB hardware changes", slot.mountName);
		return false;
	}

	MutexLock guard(stateLock);

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

	WutDeviceState & dev = devices[slotUSB1 + slotIdx];
	WutStorageSlot & slot = storageSlots[slotIdx];

	bool wasMounted;

	{
		MutexLock guard(stateLock);
		wasMounted = dev.isMounted;
	}

	// Flushes the cache and shuts the device down: not under the lock
	if(wasMounted)
		dvmWutUnmountVolume(slot.mountName);

	MutexLock guard(stateLock);

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

	{
		MutexLock guard(stateLock);

		if(!devices[slotUSB1 + slotIdx].isMounted)
			return false;
	}

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

	MutexLock guard(stateLock);

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
		const char * browsePrefix = (devices[i].id == DEVICE_SMB) ? devices[i].stablePrefix : devices[i].prefix;
		strncpy(out.prefix, browsePrefix, sizeof(out.prefix) - 1);
		out.prefix[sizeof(out.prefix) - 1] = '\0';
		// SD (mounted in init(), never polled) and SMB (needs explicit getSmb()->connect() first) are neither removable nor auto-mounted
		bool fixedDevice = (devices[i].id == DEVICE_SD || devices[i].id == DEVICE_SMB);
		out.removable = !fixedDevice;
		out.autoMountAtStartup = !fixedDevice;
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

	MutexLock guard(stateLock);
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

	// SD is always present and always mounted
	if(deviceId == DEVICE_SD)
		return MountResult::Success;

	// USB1/2/3 - backed by storageSlots[]
	const int slotIdx = idx - slotUSB1;
	if(tryMountStorageSlot(slotIdx))
		return MountResult::Success;

	// Present, but there's nothing on it we can mount (never re-probed - see tryMountStorageSlot())
	bool unmountable;
	{
		MutexLock guard(stateLock);
		unmountable = storageSlots[slotIdx].unmountable;
	}
	return unmountable ? MountResult::MountFailed : MountResult::DeviceNotFound;
}

const char * WutFileSystemDriver::mountResultMessage(int deviceId, MountResult result)
{
	if(result == MountResult::MountFailed)
	{
		if(deviceId == DEVICE_USB || deviceId == DEVICE_USB2 || deviceId == DEVICE_USB3)
			return "Unsupported drive format. Use FAT32, exFAT or NTFS.";
		return "Unable to mount device.";
	}

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

	if(deviceId == DEVICE_SD)
		return; // SD never goes away

	int idx = findDeviceIndex(deviceId);
	if(idx < 0)
		return;

	MutexLock guard(stateLock);

	devices[idx].isMounted = false;
	devices[idx].unmountRequired = true;
	devices[idx].labelFetched = false;
}

void WutFileSystemDriver::pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged)
{
	outRemovedCount = 0;
	deviceListChanged = false;

	// SD is assumed always present and is not polled at all.

	// Cheap, read-only nsysuhs scan. Detects a real hardware-level
	// attach/detach independently of whether Mocha's mount attempt has
	// succeeded. On a change, a fresh insertion should be tried this same
	// cycle.
	UsbHardwareSignature currentHwSig;
	const bool scanned = ScanUsbHardwareSignature(currentHwSig);
	if(scanned)
		usbScanWorked = true;

	// Do we know what is attached? Yes after any successful scan - a later failure just means no news.
	const bool hwKnown = scanned || usbScanWorked;

	if(!scanned)
	{
		// The scan itself failed. That says nothing about the hardware (an empty result here would read as
		// "everything was unplugged"), so carry on with what we knew.
		currentHwSig = usbHwSignature;
	}
	else if(UsbHardwareSignatureChanged(currentHwSig, usbHwSignature))
	{
		// A real hardware change anywhere is worth one fresh look at every USB slot right now: lift any
		// backoff, and forget "unmountable" - the device that earned it may be gone or replaced. (failCount
		// is kept, so a slot that keeps failing still reaches maxQuickRetries and backs off properly.)
		MutexLock guard(stateLock);

		for(int i = 0; i < storageSlotCount; i++)
		{
			storageSlots[i].backoffPollsLeft = 0;
			storageSlots[i].unmountable = false;
		}
	}

	usbHwSignature = currentHwSig;

	// First: is everything we have mounted still there?
	for(int slotIdx = 0; slotIdx < storageSlotCount; slotIdx++)
	{
		WutDeviceState & dev = devices[slotUSB1 + slotIdx];
		bool mounted;

		{
			MutexLock guard(stateLock);
			mounted = dev.isMounted;
		}

		if(mounted && !slotStillPresent(slotIdx))
		{
			unmountStorageSlot(slotIdx);

			if(outRemovedCount < MAX_STORAGE_DEVICES)
				removedIds[outRemovedCount++] = dev.id;
			deviceListChanged = true;
		}
	}

	// Then: look for devices we don't have an answer for yet. Every attached storage device is either
	// mounted, or found unmountable. Only while there are more attached than that is there anything to
	// look for - so with nothing attached (or everything accounted for) no mount attempt is made at all.
	int accountedFor = 0;
	{
		MutexLock guard(stateLock);

		for(int slotIdx = 0; slotIdx < storageSlotCount; slotIdx++)
			if(devices[slotUSB1 + slotIdx].isMounted || storageSlots[slotIdx].unmountable)
				accountedFor++;
	}

	for(int slotIdx = 0; slotIdx < storageSlotCount; slotIdx++)
	{
		bool accounted;

		{
			MutexLock guard(stateLock);
			accounted = devices[slotUSB1 + slotIdx].isMounted || storageSlots[slotIdx].unmountable;
		}

		if(accounted)
			continue;

		if(hwKnown && usbHwSignature.count <= accountedFor)
			break; // everything attached is accounted for

		if(tryMountStorageSlot(slotIdx))
		{
			deviceListChanged = true;
			accountedFor++;
			continue;
		}

		bool nowUnmountable;
		{
			MutexLock guard(stateLock);
			nowUnmountable = storageSlots[slotIdx].unmountable;
		}
		if(nowUnmountable)
			accountedFor++; // a device, just not one we can use
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
	if(idx < 0)
		return "";

	{
		MutexLock guard(stateLock);

		if(!devices[idx].isMounted || devices[idx].prefix[0] == '\0')
			return "";
	}

	// The caller keeps this pointer after the lock is gone. prefix[] is rewritten
	// by every mount and unmount; stablePrefix[] is the same text, written once in init().
	// SMB's prefix is only ever touched on the main thread (refreshSmbSlot()).
	return device == DEVICE_SMB ? devices[idx].prefix : devices[idx].stablePrefix;
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

void WutFileSystemDriver::nameToDisplay(const char * rawName, char * out, size_t outSize) const
{
	// FSA hands back non-ASCII FAT names as CP932 (Shift-JIS), not UTF-8
	WutNameToUtf8(rawName, out, outSize);
}

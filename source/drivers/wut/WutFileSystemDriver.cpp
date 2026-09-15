/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutFileSystemDriver.cpp
 *
 * Wii U storage device enumeration + mounting: SD via WHB, USB via
 * libmocha's raw disc interface + libdvm (see dvm_wut.c/h).
 ***************************************************************************/
#include <whb/sdcard.h>
#include <mocha/mocha.h>
#include <mocha/disc_interface.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

#include "WutFileSystemDriver.h"
#include "dvm_wut.h"
#include "../Logger.h"

static bool DevicePresent(const char * prefix)
{
	struct stat st;
	return stat(prefix, &st) == 0;
}

void WutFileSystemDriver::init()
{
	memset(devices, 0, sizeof(devices));
	deviceCount = 0;

	FSAInit();
	fsaClient = FSAAddClient(nullptr); // best-effort; volume-label lookups just fall back if this is negative (FSError on failure, not necessarily 0)

	// USB (via Mocha_usbX_disc_interface) needs Mocha; SD (via WHB)
	// doesn't. If this fails - not booted under Aroma/compatible CFW - USB
	// just stays permanently absent rather than the whole driver failing.
	mochaReady = (Mocha_InitLibrary() == MOCHA_RESULT_SUCCESS);

	// Independent of Mocha - dvmWutInit() just registers libdvm's vfat/exfat/ntfs filesystem drivers, which don't touch hardware themselves.
	dvmWutInit();

	WHBMountSdCard();

	WutDeviceState & sd = devices[slotSD];
	memset(&sd, 0, sizeof(sd));
	sd.id = DEVICE_SD;
	strcpy(sd.name, "SD Card");

	// The SD mount path is only known at runtime - WHBMountSdCard() picks
	// the real FS path (typically "/vol/external01", with NO trailing
	// slash) and only exposes it through WHBGetSdCardMountPath(). 
	// It's normalized here by adding a slash
	const char * sdPath = WHBGetSdCardMountPath();
	if(sdPath && sdPath[0])
	{
		strncpy(sd.prefix, sdPath, sizeof(sd.prefix) - 2); // leave room for '/' + NUL
		sd.prefix[sizeof(sd.prefix) - 2] = '\0';

		size_t len = strlen(sd.prefix);
		if(len == 0 || sd.prefix[len - 1] != '/')
		{
			sd.prefix[len] = '/';
			sd.prefix[len + 1] = '\0';
		}
	}
	else
	{
		strncpy(sd.prefix, "sdmc:/", sizeof(sd.prefix) - 1);
		sd.prefix[sizeof(sd.prefix) - 1] = '\0';
	}

	sd.isPresent = DevicePresent(sd.prefix);
	sd.isMounted = sd.isPresent;
	sd.unmountRequired = false;
	refreshDisplayName(sd);

	// USB 1 Setup
	WutDeviceState & usb1 = devices[slotUSB1];
	memset(&usb1, 0, sizeof(usb1));
	usb1.id = DEVICE_USB;
	strcpy(usb1.name, "USB Storage 1");
	usb1.prefix[0] = '\0';
	usb1.isPresent = false;
	usb1.isMounted = false;
	usb1.unmountRequired = false;

	// USB 2 Setup
	WutDeviceState & usb2 = devices[slotUSB2];
	memset(&usb2, 0, sizeof(usb2));
	usb2.id = DEVICE_USB2;
	strcpy(usb2.name, "USB Storage 2");
	usb2.prefix[0] = '\0';
	usb2.isPresent = false;
	usb2.isMounted = false;
	usb2.unmountRequired = false;

	// USB 3 Setup
	WutDeviceState & usb3 = devices[slotUSB3];
	memset(&usb3, 0, sizeof(usb3));
	usb3.id = DEVICE_USB3;
	strcpy(usb3.name, "USB Storage 3");
	usb3.prefix[0] = '\0';
	usb3.isPresent = false;
	usb3.isMounted = false;
	usb3.unmountRequired = false;

	usbSlots[0] = { &Mocha_usb1_disc_interface, "usb1", 0, 0 };
	usbSlots[1] = { &Mocha_usb2_disc_interface, "usb2", 0, 0 };
	usbSlots[2] = { &Mocha_usb3_disc_interface, "usb3", 0, 0 };

	// Deliberately not attempting the USB mount here: the raw open probe
	// is a real IOSU IPC round trip, and init() runs on the main thread
	// during app startup - not the storage-checking thread. The first
	// pollStorageDevices() call (or an explicit mountStorageDevice(),
	// eg. from "autoMountAtStartup") picks it up from there.

	smbDriver.init();

	WutDeviceState & smb = devices[slotSMB];
	memset(&smb, 0, sizeof(smb));
	smb.id = DEVICE_SMB;
	strcpy(smb.name, "Network Share");
	
	deviceCount = slotCount;
}

void WutFileSystemDriver::shutdown()
{
	WHBUnmountSdCard();
	smbDriver.shutdown();

	// unmountUsbSlot() only touches whichever slots were actually mounted -
	// a slot that's mid-backoff and never mounted could still theoretically
	// hold an open fd (eg. app killed mid-probe). Cheap and safe to call
	// unconditionally; Mocha_usbN_shutdown() no-ops if the fd isn't open.
	for(int i = 0; i < usbSlotCount; i++)
	{
		unmountUsbSlot(i);
		if(usbSlots[i].iface)
			usbSlots[i].iface->shutdown();
	}

	if(fsaClient >= 0)
	{
		FSADelClient(fsaClient);
	}
	fsaClient = -1;

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

void WutFileSystemDriver::refreshDisplayName(WutDeviceState & dev)
{
	// Volume label, kept separate from `name`
	// Best-effort: FSAGetVolumeInfo only succeeds if dev.prefix genuinely
	// resolves through our own FSA client.
	dev.label[0] = '\0';

	if(fsaClient >= 0)
	{
		FSAVolumeInfo volInfo;
		memset(&volInfo, 0, sizeof(volInfo));
		if(FSAGetVolumeInfo(fsaClient, dev.prefix, &volInfo) == FS_ERROR_OK && volInfo.volumeLabel[0] != '\0')
		{
			strncpy(dev.label, volInfo.volumeLabel, sizeof(dev.label) - 1);
			dev.label[sizeof(dev.label) - 1] = '\0';
		}
	}
}

bool WutFileSystemDriver::tryMountUsbSlot(int usbSlotIdx)
{
	if(usbSlotIdx < 0 || usbSlotIdx >= usbSlotCount)
		return false;

	WutDeviceState & usb = devices[slotUSB1 + usbSlotIdx];
	WutUsbPhysicalSlot & slot = usbSlots[usbSlotIdx];

	if(usb.isMounted)
		return true;

	if(usb.unmountRequired)
		unmountUsbSlot(usbSlotIdx);

	if(!mochaReady)
		return false;

	// Backing off after repeated failures against an unremoved device -
	// skip the IOSU round trip entirely until the countdown elapses,
	// rather than re-probing every single poll cycle forever. This is a
	// plain poll counter rather than anything keyed off isInserted(),
	// since Mocha_usbN_isInserted() only reflects "do we currently have an
	// fd open" - it never re-probes hardware, so it can't distinguish
	// "still the same bad device" from "something changed" on its own.
	if(slot.backoffPollsLeft > 0)
	{
		slot.backoffPollsLeft--;
		return false;
	}

	// dvmWutMountUsb() takes a non-const DISC_INTERFACE* (matching
	// dvmDiscCreate()'s own signature upstream) but never mutates it -
	// only ever calls through its function pointers.
	if(dvmWutMountUsb(slot.mountName, (DISC_INTERFACE *) slot.iface, usbCachePages, usbSectorsPerPage))
	{
		slot.failCount = 0;
		usb.isPresent = true;
		usb.isMounted = true;
		usb.unmountRequired = false;
		snprintf(usb.prefix, sizeof(usb.prefix), "%s:/", slot.mountName);
		refreshDisplayName(usb);
		return true;
	}

	// Mount failed (nothing there, unformatted, or a format we don't
	// recognize). Always leave the interface shutdown() here rather than
	// leaving the fd open across attempts.
	slot.iface->shutdown();

	slot.failCount++;
	if(slot.failCount < usbMaxQuickRetries)
		slot.backoffPollsLeft = 0; // could be transient (eg. drive still spinning up) - try again next cycle
	else
		slot.backoffPollsLeft = usbBackoffPolls; // give up on this device for a while - never permanently

	usb.isPresent = false;
	usb.isMounted = false;
	return false;
}

void WutFileSystemDriver::unmountUsbSlot(int usbSlotIdx)
{
	if(usbSlotIdx < 0 || usbSlotIdx >= usbSlotCount)
		return;

	WutDeviceState & usb = devices[slotUSB1 + usbSlotIdx];
	WutUsbPhysicalSlot & slot = usbSlots[usbSlotIdx];

	if(usb.isMounted)
		dvmWutUnmountUsb(slot.mountName);

	slot.failCount = 0;
	slot.backoffPollsLeft = 0;
	usb.isPresent = false;
	usb.isMounted = false;
	usb.unmountRequired = false;
	usb.prefix[0] = '\0';
}

bool WutFileSystemDriver::usbStillPresent(int usbSlotIdx)
{
	if(usbSlotIdx < 0 || usbSlotIdx >= usbSlotCount || !mochaReady)
		return false;

	WutDeviceState & usb = devices[slotUSB1 + usbSlotIdx];
	if(!usb.isMounted)
		return false;

	// Forces a genuine, uncached raw sector read through the active slot's
	// mounted disc - see dvmDiscProbePresence() in the libdvm fork. Unlike
	// stat()-ing the mount root (which libdvm's own sector cache, like
	// libfat's before it, can answer entirely from memory without ever
	// touching hardware again after mount), this always re-touches the
	// device, and does so under libdvm's own cache lock so it can't race
	// a concurrent file read/write on another thread.
	return dvmWutUsbStillPresent(usbSlots[usbSlotIdx].mountName);
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
		smb.label[0] = '\0';
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
		strncpy(out.label, devices[i].label, sizeof(out.label) - 1);
		out.label[sizeof(out.label) - 1] = '\0';
		strncpy(out.prefix, devices[i].prefix, sizeof(out.prefix) - 1);
		out.prefix[sizeof(out.prefix) - 1] = '\0';
		out.removable = (devices[i].id != DEVICE_SMB);
		out.autoMountAtStartup = (devices[i].id != DEVICE_SMB); // SMB needs explicit getSmb()->connect() first
		out.alwaysListed = (devices[i].id == DEVICE_SMB);

		WutStorageMetrics metrics;
		out.metricsValid = getStorageMetrics(devices[i].id, metrics);
		if(out.metricsValid)
		{
			out.totalBytes = metrics.totalBytes;
			out.freeBytes  = metrics.freeBytes;
			out.blockSize  = metrics.blockSize;
			out.readOnly   = metrics.readOnly;
		}
		else
		{
			out.totalBytes = 0;
			out.freeBytes  = 0;
			out.blockSize  = 0;
			out.readOnly   = false;
		}

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

	if(deviceId == DEVICE_USB)
		return tryMountUsbSlot(0) ? MountResult::Success : MountResult::DeviceNotFound;
	else if(deviceId == DEVICE_USB2)
		return tryMountUsbSlot(1) ? MountResult::Success : MountResult::DeviceNotFound;
	else if(deviceId == DEVICE_USB3)
		return tryMountUsbSlot(2) ? MountResult::Success : MountResult::DeviceNotFound;

	if(deviceId == DEVICE_SMB)
	{
		refreshSmbSlot();
		return smbDriver.isConnected() ? MountResult::Success : MountResult::DeviceNotFound;
	}

	// SD (and anything else using the DevicePresent()-style devoptab check)
	WutDeviceState & dev = devices[idx];

	if(dev.isMounted)
		return MountResult::Success;

	if(dev.unmountRequired)
	{
		// wut's devoptab drivers own the underlying block I/O themselves;
		// we just need to drop our local mount-state lock and re-verify.
		dev.unmountRequired = false;
		dev.isMounted = false;
	}

	if(!DevicePresent(dev.prefix))
	{
		dev.isPresent = false;
		return MountResult::DeviceNotFound;
	}

	dev.isPresent = true;
	dev.isMounted = true;
	refreshDisplayName(dev);
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
}

void WutFileSystemDriver::pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged)
{
	outRemovedCount = 0;
	deviceListChanged = false;

	// SD: re-verify via stat() on its devoptab prefix, same as before.
	{
		WutDeviceState & sd = devices[slotSD];
		bool present = DevicePresent(sd.prefix);

		if(sd.isPresent && !present)
		{
			sd.isPresent = false;
			sd.isMounted = false;
			sd.unmountRequired = true;

			if(outRemovedCount < MAX_STORAGE_DEVICES)
				removedIds[outRemovedCount++] = sd.id;
			deviceListChanged = true;
		}
		else if(!sd.isPresent && present)
		{
			sd.isPresent = true;
			refreshDisplayName(sd);
			deviceListChanged = true;
		}
	}

	// Cheap, read-only nsysuhs scan . Detects a real hardware-level 
	// attach/detach independently of whether Mocha's mount attempt has 
	// succeeded. On a change, a fresh insertion should be tried this same 
	// cycle.
	UsbHardwareSignature currentHwSig;
	ScanUsbHardwareSignature(currentHwSig);

	if(UsbHardwareSignatureChanged(currentHwSig, usbHwSignature))
	{
		// Only backoffPollsLeft, not failCount: a real hardware change
		// anywhere is worth one fresh look at every slot right now (that's
		// what backoffPollsLeft=0 buys), but a slot sitting on a
		// permanently-unmountable device (eg. an unsupported filesystem)
		// still needs to reach usbMaxQuickRetries and back off properly.
		for(int i = 0; i < usbSlotCount; i++)
			usbSlots[i].backoffPollsLeft = 0;
	}

	usbHwSignature = currentHwSig;

	// USB1-4 Independent Polling
	for(int usbIdx = 0; usbIdx < usbSlotCount; usbIdx++)
	{
		WutDeviceState & usb = devices[slotUSB1 + usbIdx];

		if(usb.isMounted)
		{
			if(!usbStillPresent(usbIdx))
			{
				unmountUsbSlot(usbIdx);

				if(outRemovedCount < MAX_STORAGE_DEVICES)
					removedIds[outRemovedCount++] = usb.id;
				deviceListChanged = true;
			}
		}
		else if(tryMountUsbSlot(usbIdx))
		{
			deviceListChanged = true;
		}
	}
}

bool WutFileSystemDriver::getStorageMetrics(int deviceId, WutStorageMetrics & outMetrics)
{
	int idx = findDeviceIndex(deviceId);
	if(idx < 0 || !devices[idx].isPresent)
		return false;

	struct statvfs st;
	if(statvfs(devices[idx].prefix, &st) == 0)
	{
		outMetrics.totalBytes = (uint64_t)st.f_blocks * st.f_frsize;
		outMetrics.freeBytes  = (uint64_t)st.f_bavail * st.f_frsize;
		outMetrics.blockSize  = (uint32_t)st.f_frsize;
		outMetrics.readOnly   = (st.f_flag & ST_RDONLY) != 0;
		return true;
	}

	outMetrics.readOnly = false;
	return false;
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

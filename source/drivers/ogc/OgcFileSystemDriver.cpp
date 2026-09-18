/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcFileSystemDriver.cpp
 ***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <fat.h>
#include <iso9660.h>

#include "OgcFileSystemDriver.h"

const OgcFatSlotDescriptor * OgcFileSystemDriver::findFatSlot(int deviceId) const
{
	for(int i = 0; i < fatSlotCount; i++)
		if(fatSlots[i].deviceId == deviceId)
			return &fatSlots[i];
	return nullptr;
}

void OgcFileSystemDriver::initFatSlotsAndAutoMount()
{
	for(int i = 0; i < fatSlotCount; i++)
	{
		if(!fatSlots[i].pollable)
			continue;

		DISC_INTERFACE * disc = fatSlots[i].getDisc();
		isPresentCache[fatSlots[i].deviceId] = disc->isInserted(disc);
	}

	// DVD is deliberately not probed here

	StorageDevice devices[MAX_STORAGE_DEVICES];
	int count = enumerateStorageDevices(devices);

	for(int i = 0; i < count; i++)
		if(devices[i].autoMountAtStartup)
			mountStorageDevice(devices[i].id);
}

void OgcFileSystemDriver::shutdown()
{
	smbDriver.shutdown();

	for(int i = 0; i < fatSlotCount; i++)
	{
		char mountPoint[10];
		snprintf(mountPoint, sizeof(mountPoint), "%s:", fatSlots[i].mountName);
		fatUnmount(mountPoint);
	}

	shutdownPlatform();
}

int OgcFileSystemDriver::enumerateStorageDevices(StorageDevice outDevices[MAX_STORAGE_DEVICES])
{
	int count = 0;

	for(int i = 0; i < fatSlotCount && count < MAX_STORAGE_DEVICES; i++)
	{
		const OgcFatSlotDescriptor & slot = fatSlots[i];
		StorageDevice & out = outDevices[count];

		out.id = slot.deviceId;
		snprintf(out.prefix, sizeof(out.prefix), "%s", slot.prefix);
		snprintf(out.name, sizeof(out.name), "%s", slot.displayName);
		snprintf(out.volumeLabel, sizeof(out.volumeLabel), "%s", volumeLabel[slot.deviceId]);
		out.removable = slot.removable;
		out.autoMountAtStartup = slot.autoMountAtStartup;
		out.alwaysListed = false;
		count++;
	}

	// DVD and SMB are the same on both platforms, and both alwaysListed
	if(count < MAX_STORAGE_DEVICES)
	{
		outDevices[count] = StorageDevice{ DEVICE_DVD, "dvd:/", "Data DVD", "", false, false, true };
		count++;
	}

	if(count < MAX_STORAGE_DEVICES)
	{
		outDevices[count] = StorageDevice{ DEVICE_SMB, "smb:/", "Network Share", "", false, false, true };
		count++;
	}

	return count;
}

MountResult OgcFileSystemDriver::mountFAT(int deviceId)
{
	const OgcFatSlotDescriptor * slot = findFatSlot(deviceId);
	if(!slot)
		return MountResult::DeviceNotFound;

	char mountPoint[10];
	snprintf(mountPoint, sizeof(mountPoint), "%s:", slot->mountName);

	DISC_INTERFACE * disc = slot->getDisc();

	if(unmountRequired[deviceId])
	{
		unmountRequired[deviceId] = false;
		fatUnmount(mountPoint);
		disc->shutdown(disc);
		isMounted[deviceId] = false;
		labelFetched[deviceId] = false;
	}

	// Distinguish "nothing there" from "something's there but we can't read it"
	if(!disc->startup(disc) || !disc->isInserted(disc))
	{
		isMounted[deviceId] = false;
		volumeLabel[deviceId][0] = '\0';
		labelFetched[deviceId] = false;
		return MountResult::DeviceNotFound;
	}

	bool mounted = fatMountSimple(slot->mountName, disc);
	isMounted[deviceId] = mounted;

	if(!mounted)
	{
		volumeLabel[deviceId][0] = '\0';
	}
	else if(!labelFetched[deviceId])
	{
		// Only ever looked up once per mount
		fatGetVolumeLabel(mountPoint, volumeLabel[deviceId]);
		labelFetched[deviceId] = true;
	}

	return mounted ? MountResult::Success : MountResult::MountFailed;
}

MountResult OgcFileSystemDriver::mountDVD()
{
	if(!dvdDisc)
		return MountResult::DeviceNotFound;

	if(unmountRequired[DEVICE_DVD])
	{
		unmountRequired[DEVICE_DVD] = false;
		ISO9660_Unmount("dvd:");
	}

	if(!dvdDisc->isInserted(dvdDisc))
	{
		isMounted[DEVICE_DVD] = false;
		isPresentCache[DEVICE_DVD] = false;
		return MountResult::DeviceNotFound;
	}

	isPresentCache[DEVICE_DVD] = true;

	if(!ISO9660_Mount("dvd", dvdDisc))
	{
		isMounted[DEVICE_DVD] = false;
		return MountResult::MountFailed;
	}

	isMounted[DEVICE_DVD] = true;
	return MountResult::Success;
}

MountResult OgcFileSystemDriver::attemptFatMount(int deviceId)
{
	MountResult result = mountFAT(deviceId);
	mountFailed[deviceId] = (result == MountResult::MountFailed);
	return result;
}

MountResult OgcFileSystemDriver::mountStorageDevice(int deviceId)
{
	if(deviceId < 0 || deviceId >= MAX_STORAGE_DEVICES)
		return MountResult::DeviceNotFound;

	if(deviceId == DEVICE_SMB)
		return smbDriver.isConnected() ? MountResult::Success : MountResult::DeviceNotFound;

	if(isMounted[deviceId])
		return MountResult::Success;

	// Re-resolve hardware topology first, so a device inserted since the
	// last poll cycle mounts on this attempt rather than the next one.
	prepareMount(deviceId);

	if(deviceId == DEVICE_DVD)
		return mountDVD();

	if(findFatSlot(deviceId))
		return attemptFatMount(deviceId);

	return MountResult::DeviceNotFound;
}

const char * OgcFileSystemDriver::mountResultMessage(int deviceId, MountResult result)
{
	if(deviceId == DEVICE_DVD)
		return result == MountResult::MountFailed ? "Unrecognized DVD format." : "No disc inserted!";

	if(deviceId == DEVICE_SMB)
		return "Network share not connected!";

	const OgcFatSlotDescriptor * slot = findFatSlot(deviceId);
	if(!slot)
		return "Device not found!";

	return result == MountResult::MountFailed ? unsupportedFormatMessage : slot->notFoundMessage;
}

void OgcFileSystemDriver::invalidateStorageDevice(int deviceId)
{
	if(deviceId < 0 || deviceId >= MAX_STORAGE_DEVICES)
		return;

	if(deviceId == DEVICE_SMB)
	{
		smbDriver.disconnect();
		return;
	}

	isMounted[deviceId] = false;
	unmountRequired[deviceId] = true;
	labelFetched[deviceId] = false;
	volumeLabel[deviceId][0] = '\0';

	// Whatever shows up in this slot next (even the same disk, replugged)
	// gets exactly one fresh attempt - see attemptFatMount().
	mountFailed[deviceId] = false;
}

void OgcFileSystemDriver::pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged)
{
	outRemovedCount = 0;
	deviceListChanged = pollPlatformExtra();

	// DVD is deliberately excluded from this loop - see mountDVD()/class
	// comment. Its isPresentCache/isMounted state only ever changes as a
	// side effect of an actual mount attempt.
	for(int i = 0; i < fatSlotCount; i++)
	{
		const OgcFatSlotDescriptor & slot = fatSlots[i];
		if(!slot.pollable)
			continue;

		DISC_INTERFACE * disc = slot.getDisc();
		bool present = disc->isInserted(disc);

		if(present != isPresentCache[slot.deviceId])
		{
			isPresentCache[slot.deviceId] = present;

			// Hardware topology changed either way
			mountFailed[slot.deviceId] = false;
		}

		if(isMounted[slot.deviceId] && !present)
		{
			invalidateStorageDevice(slot.deviceId);
			if(outRemovedCount < MAX_STORAGE_DEVICES)
				removedIds[outRemovedCount++] = slot.deviceId;
			deviceListChanged = true;
		}
		else if(present && !isMounted[slot.deviceId] && !mountFailed[slot.deviceId])
		{
			// Exactly one attempt per insertion
			if(attemptFatMount(slot.deviceId) == MountResult::Success)
				deviceListChanged = true;
		}
	}
}

bool OgcFileSystemDriver::isDevicePresent(int deviceId) const
{
	if(deviceId == DEVICE_SMB)
		return smbDriver.isConnected(); // informational only - SMB is alwaysListed

	if(deviceId == DEVICE_DVD)
		return isPresentCache[DEVICE_DVD]; // informational only - DVD is alwaysListed

	const OgcFatSlotDescriptor * slot = findFatSlot(deviceId);
	if(!slot || !slot->pollable)
		return false; // non-pollable slots (GC Loader) can't be known without an explicit mount attempt

	return isMounted[deviceId];
}

const char * OgcFileSystemDriver::getDevicePrefix(int device) const
{
	if(device < 0 || device >= MAX_STORAGE_DEVICES)
		return "";

	if(device == DEVICE_SMB)
		return "smb:/";

	if(device == DEVICE_DVD)
		return "dvd:/";

	const OgcFatSlotDescriptor * slot = findFatSlot(device);
	return slot ? slot->prefix : "";
}

const char * OgcFileSystemDriver::getMountPath(int device) const
{
	if(device < 0 || device >= MAX_STORAGE_DEVICES)
		return "";

	if(device == DEVICE_SMB)
		return smbDriver.getMountPath();

	if(!isMounted[device])
		return "";

	return getDevicePrefix(device);
}

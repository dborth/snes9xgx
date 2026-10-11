/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcFileSystemDriver.h
 *
 * Shared enumeration/mount/poll implementation for the GameCube and Wii
 * FileSystemDrivers. The two platforms differ only in:
 *   - which hot-pluggable FAT-formatted slots physically exist (SD Gecko
 *     A/B/port2/GC Loader on GameCube; SD + up to 3 USB MSD on Wii), and
 *   - how DVD access is wired up (GX DI on GameCube vs. the Wii's
 *     IOS-backed DI).
 *
 * DVD is deliberately NEVER probed in pollStorageDevices() - real disc
 * hardware access only belongs to a genuine user-initiated mount attempt.
 ***************************************************************************/
#pragma once
#include "../FileSystemDriver.h"
#include "OgcSmbDriver.h"
#include <ogc/disc_io.h>

//! One hot-pluggable FAT-formatted slot, shared shape between GameCube
//! (SD Gecko A/B/port2, GC Loader) and Wii (SD, USB1-3).
//!\ingroup grp_ogc
struct OgcFatSlotDescriptor
{
	int    deviceId; //!< Device enum value this slot mounts (DEVICE_SD, DEVICE_USB, ...)
	DISC_INTERFACE * (*getDisc)();  //!< resolved lazily - Wii's USB slots move between physical interfaces at runtime (see WiiUsbMulti)
	const char * mountName;         //!< devoptab basename, no colon/slash - eg. "carda" - passed to fatMountSimple()/fatUnmount()
	const char * prefix;            //!< precomputed "name:/" literal - returned by getMountPath()/enumerateStorageDevices() as-is
	const char * displayName; //!< Short name shown in device lists
	const char * notFoundMessage;   //!< eg. "SD card not found!" - see mountResultMessage()
	bool   removable;                //!< advisory only - not currently read by app code
	bool   autoMountAtStartup; //!< Mounted silently at boot
	bool   pollable;                 //!< true: probed every pollStorageDevices() cycle
};

//!Shared GameCube/Wii FileSystemDriver base: FAT slot enumeration, mounting and polling. DVD is deliberately never probed while polling.
//!\ingroup grp_ogc
class OgcFileSystemDriver : public FileSystemDriver
{
	public:
		void shutdown() override;

		int enumerateStorageDevices(StorageDevice outDevices[MAX_STORAGE_DEVICES]) override;
		MountResult mountStorageDevice(int deviceId) override;
		const char * mountResultMessage(int deviceId, MountResult result) override;
		void invalidateStorageDevice(int deviceId) override;
		void pollStorageDevices(int removedIds[MAX_STORAGE_DEVICES], int & outRemovedCount, bool & deviceListChanged) override;
		bool hasRemovableStorageDevices() const override { return true; }
		bool isDevicePresent(int deviceId) const override;

		const char * getDevicePrefix(int device) const override;
		const char * getMountPath(int device) const override;

		SmbDriver * getSmb() override { return &smbDriver; }

	protected:
		//!Table of FAT slots this platform supports; set by the concrete driver
		const OgcFatSlotDescriptor * fatSlots = nullptr;
		//!Number of entries in fatSlots
		int                          fatSlotCount = 0;
		//!Disc interface used to mount the DVD drive, or nullptr if the platform has none
		DISC_INTERFACE *             dvdDisc = nullptr;
		//!Text returned by mountResultMessage() when a present device fails to mount
		const char *                 unsupportedFormatMessage = "Unsupported format.";

		//! Runs the shared boot-time pass: probe every pollable fatSlots[]
		//! entry once for isPresentCache, then auto-mount whatever
		//! enumerateStorageDevices() marks autoMountAtStartup.
		void initFatSlotsAndAutoMount();

		//! Platform-specific extra teardown beyond the shared per-slot
		//! fatUnmount() pass (eg. Wii's WiiUsbMulti/USBStorage/DI_Close()).
		virtual void shutdownPlatform() {}

		//! Platform-specific extra polling that must run before the
		//! shared per-slot presence check (eg. Wii's WiiUsbMulti::scan(),
		//! which re-resolves which physical interface each USB slot's
		//! getDisc() currently points at). Returns true if this alone
		//! constitutes a device-list change. No-op/false by default.
		virtual bool pollPlatformExtra() { return false; }

		//! Platform-specific work that must run immediately before a
		//! user-initiated mount attempt on deviceId, so the attempt sees
		//! current hardware topology instead of whatever the last poll
		//! cycle happened to observe.
		virtual void prepareMount(int deviceId) {}

		//!Mounts the FAT filesystem on a slot with fatMountSimple(), unmounting first if unmountRequired is set.
		MountResult mountFAT(int deviceId);
		//!Mounts the DVD as ISO9660 if a disc is inserted.
		MountResult mountDVD();
		//!Single mountFAT() attempt that also records the outcome in mountFailed.
		MountResult attemptFatMount(int deviceId);

		//!\return the slot descriptor for deviceId, or nullptr if it is not a FAT slot
		const OgcFatSlotDescriptor * findFatSlot(int deviceId) const;

		bool isMounted[MAX_STORAGE_DEVICES]         = { false }; //!< A filesystem is currently mounted for this device
		bool unmountRequired[MAX_STORAGE_DEVICES]   = { false }; //!< A stale mount must be torn down before the next mount attempt
		bool isPresentCache[MAX_STORAGE_DEVICES]    = { false }; //!< raw hardware insertion only - see isDevicePresent()'s comment for why this is no longer what it returns
		bool mountFailed[MAX_STORAGE_DEVICES]       = { false }; //!< set by a real MountFailed; cleared on removal/reinsertion - see attemptFatMount()
		bool labelFetched[MAX_STORAGE_DEVICES]      = { false }; //!< fetched since the last mount/removal - see mountFAT()/invalidateStorageDevice()
		char volumeLabel[MAX_STORAGE_DEVICES][16]   = { { 0 } }; //!< Volume label cached for each device

		//!The network-share driver returned by getSmb()
		OgcSmbDriver smbDriver;
};

/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutFileSystemDriver.h
 ***************************************************************************/
#pragma once
#include "../FileSystemDriver.h"
#include "WutSmbDriver.h"
#include <mocha/disc_interface.h>
#include "WutUsbProbe.h"

//! One USB mass-storage slot as Cafe OS/Mocha actually exposes it: just a
//! DISC_INTERFACE handed to libdvm (vfat/exfat/ntfs) via dvm_wut.c - see
//! tryMountStorageSlot(). These are attach-order slots, not fixed physical
//! ports/port-groups.
struct WutStorageSlot
{
	const DISC_INTERFACE * iface;	//!< &Mocha_usb1_disc_interface .. &Mocha_usb3_disc_interface
	const char * mountName;			//!< devoptab basename, eg. "usb1" - also the dvm_wut.c volume name
	int failCount;					//!< consecutive mount failures since the last success or hardware change - see tryMountStorageSlot()
	int backoffPollsLeft;			//!< polls left to skip before the next probe attempt (0 = probe now)
};

//! State tracker for a single storage device slot.
struct WutDeviceState
{
	int  id;
	char name[16];			//!< human-readable base name, eg. "SD Card"
	char volumeLabel[16];	//!< volume label, best-effort - empty if none could be read
	char prefix[32];		//!< devoptab mount prefix, eg. "usb1:/", or the runtime FSA path for SD - "" whenever isMounted is false
	char stablePrefix[32];	//!< same string as prefix, but set once in init() and never cleared on unmount - this device's identity for path->device resolution (FindDevice()), independent of current mount state
	bool isPresent;			//!< found on the last poll
	bool isMounted;
	bool unmountRequired;
	bool labelFetched;		//!< volume label already looked up since the last mount/removal - see getVolumeLabel()
};

//! SD: a plain WHBMountSdCard() FSA mount, done once in init(). From then on
//! it is assumed always present and always mounted.
//!
//! USB: stock Cafe OS has no FAT/exFAT/NTFS driver at all, so mounting
//! always goes through libdvm - which also supplies genuine hot-unplug
//! detection via dvmWutVolumeStillPresent(). Raw disc access below libdvm
//! is through libmocha's DISC_INTERFACE, and is unavailable entirely if
//! Mocha isn't present.
//!
//! Hotplug (insertion): Mocha_*_isInserted() only reports whether we
//! already have the fd open - it doesn't re-probe hardware. While
//! unmounted, pollStorageDevices() retries dvmWutMountVolume(). A
//! read-only nsysuhs scan resets USB's backoff immediately on any real
//! hardware-level change.
//!
//! Hotplug (removal while mounted): dvmWutVolumeStillPresent() forces a
//! real, uncached raw sector read through the mounted disc.
//!
//! Backoff: a slot that opens but won't mount (wrong/unrecognized format,
//! or genuinely nothing there) gets a few quick immediate retries. The
//! interface is always left shutdown() between attempts.
//!
//! Volume labels (USB only): looked up once via libdvm
//! (dvmWutGetVolumeLabel()) right after a successful mount, never
//! repeated until the next unmount/remount cycle - see getVolumeLabel().
class WutFileSystemDriver : public FileSystemDriver
{
	public:
		void init() override;
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
		const int * getValidLoadDevices(int & outCount) const override;
		const int * getValidSaveDevices(int & outCount) const override;

		SmbDriver * getSmb() override { return &smbDriver; }

	private:
		static const int slotSD  = 0;
		static const int slotUSB1 = 1;
		static const int slotUSB2 = 2;
		static const int slotUSB3 = 3;
		static const int slotSMB = 4;
		static const int slotCount = 5;

		//! The 3 USB slots backed by storageSlots[]
		static const int storageSlotCount = 3;

		//! Cache sizing passed to dvmWutMountVolume() - tuned and hardware-confirmed
		static const unsigned cachePages     = 512;
		static const unsigned sectorsPerPage = 128;

		//! Backoff tuning for tryMountStorageSlot() - a handful of immediate retries, then back off
		static const int maxQuickRetries = 3;
		static const int backoffPolls    = 180;

		WutDeviceState     devices[slotCount];
		int                deviceCount;
		bool               mochaReady; //!< Mocha_InitLibrary() succeeded - USB unavailable entirely if not

		//! [0..2] = USB1..3, so storageSlots[i] backs devices[slotUSB1 + i]
		WutStorageSlot    storageSlots[storageSlotCount];

		//! Last poll's read-only nsysuhs scan across USB only. A change
		//! here means real hardware just appeared/disappeared.
		UsbHardwareSignature usbHwSignature;

		WutSmbDriver       smbDriver;

		int  findDeviceIndex(int deviceId) const;
		//! dvmName is null when there's no dvm volume behind the device -
		//! no label source then.
		void getVolumeLabel(WutDeviceState & dev, const char * dvmName);

		void refreshSmbSlot();

		//! The three below take an index into storageSlots[] (0..2 = USB1..3), not into devices[].
		//! Single-slot attempt: handles the backoff check, then a real dvmWutMountVolume() probe if warranted
		bool tryMountStorageSlot(int slotIdx);
		//! dvmWutUnmountVolume() on the slot, which shuts down its DISC_INTERFACE
		void unmountStorageSlot(int slotIdx);
		//! Real liveness check for an already-mounted slot: forces an uncached raw sector read through it
		bool slotStillPresent(int slotIdx);
};

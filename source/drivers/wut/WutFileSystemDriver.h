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

//! One storage slot as Cafe OS/Mocha actually exposes it. SD and
//! the three USB mass-storage slots are all just a DISC_INTERFACE handed
//! to libdvm (vfat/exfat/ntfs) via dvm_wut.c, so they share one
//! mounting/polling code path - see tryMountStorageSlot(). USB slots are
//! attach-order slots, not fixed physical ports/port-groups; SD is a
//! fixed slot with no equivalent topology to enumerate.
struct WutStorageSlot
{
	const DISC_INTERFACE * iface;	//!< &Mocha_sdio_disc_interface or &Mocha_usb1_disc_interface .. &Mocha_usb3_disc_interface
	const char * mountName;			//!< devoptab basename, eg. "sd" or "usb1" - also the dvm_wut.c volume name
	int failCount;					//!< consecutive mount failures since the last success or hardware change - see tryMountStorageSlot()
	int backoffPollsLeft;			//!< polls left to skip before the next probe attempt (0 = probe now)
};

//! State tracker for a single storage device slot.
struct WutDeviceState
{
	int  id;
	char name[16];			//!< human-readable base name, eg. "SD Card"
	char volumeLabel[16];	//!< volume label, best-effort - empty if none could be read
	char prefix[32];		//!< devoptab mount prefix, eg. "usb1:/", or the runtime FSA SD path in the non-Mocha fallback
	bool isPresent;			//!< found on the last poll
	bool isMounted;
	bool unmountRequired;
	bool labelFetched;		//!< volume label already looked up since the last mount/removal - see getVolumeLabel()
};

//! SD with Mocha (the normal case under Aroma): Mocha_sdio_disc_interface
//! + libdvm, identical in every respect to a USB slot - see
//! WutStorageSlot. This also means SD picks up exFAT/NTFS support it
//! never had through Cafe OS's own FSA mount.
//!
//! SD without Mocha (Mocha is an optional Aroma component): falls back to
//! a plain WHBMountSdCard() FSA mount, one attempt only - see
//! mountSdFallback(). There's no cheap presence probe available without
//! Mocha's raw disc access.
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
//! hardware-level change (SD has no equivalent topology to rescan).
//!
//! Hotplug (removal while mounted): dvmWutVolumeStillPresent() forces a
//! real, uncached raw sector read through the mounted disc.
//!
//! Backoff: a slot that opens but won't mount (wrong/unrecognized format,
//! or genuinely nothing there) gets a few quick immediate retries. The
//! interface is always left shutdown() between attempts.
//!
//! Volume labels (SD-via-Mocha and USB): looked up once via libdvm
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

		//! SD + 3 USB slots, all mounted identically through storageSlots
		static const int storageSlotCount = 4;

		//! Cache sizing passed to dvmWutMountVolume() - tuned and hardware-confirmed
		static const unsigned cachePages     = 512;
		static const unsigned sectorsPerPage = 128;

		//! Backoff tuning for tryMountStorageSlot() - a handful of immediate retries, then back off
		static const int maxQuickRetries = 3;
		static const int backoffPolls    = 180;

		WutDeviceState     devices[slotCount];
		int                deviceCount;
		bool               mochaReady;      //!< Mocha_InitLibrary() succeeded - USB unavailable entirely if not; SD falls back (see mountSdFallback())
		bool               sdUsesMochaPath; //!< true once init() decides SD goes through storageSlots[slotSD] rather than mountSdFallback() - tracks mochaReady, kept as its own flag for clarity at call sites

		//! [slotSD]=SD, [slotUSB1..slotUSB3]=USB1..3 - indices line up
		//! exactly with devices[] above, so no separate offset is needed.
		WutStorageSlot    storageSlots[storageSlotCount];

		//! Last poll's read-only nsysuhs scan across USB only. A change
		//! here means real hardware just appeared/disappeared.
		UsbHardwareSignature usbHwSignature;

		WutSmbDriver       smbDriver;

		int  findDeviceIndex(int deviceId) const;
		//! dvmName is null for SD on the non-Mocha fallback path - no
		//! label source there, see the class comment.
		void getVolumeLabel(WutDeviceState & dev, const char * dvmName);

		void refreshSmbSlot();

		//! Single-slot attempt: handles the backoff check, then a real dvmWutMountVolume() probe if warranted
		bool tryMountStorageSlot(int slotIdx);
		//! dvmWutUnmountVolume() on the slot, which shuts down its DISC_INTERFACE
		void unmountStorageSlot(int slotIdx);
		//! Real liveness check for an already-mounted slot: forces an uncached raw sector read through it
		bool slotStillPresent(int slotIdx);

		//! Non-Mocha SD path - see the class comment.
		MountResult mountSdFallback();
};

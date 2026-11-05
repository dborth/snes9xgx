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
#include "../Mutex.h"

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
	bool unmountable;				//!< the device on this slot reads fine but has nothing we can mount (eg. a WFS drive): never probed
									//!< again until the set of attached USB storage devices changes - see tryMountStorageSlot()
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
	bool labelFetched;		//!< volume label already looked up since the last mount/removal - see readVolumeLabel()
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
//! already have the fd open - it doesn't re-probe hardware. A read-only
//! nsysuhs scan counts the attached USB storage devices; only while that
//! is more than the devices already accounted for (mounted, or found
//! unmountable) does pollStorageDevices() try dvmWutMountVolumeEx() on
//! the remaining slots - an idle console makes no mount attempts at all.
//! Any real hardware-level change resets USB's backoff immediately.
//!
//! Hotplug (removal while mounted): dvmWutVolumeStillPresent() forces a
//! real, uncached raw sector read through the mounted disc.
//!
//! Backoff: a slot where nothing could be read (not ready, or nothing
//! there) gets a few quick immediate retries, then backs off. A slot whose
//! device reads fine but has nothing mountable on it (no MBR/VBR - eg. a
//! Wii U WFS drive - or no supported filesystem) is marked unmountable and
//! is not probed again until the USB hardware changes. The interface is
//! always left shutdown() between attempts.
//!
//! Volume labels (USB only): looked up once via libdvm
//! (dvmWutGetVolumeLabel()) right after a successful mount, never
//! repeated until the next unmount/remount cycle - see readVolumeLabel().
//!
//! Threading: pollStorageDevices() runs on the app's poll thread while the
//! main thread reads the device table (enumerateStorageDevices(),
//! getMountPath(), isDevicePresent()). stateLock guards devices[] and
//! storageSlots[]. It is only ever held for a few field reads/writes, never
//! across a Mocha/libdvm/nsysuhs call (those can block for seconds), so the
//! main thread never waits on a mount.
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
		void nameToDisplay(const char * rawName, char * out, size_t outSize) const override;

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

		//! Guards devices[] and storageSlots[] - see the class comment. Not recursive.
		mutable Mutex      stateLock;

		WutDeviceState     devices[slotCount];
		int                deviceCount;
		bool               mochaReady; //!< Mocha_InitLibrary() succeeded - USB unavailable entirely if not

		//! [0..2] = USB1..3, so storageSlots[i] backs devices[slotUSB1 + i]
		WutStorageSlot    storageSlots[storageSlotCount];

		//! Last poll's read-only nsysuhs scan across USB only. A change
		//! here means real hardware just appeared/disappeared.
		UsbHardwareSignature usbHwSignature;

		//! The nsysuhs scan has worked at least once. Until then (eg. nsysuhs unavailable) a scan says
		//! nothing about what is attached, so mounting falls back to trying the free slots; after that, a
		//! failed scan just means "no news" and the last known signature stands.
		bool usbScanWorked = false;

		WutSmbDriver       smbDriver;

		int  findDeviceIndex(int deviceId) const;
		//! dvmName is null when there's no dvm volume behind the device -
		//! no label source then.
		void readVolumeLabel(const char * dvmName, char (&out)[16]);

		void refreshSmbSlot();

		//! The three below take an index into storageSlots[] (0..2 = USB1..3), not into devices[].
		//! Single-slot attempt: handles the unmountable and backoff checks, then a real dvmWutMountVolumeEx() probe if warranted
		bool tryMountStorageSlot(int slotIdx);
		//! dvmWutUnmountVolume() on the slot, which shuts down its DISC_INTERFACE
		void unmountStorageSlot(int slotIdx);
		//! Real liveness check for an already-mounted slot: forces an uncached raw sector read through it
		bool slotStillPresent(int slotIdx);
};

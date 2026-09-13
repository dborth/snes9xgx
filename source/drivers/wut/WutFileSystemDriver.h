/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutFileSystemDriver.h
 ***************************************************************************/
#pragma once
#include "../FileSystemDriver.h"
#include "WutSmbDriver.h"
#include <coreinit/filesystem_fsa.h>
#include <mocha/disc_interface.h>
#include "WutUsbProbe.h"

//! Optional capacity/health telemetry for a single device, filled in on
//! request via getStorageMetrics(). Kept separate from the generic
//! FileSystemDriver contract (rather than, say, a virtual on the base)
//! since not every platform can supply it the same way; StorageDevice
//! carries a copy of this once enumerateStorageDevices() has queried it.
struct WutStorageMetrics
{
	uint64_t totalBytes;
	uint64_t freeBytes;
	uint32_t blockSize; //!< allocation unit / cluster size in bytes
	bool     readOnly;
};

//! One USB storage slot as Cafe OS actually exposes it.
struct WutUsbPhysicalSlot
{
	const DISC_INTERFACE * iface;      //!< &Mocha_usb1_disc_interface .. &Mocha_usb3_disc_interface
	const char *           mountName;  //!< devoptab basename, eg. "usb1" - also the dvm_wut.c volume name
	int                     failCount;       //!< consecutive mount failures since the last success or hardware change - see tryMountUsbSlot()
	int                     backoffPollsLeft; //!< polls left to skip before the next probe attempt (0 = probe now)
};

//! State tracker for a single storage device slot.
struct WutDeviceState
{
	int  id;
	char name[16];		//!< human-readable base name, eg. "SD Card" or derived from prefix (eg. "usb0")
	char label[16];		//!< volume label when we can read one via FSAGetVolumeInfo, empty otherwise
	char prefix[32];	//!< devoptab mount prefix, eg. "usb1:/" (whichever physical slot is active) or the runtime SD path
	bool isPresent;		//!< found on the last poll (stat()-able)
	bool isMounted;
	bool unmountRequired;
};

//!Wii U FileSystemDriver.
//!
//!SD: WHBMountSdCard() - a runtime-assigned FSA path, not a static devoptab name
//!USB: stock Cafe OS has no FAT/exFAT/ntfs driver for USB at all, so mounting
//!goes through libdvm - libdvm gets us exFAT/ntfs for free and is what supplies
//!dvmDiscProbePresence() for genuine hot-unplug detection below.
//! Raw disc access below libdvm is through libmocha's DISC_INTERFACE.
//!
//!Cafe OS exposes USB as up to four independent storage slots (see
//!WutUsbPhysicalSlot - these are attach-order slots, not fixed physical
//!ports/port-groups. All four are probed independently (usbSlots) and each
//!surfaces as its own device outward too (DEVICE_USB/USB2/USB3).
//!
//!Hotplug (insertion): Mocha_usbN_isInserted() only reports whether we
//!already have the fd open - it doesn't re-probe hardware - so it can't
//!drive polling the way __io_usbstorage.isInserted() does on GC/Wii, and
//!in particular can't be used to tell "still the same bad device" apart
//!from "something changed". Instead: while unmounted, pollStorageDevices() retries
//!dvmWutMountUsb() (which does force a fresh /dev/usb0N open attempt)
//!according to each slot's own poll-count backoff - see failCount/
//!backoffPollsLeft on WutUsbPhysicalSlot and tryMountUsbSlot() - and a
//!read-only nsysuhs scan (see WutUsbDiagnostics.h) resets that backoff
//!immediately on any real hardware-level change instead of waiting it out.
//!
//!Hotplug (removal while mounted): dvmWutUsbStillPresent() forces a real,
//!uncached raw sector read through the mounted disc rather than stat()-ing
//!the mount root, which libdvm's own sector cache can answer entirely from
//!memory without ever touching hardware again after mount.
//!
//!Backoff: a slot that opens but won't mount (wrong/unrecognized format,
//!or genuinely nothing there) gets a few quick immediate retries (in case
//!it's transient, eg. a drive still spinning up) and then backs off to a
//!probe roughly every backoffPollsLeft polls, rather than paying for a
//!full mount attempt - and the IOSU round trip that comes with it - every
//!single cycle forever. The interface is always left shutdown() between
//!attempts so a real unplug/replug is genuinely observed rather than
//!hidden behind a stale open fd; the backoff never becomes permanent.
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

		const char * getMountPath(int device) const override;
		const int * getValidLoadDevices(int & outCount) const override;
		const int * getValidSaveDevices(int & outCount) const override;

		//! WUT-specific extension: fills outMetrics with capacity/health info
		//! for deviceId via statvfs(). Returns false if the device isn't
		//! currently present or statvfs() failed.
		bool getStorageMetrics(int deviceId, WutStorageMetrics & outMetrics);

		SmbDriver * getSmb() override { return &smbDriver; }

	private:
		static const int slotSD  = 0;
		static const int slotUSB1 = 1;
		static const int slotUSB2 = 2;
		static const int slotUSB3 = 3;
		static const int slotSMB = 4;
		static const int slotCount = 5;

		static const int usbSlotCount = 3; //!< independent USB storage slots (see WutUsbPhysicalSlot - not fixed physical ports)

		//! Cache sizing passed to dvmWutMountUsb() - tuned and hardware-confirmed
		static const unsigned usbCachePages     = 512;
		static const unsigned usbSectorsPerPage = 128;

		//! Backoff tuning for tryMountUsbSlot() - a handful of immediate
		//! retries (covers a drive still spinning up / a transient IOSU
		//! hiccup), then back off to roughly one probe every
		//! usbBackoffPolls calls to pollStorageDevices() for a port group
		//! that just isn't mounting.
		static const int usbMaxQuickRetries = 3;
		static const int usbBackoffPolls    = 180;

		WutDeviceState     devices[slotCount];
		int                deviceCount;
		FSAClientHandle    fsaClient = -1; //!< used only for best-effort volume-label lookups; negative if unavailable
		bool               mochaReady; //!< Mocha_InitLibrary() succeeded - USB unavailable entirely if not

		WutUsbPhysicalSlot usbSlots[usbSlotCount];
		int                activeUsbSlot; //!< index into usbSlots backing DEVICE_USB right now, or -1 if unmounted

		//! Last poll's read-only nsysuhs scan (see WutUsbDiagnostics.h). A
		//! change here means real hardware just appeared/disappeared, so
		//! pollStorageDevices() uses it to reset the usb slots' backoff
		//! immediately rather than waiting out usbBackoffPolls (up to 3
		//! minutes) for a fresh insertion to be tried again.
		UsbHardwareSignature usbHwSignature;

		WutSmbDriver       smbDriver;

		int  findDeviceIndex(int deviceId) const;
		void refreshDisplayName(WutDeviceState & dev);

		void refreshSmbSlot();

		//! Single-slot attempt: handles the backoff check, then a real
		//! dvmWutMountUsb() probe if warranted - see failCount/
		//! backoffPollsLeft on WutUsbPhysicalSlot above.
		bool tryMountUsbSlot(int usbSlotIdx);
		//! dvmWutUnmountUsb() on the slot, which shuts down its
		//! DISC_INTERFACE once nothing else references it, so the next
		//! tryMountUsbSlot() genuinely re-probes hardware rather than
		//! reusing a stale fd. Safe to call whether or not USB is mounted.
		void unmountUsbSlot(int usbSlotIdx);
		//! Real liveness check for an already-mounted USB volume: forces an
		//! uncached raw sector read through the slot's mounted disc
		//! via dvmWutUsbStillPresent().
		bool usbStillPresent(int usbSlotIdx);
};

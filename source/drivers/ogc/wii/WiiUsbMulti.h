/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * WiiUsbMulti.h
 *
 * Multiple concurrent USB mass storage devices via IOS58's official USB2
 *
 * Stock libogc/libogc2's __io_usbstorage only ever mounts a single device:
 * its mount loop breaks as soon as it successfully mounts one LUN on one
 * device. USBStorage_Open/Close/GetMaxLUN/MountLUN/Read/Write
 * themselves are plain per-handle functions with no hidden shared state
 * beyond a scratch heap and a wait queue.
 *
 * Exposes three independent DISC_INTERFACEs, one per WiiFileSystemDriver
 * USB slot (DEVICE_USB/USB2/USB3).
 ***************************************************************************/
#pragma once
#include <ogc/disc_io.h>

#define WII_USB_MAX_DEVICES 3

namespace WiiUsbMulti
{
	//! Call once, after USBStorage_Initialize(). Builds the three
	//! DISC_INTERFACEs and resets all slot state to empty.
	void init();

	//! Closes any open slots' USB handles. Does NOT call
	//! USBStorage_Deinitialize() - that stays WiiFileSystemDriver's
	//! responsibility, since it's a shared global, not per-slot.
	void shutdown();

	//! Rescans attached USB mass storage devices via USB_GetDeviceList.
	//! Cheap enough to call once per device-checking thread cycle. Returns
	//! true if any slot newly attached or detached since the previous scan.
	bool scan();

	//! Slot's DISC_INTERFACE (slot 0..WII_USB_MAX_DEVICES-1). Always safe
	//! to pass to fatMountSimple/fatUnmount even when empty - startup()/
	//! isInserted() report false rather than touching hardware.
	DISC_INTERFACE * getInterface(int slot);
}

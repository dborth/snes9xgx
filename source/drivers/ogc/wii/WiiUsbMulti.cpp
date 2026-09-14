/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * WiiUsbMulti.cpp
 ***************************************************************************/
#include <string.h>

#define LIBOGC_INTERNAL
#include <ogc/disc_io.h>
#undef LIBOGC_INTERNAL

#include <ogc/usb.h>
#include <ogc/usbstorage.h>

#include "WiiUsbMulti.h"

// Not exported by ogc/usb.h - standard USB mass-storage class code (0x08),
// matches the private #define of the same name in libogc2's usbstorage.c.
#define USB_CLASS_MASS_STORAGE 0x08
#define USB_DEVLIST_MAXSIZE    8
#define DEVICE_TYPE_WII_USB_MULTI (('W'<<24)|('U'<<16)|('S'<<8)|'M')

namespace
{
	struct UsbSlot
	{
		usbstorage_handle handle;
		u8   lun;
		s32  deviceId; //!< IOS device_id from USB_GetDeviceList; -1 = empty
		u16  vid, pid;
		bool open;
		DISC_INTERFACE disc;
	};

	UsbSlot slots[WII_USB_MAX_DEVICES];

	template<int N> bool Startup(DISC_INTERFACE *)    { return slots[N].open; }
	template<int N> bool IsInserted(DISC_INTERFACE *) { return slots[N].open; }

	template<int N> bool ReadSectors(DISC_INTERFACE *, sec_t sector, sec_t numSectors, void * buffer)
	{
		if(!slots[N].open)
			return false;
		return USBStorage_Read(&slots[N].handle, slots[N].lun, sector, numSectors, (u8 *)buffer) >= 0;
	}

	template<int N> bool WriteSectors(DISC_INTERFACE *, sec_t sector, sec_t numSectors, const void * buffer)
	{
		if(!slots[N].open)
			return false;
		return USBStorage_Write(&slots[N].handle, slots[N].lun, sector, numSectors, (const u8 *)buffer) >= 0;
	}

	template<int N> bool EraseSectors(DISC_INTERFACE *, sec_t, sec_t) { return false; }
	template<int N> bool Flush(DISC_INTERFACE *) { return true; }

	template<int N> bool Shutdown(DISC_INTERFACE *)
	{
		if(slots[N].open)
			USBStorage_Close(&slots[N].handle);

		slots[N].open = false;
		slots[N].deviceId = -1;
		slots[N].vid = slots[N].pid = 0;
		return true;
	}

	//! Builds slot N's DISC_INTERFACE with the correct per-slot function pointers. numberOfSectors/bytesPerSector
	//! start at 0 and are filled in by SlotOpen() on a successful mount.
	template<int N> void InitSlotInterface()
	{
		slots[N].disc = DISC_INTERFACE
		{
			DEVICE_TYPE_WII_USB_MULTI,
			FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_USB,
			Startup<N>,
			IsInserted<N>,
			ReadSectors<N>,
			WriteSectors<N>,
			EraseSectors<N>,
			Flush<N>,
			Shutdown<N>,
			0, 0, 0
		};
	}

	//! Opens deviceId into slot i and mounts its first usable LUN, mirroring __io_usbstorage's own
	//! Open->GetMaxLUN->MountLUN sequence. Returns false (slot left empty) if the device has no readable LUN.
	bool SlotOpen(int i, s32 deviceId, u16 vid, u16 pid)
	{
		UsbSlot & s = slots[i];
		memset(&s.handle, 0, sizeof(s.handle));

		if(USBStorage_Open(&s.handle, deviceId, vid, pid) < 0)
			return false;

		s32 maxLun = USBStorage_GetMaxLUN(&s.handle);
		for(s32 lun = 0; lun < maxLun; lun++)
		{
			s32 retval = USBStorage_MountLUN(&s.handle, (u8)lun);
			if(retval < 0)
			{
				// Matches __io_usbstorage's own recovery step - a comms error on this LUN 
				// shouldn't be left in a wedged state before trying the next one.
				USBStorage_Reset(&s.handle);
				continue;
			}

			s.lun      = (u8)lun;
			s.deviceId = deviceId;
			s.vid      = vid;
			s.pid      = pid;
			s.open     = true;

			s.disc.numberOfSectors = s.handle.n_sectors[lun];
			s.disc.bytesPerSector  = s.handle.sector_size[lun];
			return true;
		}

		USBStorage_Close(&s.handle);
		return false;
	}
}

void WiiUsbMulti::init()
{
	InitSlotInterface<0>();
	InitSlotInterface<1>();
	InitSlotInterface<2>();

	for(auto & s : slots)
	{
		s.open = false;
		s.deviceId = -1;
		s.vid = s.pid = 0;
	}
}

void WiiUsbMulti::shutdown()
{
	for(auto & s : slots)
	{
		if(s.open)
			USBStorage_Close(&s.handle);
		s.open = false;
	}
}

bool WiiUsbMulti::scan()
{
	// USB_GetDeviceList() goes over IOS IPC, which requires a 32-byte cache-aligned buffer
	alignas(32) usb_device_entry buffer[USB_DEVLIST_MAXSIZE];
	u8 deviceCount = 0;
	bool changed = false;

	if(USB_GetDeviceList(buffer, USB_DEVLIST_MAXSIZE, USB_CLASS_MASS_STORAGE, &deviceCount) < 0)
		deviceCount = 0;

	// Drop any open slot whose device_id is no longer in the current list.
	for(auto & s : slots)
	{
		if(!s.open)
			continue;

		bool stillPresent = false;
		for(u8 j = 0; j < deviceCount; j++)
		{
			if(buffer[j].device_id == s.deviceId)
			{
				stillPresent = true;
				break;
			}
		}

		if(!stillPresent)
		{
			USBStorage_Close(&s.handle);
			s.open = false;
			s.deviceId = -1;
			s.vid = s.pid = 0;
			changed = true;
		}
	}

	// Assign newly-seen devices to free slots, in USB_GetDeviceList() order
	for(u8 j = 0; j < deviceCount; j++)
	{
		u16 vid = buffer[j].vid;
		u16 pid = buffer[j].pid;
		if(vid == 0 || pid == 0)
			continue;

		bool alreadyOpen = false;
		for(auto & s : slots)
		{
			if(s.open && s.deviceId == buffer[j].device_id)
			{
				alreadyOpen = true;
				break;
			}
		}
		if(alreadyOpen)
			continue;

		int freeSlot = -1;
		for(int i = 0; i < WII_USB_MAX_DEVICES; i++)
		{
			if(!slots[i].open)
			{
				freeSlot = i;
				break;
			}
		}
		if(freeSlot < 0)
			break; // all slots full - additional devices are ignored

		if(SlotOpen(freeSlot, buffer[j].device_id, vid, pid))
			changed = true;
	}

	return changed;
}

DISC_INTERFACE * WiiUsbMulti::getInterface(int slot)
{
	if(slot < 0 || slot >= WII_USB_MAX_DEVICES)
		return nullptr;
	return &slots[slot].disc;
}

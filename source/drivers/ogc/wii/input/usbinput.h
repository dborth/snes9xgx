#ifndef _USBINPUT_H_
#define _USBINPUT_H_

/*
 * Shared USB input discovery for the third-party USB controller drivers
 * (Retrode, Xbox 360, Hornet, Mayflash, DualShock 4).
 *
 * Discovery is done once here: a single USB_GetDeviceList() for every driver,
 * rate limited, and only acted on when the device list changed. Each driver
 * describes itself with a UsbInputDriver and is handed the list only when one
 * of its own devices is present and it has nothing attached. Drivers keep all
 * per-device I/O (reading reports, rumble/LEDs, removal handling) themselves.
 */

#ifdef HW_RVL

#include <gccore.h>
#include <ogc/usb.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	const char *name;

	// Cheap VID/PID test for one list entry. Must not do any USB I/O.
	bool (*matches)(const usb_device_entry *dev);

	// Called when at least one entry matches and the driver is not attached.
	// Receives the full list (some devices enumerate as several entries).
	// May open/claim devices. Success is reported through attached().
	void (*attach)(const usb_device_entry *list, u8 count);

	// True while the driver holds an open device.
	bool (*attached)(void);
} UsbInputDriver;

extern const UsbInputDriver Retrode_UsbDriver;
extern const UsbInputDriver XBOX360_UsbDriver;
extern const UsbInputDriver Hornet_UsbDriver;
extern const UsbInputDriver Mayflash_UsbDriver;
extern const UsbInputDriver DS4_UsbDriver;

// Call once per input update, before the drivers' *_ScanPads().
void UsbInput_Scan(void);

// Request a scan on the next UsbInput_Scan(). Safe to call from a USB
// callback (it only sets a flag). Drivers call it from their removal callback.
void UsbInput_Rescan(void);

// Queue a device handle to be closed from UsbInput_Scan(). Drivers call this
// from their removal callback for every handle they hold: USB callbacks run in
// the IPC interrupt handler, where USB_CloseDevice() (a blocking IOS call)
// cannot be used. Safe to call from a callback.
void UsbInput_DeferClose(s32 fd);

#ifdef __cplusplus
}
#endif

#endif // HW_RVL
#endif

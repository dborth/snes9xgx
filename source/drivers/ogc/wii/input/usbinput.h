#ifndef _USBINPUT_H_
#define _USBINPUT_H_

/*
 * Shared USB input framework for the third-party USB controller drivers
 * (Retrode, Xbox 360, Hornet, Mayflash, DualShock 4, and the pen tablet).
 *
 * Discovery is done once here: a single USB_GetDeviceList() for every driver,
 * rate limited, and only acted on when the device list changed. Each driver
 * describes itself with a UsbInputDriver and is handed the list only when one
 * of its own devices is present and it has nothing attached. Drivers keep the
 * device-specific I/O (decoding reports, rumble/LEDs) themselves; everything
 * every driver needs is here:
 *
 *   - state       UsbInput_GetState(): connected / replug required / not found.
 *                 Tracked here from attach()'s result, so drivers keep no
 *                 status flag and report no text.
 *   - opening     UsbInput_OpenDevice(), UsbInput_FirstEndpoint()
 *   - reading     UsbReader: an async interrupt-IN read loop that is safe
 *                 against a device being unplugged and replugged mid-read
 *   - buttons     UsbInputButtons: held/down/up edges latched once per frame
 *   - removal     UsbInput_DeferClose(), UsbInput_Rescan()
 *
 * ADDING A DRIVER
 *
 *   1. Write <name>.c/.h. Define a `const UsbInputDriver Name_UsbDriver` with:
 *        matches   cheap VID/PID test of one list entry (no USB I/O)
 *        attach    walk the list, open what is yours with UsbInput_OpenDevice(),
 *                  keep the handle, ask for removal notification, and return
 *                  UsbInput_AttachResult(attached, replugNeeded).
 *                  Only called while attached() is false, so it needs no guard.
 *        attached  true while you hold an open device
 *      and export the per-frame accessors the input driver needs
 *      (Name_ScanPads() and your buttons/sticks).
 *   2. Declare the driver below, add it to the table in usbinput.c.
 *   3. In WiiUsbPads.cpp: call Name_ScanPads() from scan(), and add the
 *      button mapping and read(). The status text on the credits screen needs
 *      no change (it walks the table); the status strip needs a case in
 *      WiiUsbPads::getStatus() only if the controller has its own icon.
 *
 *   Removal callback: runs in the IPC interrupt handler. Clear your state,
 *   UsbReader_Stop() your reader, UsbInput_DeferClose() every handle you hold
 *   (USB_CloseDevice() blocks and cannot be called there), then
 *   UsbInput_Rescan(). retrode.c is the smallest complete example for a
 *   polled device, ds4.c for one read through a UsbReader.
 */

#include <gccore.h>
#include <ogc/usb.h>

#ifdef __cplusplus
extern "C" {
#endif

//! What a driver's controller is doing, as seen by the rest of the program
typedef enum
{
	USBINPUT_STATE_NOT_FOUND,        // nothing of this driver's is plugged in
	USBINPUT_STATE_CONNECTED,        // attached and usable
	USBINPUT_STATE_REPLUG_REQUIRED   // plugged in, but IOS will not describe it until it is unplugged and plugged back in
} UsbInputState;

//! What attach() achieved. Build it with UsbInput_AttachResult().
typedef enum
{
	USBINPUT_ATTACH_FAILED,          // nothing attached; the scan may retry
	USBINPUT_ATTACH_OK,              // attached
	USBINPUT_ATTACH_REPLUG           // nothing attached: a matching device could not be described
} UsbAttachResult;

typedef struct
{
	const char *name;

	// Cheap VID/PID test for one list entry. Must not do any USB I/O.
	bool (*matches)(const usb_device_entry *dev);

	// Called when at least one entry matches and the driver is not attached().
	// Receives the full list (some devices enumerate as several entries).
	// May open/claim devices.
	UsbAttachResult (*attach)(const usb_device_entry *list, u8 count);

	// True while the driver holds everything it needs. A driver that needs
	// several handles (Mayflash MF105: one per port) is attached only once it
	// has them all, which is what keeps attach() being called until it does.
	bool (*attached)(void);

	// Optional (NULL = same as attached): true while anything usable is open,
	// even if attached() is false. Decides CONNECTED in UsbInput_GetState().
	bool (*connected)(void);
} UsbInputDriver;

extern const UsbInputDriver Retrode_UsbDriver;
extern const UsbInputDriver XBOX360_UsbDriver;
extern const UsbInputDriver Hornet_UsbDriver;
extern const UsbInputDriver Mayflash_UsbDriver;
extern const UsbInputDriver DS4_UsbDriver;
extern const UsbInputDriver Tablet_UsbDriver;

/****************************************************************************
 * Discovery and state
 ***************************************************************************/

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

// The state of one driver. A replug request stands only while a device that
// needs it is plugged in.
UsbInputState UsbInput_GetState(const UsbInputDriver *driver);

// Every registered driver, in registration order
int UsbInput_DriverCount(void);
const UsbInputDriver *UsbInput_DriverAt(int index);

// For attach() to return: OK if it ended up attached, else REPLUG if it met a
// device it could not describe, else FAILED.
static inline UsbAttachResult UsbInput_AttachResult(bool attached, bool replugNeeded)
{
	return attached ? USBINPUT_ATTACH_OK : (replugNeeded ? USBINPUT_ATTACH_REPLUG : USBINPUT_ATTACH_FAILED);
}

/****************************************************************************
 * Opening devices (from attach())
 ***************************************************************************/

typedef enum
{
	USBINPUT_OPEN_OK,            // *fd is open and *desc filled in: free it with USB_FreeDescriptors()
	USBINPUT_OPEN_FAILED,        // could not be opened (it may be claimed elsewhere); nothing to clean up
	USBINPUT_OPEN_NEEDS_REPLUG   // opened but not describable: closed again, nothing to clean up
} UsbOpenResult;

// Opens one list entry and reads its descriptors.
UsbOpenResult UsbInput_OpenDevice(const usb_device_entry *entry, s32 *fd, usb_devdesc *desc);

// For a device that turned out not to be the one wanted: frees what
// UsbInput_OpenDevice() returned and closes the device.
void UsbInput_Discard(s32 *fd, usb_devdesc *desc);

// First endpoint of the first interface of the first configuration, or
// USBINPUT_NO_ENDPOINT if the descriptors have none.
#define USBINPUT_NO_ENDPOINT 0xFF
u8 UsbInput_FirstEndpoint(const usb_devdesc *desc);

/****************************************************************************
 * Button edges
 ***************************************************************************/

//! Held/down/up for a device that reports held state only
typedef struct
{
	u32 held;
	u32 down;   // pressed since the last latch
	u32 up;     // released since the last latch
} UsbInputButtons;

// Once per frame, with the buttons as they are now
static inline void UsbInputButtons_Latch(UsbInputButtons *b, u32 now)
{
	b->down = now & ~b->held;
	b->up = b->held & ~now;
	b->held = now;
}

static inline void UsbInputButtons_Clear(UsbInputButtons *b)
{
	b->held = b->down = b->up = 0;
}

/****************************************************************************
 * Asynchronous reading
 ***************************************************************************/

/*
 * A read loop on one interrupt IN endpoint: each completed read is passed to
 * onReport and the next one is submitted straight away. A read that completes
 * after its device was unplugged (or replugged, when IOS has handed the same
 * descriptor out again) is ignored, so it can neither touch the new device's
 * state nor start a second loop.
 *
 * Declare one static per device, with the buffer it reads into (static, 32
 * byte aligned: a read still pending at removal completes into it later):
 *
 *   static u8 ATTRIBUTE_ALIGN(32) buf[32];
 *   static UsbReader reader = { .buf = buf, .size = sizeof(buf), .onReport = onReport };
 *
 * UsbReader_Bind() when attached, UsbReader_Start() every frame from
 * ScanPads() (a no-op while reading; restarts a loop that stopped after a
 * transfer error), UsbReader_Stop() from the removal callback.
 */
typedef struct UsbReader
{
	u8 *buf;
	u16 size;
	// Called from the IPC interrupt handler: parse and store, nothing blocking
	void (*onReport)(const u8 *report, s32 length);

	// Set by UsbReader_Bind()
	s32 fd;
	u8 endpoint;

	// Private
	volatile bool reading;
	volatile u32 generation;
	u8 slot;
	bool registered;
} UsbReader;

void UsbReader_Bind(UsbReader *reader, s32 fd, u8 endpoint);
void UsbReader_Start(UsbReader *reader);
void UsbReader_Stop(UsbReader *reader);

#ifdef __cplusplus
}
#endif

#endif

#ifdef HW_RVL
#include "usbinput.h"

#define USBINPUT_MAX_ENTRIES      64   // libogc2 tracks up to 32 devices per host (HID + vendor)
#define USBINPUT_SCAN_FRAMES      30   // ~0.5 s at 60 fps; hot-plug latency is not critical
#define USBINPUT_ATTACH_RETRIES   3    // extra attempts after a failed attach with an unchanged list
#define USBINPUT_CLASS_ANY        0
#define USBINPUT_CLASS_VENDOR     0xFF
#define USBINPUT_CLOSE_QUEUE      8    // a removal event closes at most a few handles (MF105 holds two)

static const UsbInputDriver * const drivers[] =
{
	&Retrode_UsbDriver,
	&XBOX360_UsbDriver,
	&Hornet_UsbDriver,
	&Mayflash_UsbDriver,
	&DS4_UsbDriver
};
#define NUM_DRIVERS ((int)(sizeof(drivers) / sizeof(drivers[0])))

static usb_device_entry entries[USBINPUT_MAX_ENTRIES];
static bool usbReady = false;
static bool haveSignature = false;
static u32 lastSignature = 0;
static u32 framesUntilScan = 0;
static u8 retriesLeft = 0;
static volatile bool rescanRequested = true;
static volatile s32 closeQueue[USBINPUT_CLOSE_QUEUE];
static volatile u8 closeCount = 0;

void UsbInput_Rescan(void)
{
	rescanRequested = true;
}

void UsbInput_DeferClose(s32 fd)
{
	if (fd == 0)
	{
		return;
	}

	u32 level = IRQ_Disable();
	if (closeCount < USBINPUT_CLOSE_QUEUE)
	{
		closeQueue[closeCount++] = fd;
	}
	IRQ_Restore(level);
}

// Main thread only. The queue is also written from the IPC interrupt handler,
// so it is copied out with interrupts disabled and closed afterwards.
static void flushDeferredCloses(void)
{
	if (closeCount == 0)
	{
		return;
	}

	s32 pending[USBINPUT_CLOSE_QUEUE];
	u8 n;

	u32 level = IRQ_Disable();
	n = closeCount;
	for (u8 i = 0; i < n; ++i)
	{
		pending[i] = closeQueue[i];
	}
	closeCount = 0;
	IRQ_Restore(level);

	for (u8 i = 0; i < n; ++i)
	{
		USB_CloseDevice(&pending[i]);
	}
}

// FNV-1a over what identifies a device on the bus. device_id is unique per
// attach on the v5 USB host, so replugging changes the signature.
static u32 signature(const usb_device_entry *list, u8 count)
{
	u32 h = 2166136261u;
	for (int i = 0; i < count; ++i)
	{
		u32 words[3] = { (u32) list[i].device_id, list[i].vid, list[i].pid };
		for (int w = 0; w < 3; ++w)
		{
			for (int b = 0; b < 4; ++b)
			{
				h ^= (words[w] >> (8 * b)) & 0xFF;
				h *= 16777619u;
			}
		}
	}
	return h;
}

static s32 fetchDeviceList(u8 *count)
{
	u8 n = 0;

	// One call covers both the HID and the vendor-class host.
	if (USB_GetDeviceList(entries, USBINPUT_MAX_ENTRIES, USBINPUT_CLASS_ANY, &n) >= 0)
	{
		*count = n;
		return 0;
	}

	// Fallback for an IOS that rejects the "any class" filter: one call per class.
	u8 hid = 0, vendor = 0;
	if (USB_GetDeviceList(entries, USBINPUT_MAX_ENTRIES, USB_CLASS_HID, &hid) < 0)
	{
		return -1;
	}
	if (hid > USBINPUT_MAX_ENTRIES)
	{
		hid = USBINPUT_MAX_ENTRIES;
	}
	if (USB_GetDeviceList(entries + hid, USBINPUT_MAX_ENTRIES - hid, USBINPUT_CLASS_VENDOR, &vendor) < 0)
	{
		vendor = 0;
	}
	*count = hid + vendor;
	return 0;
}

static bool anyMatch(const UsbInputDriver *d, const usb_device_entry *list, u8 count)
{
	for (int i = 0; i < count; ++i)
	{
		if (d->matches(&list[i]))
		{
			return true;
		}
	}
	return false;
}

void UsbInput_Scan(void)
{
	// Every frame, before the rate limit: handles of removed devices are
	// released promptly, and before the list is read again for a replug.
	flushDeferredCloses();

	bool forced = rescanRequested;
	if (!forced && framesUntilScan > 0)
	{
		framesUntilScan--;
		return;
	}
	rescanRequested = false;
	framesUntilScan = USBINPUT_SCAN_FRAMES - 1;

	if (!usbReady)
	{
		// Idempotent in libogc2; normally already done by the file system driver.
		if (USB_Initialize() < 0)
		{
			return;
		}
		usbReady = true;
	}

	u8 count = 0;
	if (fetchDeviceList(&count) < 0)
	{
		return;
	}

	u32 sig = signature(entries, count);
	bool fresh = forced || !haveSignature || sig != lastSignature;
	haveSignature = true;
	lastSignature = sig;

	if (fresh)
	{
		retriesLeft = USBINPUT_ATTACH_RETRIES;
	}
	else if (retriesLeft == 0)
	{
		return; // unchanged list and nothing left to retry
	}

	bool unresolved = false;
	for (int i = 0; i < NUM_DRIVERS; ++i)
	{
		const UsbInputDriver *d = drivers[i];
		if (d->attached() || !anyMatch(d, entries, count))
		{
			continue;
		}
		d->attach(entries, count);
		if (!d->attached())
		{
			unresolved = true;
		}
	}

	if (!unresolved)
	{
		retriesLeft = 0;
	}
	else if (!fresh)
	{
		retriesLeft--;
	}
}

#endif

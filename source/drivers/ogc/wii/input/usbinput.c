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
	&DS4_UsbDriver,
	&Tablet_UsbDriver,
};
#define NUM_DRIVERS ((int)(sizeof(drivers) / sizeof(drivers[0])))

#define USBINPUT_MAX_READERS      4    // UsbReaders in use at once (the slot is 4 bits of the read's token)
#define USBINPUT_TOKEN_GEN_MASK   0x0FFFFFFFu
#define USBINPUT_TOKEN_SLOT_SHIFT 28

static usb_device_entry entries[USBINPUT_MAX_ENTRIES];
static bool usbReady = false;
static bool haveSignature = false;
static u32 lastSignature = 0;
static u32 framesUntilScan = 0;
static u8 retriesLeft = 0;
static volatile bool rescanRequested = true;
static volatile s32 closeQueue[USBINPUT_CLOSE_QUEUE];
static volatile u8 closeCount = 0;
// Per driver: a matching device is plugged in that could not be described
static bool replugRequired[NUM_DRIVERS];
static UsbReader *readers[USBINPUT_MAX_READERS];
static u8 readerCount = 0;

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
		if (!anyMatch(d, entries, count))
		{
			replugRequired[i] = false; // unplugged: nothing left to replug
			continue;
		}
		if (d->attached())
		{
			replugRequired[i] = false;
			continue;
		}
		UsbAttachResult result = d->attach(entries, count);
		bool attached = d->attached();
		replugRequired[i] = !attached && result == USBINPUT_ATTACH_REPLUG;
		if (!attached)
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

/****************************************************************************
 * Driver state
 ***************************************************************************/

int UsbInput_DriverCount(void)
{
	return NUM_DRIVERS;
}

const UsbInputDriver *UsbInput_DriverAt(int index)
{
	return (index >= 0 && index < NUM_DRIVERS) ? drivers[index] : NULL;
}

UsbInputState UsbInput_GetState(const UsbInputDriver *driver)
{
	bool connected = driver->connected ? driver->connected() : driver->attached();
	if (connected)
	{
		return USBINPUT_STATE_CONNECTED;
	}

	for (int i = 0; i < NUM_DRIVERS; ++i)
	{
		if (drivers[i] == driver)
		{
			return replugRequired[i] ? USBINPUT_STATE_REPLUG_REQUIRED : USBINPUT_STATE_NOT_FOUND;
		}
	}
	return USBINPUT_STATE_NOT_FOUND;
}

/****************************************************************************
 * Opening devices
 ***************************************************************************/

UsbOpenResult UsbInput_OpenDevice(const usb_device_entry *entry, s32 *fd, usb_devdesc *desc)
{
	if (USB_OpenDevice(entry->device_id, entry->vid, entry->pid, fd) < 0)
	{
		return USBINPUT_OPEN_FAILED;
	}

	if (USB_GetDescriptors(*fd, desc) < 0)
	{
		// Only unplugging the device resets this
		USB_CloseDevice(fd);
		return USBINPUT_OPEN_NEEDS_REPLUG;
	}

	return USBINPUT_OPEN_OK;
}

void UsbInput_Discard(s32 *fd, usb_devdesc *desc)
{
	USB_FreeDescriptors(desc);
	USB_CloseDevice(fd);
}

u8 UsbInput_FirstEndpoint(const usb_devdesc *desc)
{
	if (desc->configurations == NULL || desc->configurations->interfaces == NULL ||
		desc->configurations->interfaces->endpoints == NULL)
	{
		return USBINPUT_NO_ENDPOINT;
	}
	return desc->configurations->interfaces->endpoints->bEndpointAddress;
}

/****************************************************************************
 * Asynchronous reading
 ***************************************************************************/

// What a read is tagged with: which reader, and the generation it was submitted in
static void *readToken(const UsbReader *reader)
{
	return (void *) (((u32) reader->slot << USBINPUT_TOKEN_SLOT_SHIFT) | (reader->generation & USBINPUT_TOKEN_GEN_MASK));
}

static int readCallback(int res, void *usrdata);

static int submitRead(UsbReader *reader)
{
	return USB_ReadIntrMsgAsync(reader->fd, reader->endpoint, reader->size, reader->buf, &readCallback, readToken(reader));
}

static int readCallback(int res, void *usrdata)
{
	u32 token = (u32) usrdata;
	u32 slot = token >> USBINPUT_TOKEN_SLOT_SHIFT;
	if (slot >= readerCount)
	{
		return 1;
	}

	UsbReader *reader = readers[slot];

	// A read submitted for a device since unplugged may complete after a quick
	// replug: it must not touch the new device's state or resubmit
	if ((token & USBINPUT_TOKEN_GEN_MASK) != (reader->generation & USBINPUT_TOKEN_GEN_MASK) || !reader->reading)
	{
		return 1;
	}

	if (res < 0)
	{
		// No transfer is pending any more: UsbReader_Start() restarts the loop
		reader->reading = false;
		return 1;
	}

	reader->onReport(reader->buf, res);

	if (submitRead(reader) < 0)
	{
		reader->reading = false;
	}
	return 1;
}

static bool registerReader(UsbReader *reader)
{
	if (reader->registered)
	{
		return true;
	}
	if (readerCount >= USBINPUT_MAX_READERS)
	{
		return false;
	}
	reader->slot = readerCount;
	readers[readerCount++] = reader;
	reader->registered = true;
	return true;
}

void UsbReader_Bind(UsbReader *reader, s32 fd, u8 endpoint)
{
	reader->reading = false;
	reader->generation++;
	reader->fd = fd;
	reader->endpoint = endpoint;
}

void UsbReader_Start(UsbReader *reader)
{
	if (reader->reading || !registerReader(reader))
	{
		return;
	}

	reader->reading = true;
	if (submitRead(reader) < 0)
	{
		// Nothing is pending: the next call tries again
		reader->reading = false;
	}
}

void UsbReader_Stop(UsbReader *reader)
{
	reader->reading = false;
	reader->generation++;
}

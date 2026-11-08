#include <gccore.h>
#include <malloc.h>
#include <string.h>
#include <ogc/usb.h>
#include "tablet.h"
#include "tabletdesc.h"
#include "usbinput.h"

// The pen tablet makers whose devices this driver opens, by USB vendor ID.
// Nothing else is ever opened by it, and only tablets that describe their pen
// in their HID report descriptor are used.
static const u16 tabletVendors[] =
{
	0x056A, // Wacom (Intuos, One by Wacom, Bamboo, CTL/CTH series)
	0x256C, // Huion, and Gaomon (Huion's brand, same vendor ID)
	0x28BD, // XP-Pen, and UGEE (XP-Pen's parent, same vendor ID)
	0x2FEB, // Veikk
	0x5543, // UC-Logic (the tablets sold as Turcom, Monoprice, Parblo, ...)
	0x172F, // Waltop (the tablets sold as Trust, Genius, Medion, ...)
};

#define TABLET_REPORT_MAX  64   // a full-speed interrupt packet
#define TABLET_RDESC_MAX   2048 // larger than any tablet's report descriptor
#define TABLET_REJECT_MAX  8

#define HID_REQ_SET_REPORT  0x09
#define HID_REPORT_FEATURE  0x03

static s32 deviceId = 0;
static tabletdesc_pen *pen = NULL; // allocated on attach, freed on removal
static s32 maxX = 1, maxY = 1;

// The interfaces of a tablet that are not its pen (its keys, its mouse), so
// retries of the scan do not open them again. device_id changes on replug.
static s32 rejected[TABLET_REJECT_MAX];
static u8 rejectedNext = 0;

static volatile s32 reportX = 0;
static volatile s32 reportY = 0;
static volatile u32 reportButtons = 0;
static volatile bool reportInRange = false;

static UsbInputButtons padButtons;
static bool inRange = false;
static float posX = 0, posY = 0;

static bool isTabletVendor(u16 vid)
{
	for (int i = 0; i < (int)(sizeof(tabletVendors) / sizeof(tabletVendors[0])); ++i)
	{
		if (tabletVendors[i] == vid)
		{
			return true;
		}
	}
	return false;
}

static bool isRejected(s32 id)
{
	for (int i = 0; i < TABLET_REJECT_MAX; ++i)
	{
		if (rejected[i] == id)
		{
			return true;
		}
	}
	return false;
}

static void reject(s32 id)
{
	rejected[rejectedNext] = id;
	rejectedNext = (rejectedNext + 1) % TABLET_REJECT_MAX;
}

static void parseReport(const u8 *report, s32 len)
{
	if (len <= 0)
	{
		return;
	}

	u8 id = pen->uses_ids ? report[0] : 0;
	const u8 *data = pen->uses_ids ? report + 1 : report;
	u32 n = pen->uses_ids ? len - 1 : len;

	if (id != pen->report_id)
	{
		return;
	}
	if (pen->valid.bit_size && !TabletDesc_Get(&pen->valid, data, n))
	{
		return;
	}

	bool range = pen->in_range.bit_size ? TabletDesc_Get(&pen->in_range, data, n) != 0 : true;
	u32 buttons = 0;

	if (range)
	{
		bool touching = pen->tip.bit_size
			? (TabletDesc_Get(&pen->tip, data, n) || TabletDesc_Get(&pen->eraser, data, n))
			: TabletDesc_Get(&pen->pressure, data, n) > 0;
		if (touching) buttons |= TABLET_BUTTON_TIP;
		if (TabletDesc_Get(&pen->barrel, data, n)) buttons |= TABLET_BUTTON_BARREL;
		if (TabletDesc_Get(&pen->barrel2, data, n)) buttons |= TABLET_BUTTON_BARREL2;

		reportX = TabletDesc_Get(&pen->x, data, n) - pen->x.logical_min;
		reportY = TabletDesc_Get(&pen->y, data, n) - pen->y.logical_min;
	}

	reportButtons = buttons;
	reportInRange = range;
}

// The DMA buffer stays static: a read still pending when the tablet goes away
// may complete into it after the removal.
static u8 ATTRIBUTE_ALIGN(32) buf[TABLET_REPORT_MAX];
static UsbReader reader = { .buf = buf, .size = TABLET_REPORT_MAX, .onReport = &parseReport };

static void resetState()
{
	reportX = 0;
	reportY = 0;
	reportButtons = 0;
	reportInRange = false;
	UsbInputButtons_Clear(&padButtons);
	inRange = false;
}

static int removal_cb(int result, void *usrdata)
{
	(void) result;
	s32 fd = (s32) usrdata;
	if (fd == deviceId)
	{
		UsbReader_Stop(&reader);
		deviceId = 0;
		UsbInput_DeferClose(fd);
		resetState();
		UsbInput_Rescan();
	}
	return 1;
}

// The interrupt IN endpoint of an HID interface that can be the pen's. Boot
// interfaces are the tablet's mouse or keyboard, left to libogc's drivers.
static const usb_endpointdesc *findPenEndpoint(const usb_interfacedesc *inter)
{
	if (inter->bInterfaceClass != USB_CLASS_HID || inter->bInterfaceSubClass == USB_SUBCLASS_BOOT ||
			inter->endpoints == NULL)
	{
		return NULL;
	}

	for (int e = 0; e < inter->bNumEndpoints; ++e)
	{
		const usb_endpointdesc *ep = &inter->endpoints[e];
		if ((ep->bmAttributes & 0x03) == USB_ENDPOINT_INTERRUPT && (ep->bEndpointAddress & 0x80) == USB_ENDPOINT_IN)
		{
			return ep;
		}
	}
	return NULL;
}

// The report descriptor's length, from the HID descriptor among the
// interface's class-specific descriptors
static u16 reportDescriptorLength(const usb_interfacedesc *inter)
{
	const u8 *d = inter->extra;
	u16 left = inter->extra_size;

	while (d != NULL && left >= 2 && d[0] >= 2 && d[0] <= left)
	{
		if (d[1] == USB_DT_HID && d[0] >= 9 && d[6] == USB_DT_REPORT)
		{
			return (u16)(d[7] | (d[8] << 8));
		}
		left -= d[0];
		d += d[0];
	}
	return 0;
}

// Reads and parses the interface's report descriptor into a newly allocated
// tabletdesc_pen. The parse table is only needed for this, and freed again.
static tabletdesc_pen *readPen(s32 fd, const usb_interfacedesc *inter)
{
	u16 len = reportDescriptorLength(inter);
	if (len == 0 || len > TABLET_RDESC_MAX)
	{
		len = TABLET_RDESC_MAX;
	}

	u8 *rdesc = (u8 *) malloc(len);
	tabletdesc_table *desc = (tabletdesc_table *) malloc(sizeof(tabletdesc_table));
	tabletdesc_pen *found = (tabletdesc_pen *) malloc(sizeof(tabletdesc_pen));

	if (rdesc == NULL || desc == NULL || found == NULL ||
			USB_GetGenericDescriptor(fd, USB_DT_REPORT, 0, inter->bInterfaceNumber, rdesc, len) < 0 ||
			!TabletDesc_Parse(rdesc, len, desc) || !TabletDesc_FindPen(desc, found))
	{
		free(found);
		found = NULL;
	}

	free(desc);
	free(rdesc);
	return found;
}

// Takes the tablet out of the mouse mode some power up in, where its
// descriptor has an Input Mode feature for it
static void setTabletMode(s32 fd, u8 interface)
{
	u32 len = pen->input_mode_bytes;
	if (pen->input_mode.bit_size == 0 || len == 0 || len > TABLET_REPORT_MAX)
	{
		return;
	}

	u8 ATTRIBUTE_ALIGN(32) report[TABLET_REPORT_MAX];
	u32 skip = pen->uses_ids ? 1 : 0;
	memset(report, 0, sizeof(report));
	report[0] = pen->input_mode.report_id;
	TabletDesc_Put(&pen->input_mode, report + skip, len - skip, 2);
	USB_WriteCtrlMsg(fd, USB_REQTYPE_INTERFACE_SET, HID_REQ_SET_REPORT, (HID_REPORT_FEATURE << 8) | pen->input_mode.report_id, interface, len, report);
}

static UsbAttachResult attach(const usb_device_entry *dev_entry, u8 dev_count)
{
	bool replug = false;

	// What a tablet removed since the last ScanPads() left behind
	if (pen != NULL)
	{
		free(pen);
		pen = NULL;
	}

	// A tablet is listed once per interface (pen, keys, mouse...), each opened on its own
	for (int i = 0; i < dev_count; ++i)
	{
		if (!isTabletVendor(dev_entry[i].vid) || isRejected(dev_entry[i].device_id))
		{
			continue;
		}
		s32 fd;
		usb_devdesc devdesc;
		UsbOpenResult opened = UsbInput_OpenDevice(&dev_entry[i], &fd, &devdesc);
		replug |= (opened == USBINPUT_OPEN_NEEDS_REPLUG);
		if (opened != USBINPUT_OPEN_OK)
		{
			continue;
		}

		const usb_interfacedesc *inter = NULL;
		const usb_endpointdesc *ep = NULL;
		bool boot = false;
		if (devdesc.configurations != NULL && devdesc.configurations[0].interfaces != NULL)
		{
			const usb_configurationdesc *config = &devdesc.configurations[0];
			for (int f = 0; f < config->bNumInterfaces && ep == NULL; ++f)
			{
				inter = &config->interfaces[f];
				boot |= (inter->bInterfaceSubClass == USB_SUBCLASS_BOOT);
				ep = findPenEndpoint(inter);
			}
		}

		tabletdesc_pen *found = ep ? readPen(fd, inter) : NULL;
		u8 interface = found ? inter->bInterfaceNumber : 0;
		if (found)
		{
			reader.size = ep->wMaxPacketSize > TABLET_REPORT_MAX ? TABLET_REPORT_MAX : ep->wMaxPacketSize;
			UsbReader_Bind(&reader, fd, ep->bEndpointAddress);
		}
		USB_FreeDescriptors(&devdesc);

		if (!found)
		{
			reject(dev_entry[i].device_id);
			// Closing suspends the interface; a boot mouse or keyboard one may be libogc's
			if (!boot)
			{
				USB_CloseDevice(&fd);
			}
			continue;
		}

		pen = found;
		maxX = pen->x.logical_max - pen->x.logical_min;
		maxY = pen->y.logical_max - pen->y.logical_min;
		if (maxX <= 0) maxX = 1;
		if (maxY <= 0) maxY = 1;

		deviceId = fd;
		resetState();
		setTabletMode(fd, interface);
		USB_DeviceRemovalNotifyAsync(fd, &removal_cb, (void*) fd);
		break;
	}

	return UsbInput_AttachResult(deviceId != 0, replug);
}

static bool matches(const usb_device_entry *dev)
{
	return isTabletVendor(dev->vid) && !isRejected(dev->device_id);
}

static bool isAttached(void)
{
	return deviceId != 0;
}

const UsbInputDriver Tablet_UsbDriver = { "Tablet", &matches, &attach, &isAttached, NULL };

void Tablet_ScanPads(void)
{
	if (deviceId == 0)
	{
		// The removal callback cannot free: it runs in the IPC interrupt handler
		if (pen != NULL)
		{
			free(pen);
			pen = NULL;
		}
		return;
	}

	UsbReader_Start(&reader);

	UsbInputButtons_Latch(&padButtons, reportButtons);

	inRange = reportInRange;
	if (inRange)
	{
		s32 x = reportX, y = reportY;
		posX = (x < 0 ? 0 : x > maxX ? maxX : x) / (float)maxX;
		posY = (y < 0 ? 0 : y > maxY ? maxY : y) / (float)maxY;
	}
}

bool Tablet_Connected(void)
{
	return deviceId != 0;
}

bool Tablet_InRange(void)
{
	return deviceId != 0 && inRange;
}

float Tablet_X(void)
{
	return posX;
}

float Tablet_Y(void)
{
	return posY;
}

u32 Tablet_ButtonsHeld(void)
{
	return padButtons.held;
}

u32 Tablet_ButtonsDown(void)
{
	return padButtons.down;
}

u32 Tablet_ButtonsUp(void)
{
	return padButtons.up;
}

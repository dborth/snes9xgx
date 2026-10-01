#ifdef HW_RVL
#include <gccore.h>
#include <ogc/usb.h>
#include "ds4.h"
#include "usbinput.h"

#define DS4_VID 0x054C
#define DS4_V1_PID 0x05C4
#define DS4_V2_PID 0x09CC

#define DS4_INPUT_REPORT_ID 0x01
#define DS4_INPUT_REPORT_MIN_SIZE 10
#define DS4_OUTPUT_REPORT_ID 0x05
#define DS4_OUTPUT_REPORT_SIZE 32
#define DS4_OUTPUT_FLAGS_RUMBLE_LIGHTBAR 0x07

#define DS4_STICK_CENTER 0x80
#define DS4_HAT_NEUTRAL 8
#define DS4_MAX_PLAYERS 4
#define DS4_PLAYER_SWITCH_HOLD_FRAMES 60

static bool replugRequired = false;
static s32 deviceId = 0;
static u8 endpointIn = 0;
static u8 endpointOut = 0;
static u8 ATTRIBUTE_ALIGN(32) buf[64];
static bool isReading = false;
static u8 player = 0;
static u32 touchpadHeldFrames = 0;

static volatile u32 reportButtons = 0;
static volatile u8 reportSticks[4] = { DS4_STICK_CENTER, DS4_STICK_CENTER, DS4_STICK_CENTER, DS4_STICK_CENTER };

static u32 held = 0;
static u32 down = 0;
static u32 up = 0;
static s16 sticks[4] = { 0, 0, 0, 0 };

static const u32 hatDirections[DS4_HAT_NEUTRAL] = {
	DS4_BUTTON_UP,
	DS4_BUTTON_UP | DS4_BUTTON_RIGHT,
	DS4_BUTTON_RIGHT,
	DS4_BUTTON_DOWN | DS4_BUTTON_RIGHT,
	DS4_BUTTON_DOWN,
	DS4_BUTTON_DOWN | DS4_BUTTON_LEFT,
	DS4_BUTTON_LEFT,
	DS4_BUTTON_UP | DS4_BUTTON_LEFT
};

static const u8 lightbarColors[DS4_MAX_PLAYERS][3] = {
	{ 0x00, 0x00, 0x40 },
	{ 0x40, 0x00, 0x00 },
	{ 0x00, 0x40, 0x00 },
	{ 0x20, 0x00, 0x20 }
};

static bool isDS4(usb_device_entry dev)
{
	return dev.vid == DS4_VID && (dev.pid == DS4_V1_PID || dev.pid == DS4_V2_PID);
}

static bool findEndpoints(const usb_devdesc *devdesc)
{
	endpointIn = 0;
	endpointOut = 0;

	if (devdesc->configurations == NULL)
	{
		return false;
	}

	const usb_configurationdesc *config = &devdesc->configurations[0];
	if (config->interfaces == NULL)
	{
		return false;
	}

	for (int i = 0; i < config->bNumInterfaces; ++i)
	{
		const usb_interfacedesc *inter = &config->interfaces[i];
		if (inter->bInterfaceClass != USB_CLASS_HID || inter->endpoints == NULL)
		{
			continue;
		}

		for (int e = 0; e < inter->bNumEndpoints; ++e)
		{
			const usb_endpointdesc *ep = &inter->endpoints[e];
			if ((ep->bmAttributes & 0x03) != USB_ENDPOINT_INTERRUPT)
			{
				continue;
			}
			if ((ep->bEndpointAddress & 0x80) == USB_ENDPOINT_IN)
			{
				endpointIn = ep->bEndpointAddress;
			}
			else
			{
				endpointOut = ep->bEndpointAddress;
			}
		}

		if (endpointIn != 0)
		{
			return true;
		}
	}

	return false;
}

static u32 parseButtons(const u8 *report)
{
	u32 buttons = 0;

	u8 hat = report[5] & 0x0F;
	if (hat < DS4_HAT_NEUTRAL)
	{
		buttons |= hatDirections[hat];
	}

	buttons |= (report[5] & 0x10) ? DS4_BUTTON_SQUARE   : 0;
	buttons |= (report[5] & 0x20) ? DS4_BUTTON_CROSS    : 0;
	buttons |= (report[5] & 0x40) ? DS4_BUTTON_CIRCLE   : 0;
	buttons |= (report[5] & 0x80) ? DS4_BUTTON_TRIANGLE : 0;

	buttons |= (report[6] & 0x01) ? DS4_BUTTON_L1      : 0;
	buttons |= (report[6] & 0x02) ? DS4_BUTTON_R1      : 0;
	buttons |= (report[6] & 0x04) ? DS4_BUTTON_L2      : 0;
	buttons |= (report[6] & 0x08) ? DS4_BUTTON_R2      : 0;
	buttons |= (report[6] & 0x10) ? DS4_BUTTON_SHARE   : 0;
	buttons |= (report[6] & 0x20) ? DS4_BUTTON_OPTIONS : 0;
	buttons |= (report[6] & 0x40) ? DS4_BUTTON_L3      : 0;
	buttons |= (report[6] & 0x80) ? DS4_BUTTON_R3      : 0;

	buttons |= (report[7] & 0x01) ? DS4_BUTTON_PS       : 0;
	buttons |= (report[7] & 0x02) ? DS4_BUTTON_TOUCHPAD : 0;

	return buttons;
}

static int read();

static int read_cb(int res, void *usrdata)
{
	if (!isReading)
	{
		return 1;
	}

	if (res < 0)
	{
		isReading = false;
		return 1;
	}

	if (res >= DS4_INPUT_REPORT_MIN_SIZE && buf[0] == DS4_INPUT_REPORT_ID)
	{
		for (int i = 0; i < 4; ++i)
		{
			reportSticks[i] = buf[1 + i];
		}
		reportButtons = parseButtons(buf);
	}

	if (read() < 0)
	{
		isReading = false;
	}

	return 1;
}

static int read()
{
	return USB_ReadIntrMsgAsync(deviceId, endpointIn, sizeof(buf), buf, &read_cb, NULL);
}

static void startReading()
{
	if (isReading)
	{
		return;
	}
	isReading = true;
	if (read() < 0)
	{
		isReading = false;
	}
}

static void stopReading()
{
	isReading = false;
}

static void resetState()
{
	reportButtons = 0;
	for (int i = 0; i < 4; ++i)
	{
		reportSticks[i] = DS4_STICK_CENTER;
		sticks[i] = 0;
	}
	held = down = up = 0;
	touchpadHeldFrames = 0;
}

static void updateLightbar()
{
	if (endpointOut == 0)
	{
		return;
	}

	u8 ATTRIBUTE_ALIGN(32) report[DS4_OUTPUT_REPORT_SIZE] = { 0 };
	report[0] = DS4_OUTPUT_REPORT_ID;
	report[1] = DS4_OUTPUT_FLAGS_RUMBLE_LIGHTBAR;
	report[6] = lightbarColors[player][0];
	report[7] = lightbarColors[player][1];
	report[8] = lightbarColors[player][2];
	USB_WriteIntrMsg(deviceId, endpointOut, sizeof(report), report);
}

static void increasePlayer()
{
	player = (player + 1) % DS4_MAX_PLAYERS;
	updateLightbar();
}

static int removal_cb(int result, void *usrdata)
{
	s32 fd = (s32) usrdata;
	if (fd == deviceId)
	{
		stopReading();
		deviceId = 0;
		UsbInput_DeferClose(fd);
		resetState();
		UsbInput_Rescan();
	}
	return 1;
}

static void attach(const usb_device_entry *dev_entry, u8 dev_count)
{
	if (deviceId != 0)
	{
		return;
	}

	for (int i = 0; i < dev_count; ++i)
	{
		if (!isDS4(dev_entry[i]))
		{
			continue;
		}
		s32 fd;
		if (USB_OpenDevice(dev_entry[i].device_id, dev_entry[i].vid, dev_entry[i].pid, &fd) < 0)
		{
			continue;
		}

		usb_devdesc devdesc;
		if (USB_GetDescriptors(fd, &devdesc) < 0)
		{
			replugRequired = true;
			USB_CloseDevice(&fd);
			break;
		}

		bool hasEndpoints = findEndpoints(&devdesc);
		USB_FreeDescriptors(&devdesc);

		if (!hasEndpoints)
		{
			USB_CloseDevice(&fd);
			continue;
		}

		deviceId = fd;
		replugRequired = false;
		resetState();
		updateLightbar();
		USB_DeviceRemovalNotifyAsync(fd, &removal_cb, (void*) fd);
		break;
	}
}

static bool matches(const usb_device_entry *dev)
{
	return isDS4(*dev);
}

static bool isAttached(void)
{
	return deviceId != 0;
}

const UsbInputDriver DS4_UsbDriver = { "DS4", &matches, &attach, &isAttached };

void DS4_ScanPads()
{
	if (deviceId == 0)
	{
		return;
	}

	startReading();

	u32 buttons = reportButtons;
	down = buttons & ~held;
	up = held & ~buttons;
	held = buttons;

	sticks[0] = (s16)reportSticks[0] - DS4_STICK_CENTER;
	sticks[1] = DS4_STICK_CENTER - (s16)reportSticks[1];
	sticks[2] = (s16)reportSticks[2] - DS4_STICK_CENTER;
	sticks[3] = DS4_STICK_CENTER - (s16)reportSticks[3];

	if ((held & DS4_BUTTON_TOUCHPAD) == 0)
	{
		touchpadHeldFrames = 0;
	}
	else if (++touchpadHeldFrames == DS4_PLAYER_SWITCH_HOLD_FRAMES)
	{
		increasePlayer();
	}
}

bool DS4_Connected(int chan)
{
	return deviceId != 0 && chan == player;
}

u32 DS4_ButtonsHeld()
{
	return held;
}

u32 DS4_ButtonsDown()
{
	return down;
}

u32 DS4_ButtonsUp()
{
	return up;
}

s16 DS4_lStickX()
{
	return sticks[0];
}

s16 DS4_lStickY()
{
	return sticks[1];
}

s16 DS4_rStickX()
{
	return sticks[2];
}

s16 DS4_rStickY()
{
	return sticks[3];
}

char* DS4_Status()
{
	if (replugRequired)
		return "please replug";
	return deviceId ? "connected" : "not found";
}

#endif

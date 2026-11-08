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

static s32 deviceId = 0;
static u8 endpointOut = 0;
static u8 player = 0;
static u32 touchpadHeldFrames = 0;

static volatile u32 reportButtons = 0;
static volatile u8 reportSticks[4] = { DS4_STICK_CENTER, DS4_STICK_CENTER, DS4_STICK_CENTER, DS4_STICK_CENTER };

static UsbInputButtons padButtons;
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

static bool findEndpoints(const usb_devdesc *devdesc, u8 *endpointIn)
{
	*endpointIn = 0;
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
				*endpointIn = ep->bEndpointAddress;
			}
			else
			{
				endpointOut = ep->bEndpointAddress;
			}
		}

		if (*endpointIn != 0)
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

static void onReport(const u8 *report, s32 length)
{
	if (length >= DS4_INPUT_REPORT_MIN_SIZE && report[0] == DS4_INPUT_REPORT_ID)
	{
		for (int i = 0; i < 4; ++i)
		{
			reportSticks[i] = report[1 + i];
		}
		reportButtons = parseButtons(report);
	}
}

static u8 ATTRIBUTE_ALIGN(32) buf[64];
static UsbReader reader = { .buf = buf, .size = sizeof(buf), .onReport = &onReport };

static void resetState()
{
	reportButtons = 0;
	for (int i = 0; i < 4; ++i)
	{
		reportSticks[i] = DS4_STICK_CENTER;
		sticks[i] = 0;
	}
	UsbInputButtons_Clear(&padButtons);
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
		UsbReader_Stop(&reader);
		deviceId = 0;
		UsbInput_DeferClose(fd);
		resetState();
		UsbInput_Rescan();
	}
	return 1;
}

static UsbAttachResult attach(const usb_device_entry *dev_entry, u8 dev_count)
{
	bool replug = false;

	for (int i = 0; i < dev_count; ++i)
	{
		if (!isDS4(dev_entry[i]))
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

		u8 endpointIn;
		bool hasEndpoints = findEndpoints(&devdesc, &endpointIn);
		if (!hasEndpoints)
		{
			UsbInput_Discard(&fd, &devdesc);
			continue;
		}
		USB_FreeDescriptors(&devdesc);

		deviceId = fd;
		UsbReader_Bind(&reader, fd, endpointIn);
		resetState();
		updateLightbar();
		USB_DeviceRemovalNotifyAsync(fd, &removal_cb, (void*) fd);
		break;
	}

	return UsbInput_AttachResult(deviceId != 0, replug);
}

static bool matches(const usb_device_entry *dev)
{
	return isDS4(*dev);
}

static bool isAttached(void)
{
	return deviceId != 0;
}

const UsbInputDriver DS4_UsbDriver = { "DS4", &matches, &attach, &isAttached, NULL };

void DS4_ScanPads()
{
	if (deviceId == 0)
	{
		return;
	}

	UsbReader_Start(&reader);

	UsbInputButtons_Latch(&padButtons, reportButtons);

	sticks[0] = (s16)reportSticks[0] - DS4_STICK_CENTER;
	sticks[1] = DS4_STICK_CENTER - (s16)reportSticks[1];
	sticks[2] = (s16)reportSticks[2] - DS4_STICK_CENTER;
	sticks[3] = DS4_STICK_CENTER - (s16)reportSticks[3];

	if ((padButtons.held & DS4_BUTTON_TOUCHPAD) == 0)
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
	return padButtons.held;
}

u32 DS4_ButtonsDown()
{
	return padButtons.down;
}

u32 DS4_ButtonsUp()
{
	return padButtons.up;
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

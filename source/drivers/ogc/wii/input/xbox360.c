#ifdef HW_RVL
#include <gccore.h>
#include <ogc/usb.h>
#include "xbox360.h"
#include "usbinput.h"

#define USB_CLASS_XBOX360 0xFF
#define XBOX360_VID 0x045e
#define XBOX360_PID 0x028e
#define XBOX360_TRIGGER_THRESHOLD 128

static bool replugRequired = false;
static s32 deviceId = 0;
static u8 endpoint_in = 0x81;
static u8 endpoint_out = 0x01; // some controllers require 0x02 (updated below)
static u8 bMaxPacketSize = 20;
static u8 bConfigurationValue = 1;
static u8 ATTRIBUTE_ALIGN(32) buf[20];
static bool isReading = false;
static u8 player = 0;

// Written from the USB read callback, latched once per frame by XBOX360_ScanPads()
static volatile u32 reportButtons = 0;
static volatile s16 reportSticks[4] = { 0, 0, 0, 0 };  // raw [-32768, 32767]: lx, ly, rx, ry

static u32 held = 0;
static u32 down = 0;
static u32 up = 0;
static s16 sticks[4] = { 0, 0, 0, 0 };  // -128..127: lx, ly, rx, ry
static bool xboxButtonDown = false;
static bool nextPlayer = false;

static u8 getEndpoint(usb_devdesc devdesc)
{
	if (devdesc.configurations == NULL || devdesc.configurations->interfaces == NULL ||
			devdesc.configurations->interfaces->endpoints == NULL)
	{
		return -1;
	}
	return devdesc.configurations->interfaces->endpoints->bEndpointAddress;
}

static bool isXBOX360(usb_device_entry dev)
{
	return dev.vid == XBOX360_VID && dev.pid == XBOX360_PID;
}

static bool isXBOX360Gamepad(usb_devdesc devdesc)
{
	return devdesc.idVendor == XBOX360_VID && devdesc.idProduct == XBOX360_PID && getEndpoint(devdesc) == endpoint_in;
}

static int read(s32 device_id, u8 endpoint, u8 bMaxPacketSize0);

static void start_reading(s32 device_id, u8 endpoint, u8 bMaxPacketSize0)
{
	if (isReading)
	{
		// already reading
		return;
	}
	isReading = true;
	if (read(deviceId, endpoint_in, bMaxPacketSize0) < 0)
	{
		// no transfer is pending, allow the next scan to try again
		isReading = false;
	}
}

static void stop_reading()
{
	isReading = false;
}

static int read_cb(int res, void *usrdata)
{
	if (!isReading)
	{
		// stop reading
		return 1;
	}

	if (res < 0)
	{
		// transfer error: no transfer is pending any more, XBOX360_ScanPads() restarts reading
		isReading = false;
		return 1;
	}

	// NOTE: The four startup messages have res = 3 and can be ignored

	// Button layout
	// A=3,10
	// B=3,20
	// X=3,40
	// Y=3,80
	// Up=2,01
	// Right=2,08
	// Left=2,04
	// Down=2,02
	// L=3,01
	// R=3,02
	// L2=4,FF ; analog trigger
	// R3=5,FF ; analog trigger
	// L3=2,40 ; left hat button
	// L3=2,80 ; right hat button
	// XBOX=3,04
	// Start=2,10
	// Back=2,20
	// LStickX=6/7,FF ; <low byte>,<high byte>
	// LStickY=8/9,FF
	// RStickX=10/11,FF
	// RStickY=12/13,FF

	if (res == 20)
	{
		u32 b = 0;
		b |= (buf[2] & 0x01) ? XBOX360_BUTTON_UP    : 0;
		b |= (buf[2] & 0x02) ? XBOX360_BUTTON_DOWN  : 0;
		b |= (buf[2] & 0x04) ? XBOX360_BUTTON_LEFT  : 0;
		b |= (buf[2] & 0x08) ? XBOX360_BUTTON_RIGHT : 0;
		b |= (buf[2] & 0x10) ? XBOX360_BUTTON_START : 0;
		b |= (buf[2] & 0x20) ? XBOX360_BUTTON_BACK  : 0;
		b |= (buf[2] & 0x40) ? XBOX360_BUTTON_L3    : 0;
		b |= (buf[2] & 0x80) ? XBOX360_BUTTON_R3    : 0;

		b |= (buf[3] & 0x01) ? XBOX360_BUTTON_LB : 0;
		b |= (buf[3] & 0x02) ? XBOX360_BUTTON_RB : 0;
		b |= (buf[3] & 0x10) ? XBOX360_BUTTON_A  : 0;
		b |= (buf[3] & 0x20) ? XBOX360_BUTTON_B  : 0;
		b |= (buf[3] & 0x40) ? XBOX360_BUTTON_X  : 0;
		b |= (buf[3] & 0x80) ? XBOX360_BUTTON_Y  : 0;

		// analog triggers, reported as buttons past a threshold
		b |= (buf[4] > XBOX360_TRIGGER_THRESHOLD) ? XBOX360_BUTTON_LT : 0;
		b |= (buf[5] > XBOX360_TRIGGER_THRESHOLD) ? XBOX360_BUTTON_RT : 0;

		reportButtons = b;

		// sticks stay analog: the caller turns them into directions where it wants that
		reportSticks[0] = (int16_t)((buf[7]  << 8) | buf[6]);
		reportSticks[1] = (int16_t)((buf[9]  << 8) | buf[8]);
		reportSticks[2] = (int16_t)((buf[11] << 8) | buf[10]);
		reportSticks[3] = (int16_t)((buf[13] << 8) | buf[12]);

		// XBOX button to switch to next player, once per press (on the rising edge)
		bool guideDown = (buf[3] & 0x04) == 0x04;
		if (guideDown && !xboxButtonDown)
		{
			nextPlayer = true;
		}
		xboxButtonDown = guideDown;
	}

	// read again
	if (read(deviceId, endpoint_in, bMaxPacketSize) < 0)
	{
		isReading = false;
	}

	return 1;
}

// never call directly
static int read(s32 device_id, u8 endpoint, u8 bMaxPacketSize0)
{
	// need to use async, because USB_ReadIntrMsg() blocks until a button is pressed
	return USB_ReadIntrMsgAsync(device_id, endpoint, sizeof(buf), buf, &read_cb, NULL);
}

static void turnOnLED()
{
	uint8_t ATTRIBUTE_ALIGN(32) buf[] = { 0x01, 0x03, 0x06 + player };
	USB_WriteIntrMsg(deviceId, endpoint_out, sizeof(buf), buf);
}

static void increasePlayer()
{
	player++;
	if (player > 3)
	{
		player = 0;
	}
	turnOnLED();
}

void rumble(s32 device_id, u8 left, u8 right)
{
	uint8_t ATTRIBUTE_ALIGN(32) buf[] = { 0x00, 0x08, 0x00, left, right, 0x00, 0x00, 0x00 };
	USB_WriteIntrMsg(deviceId, endpoint_out, sizeof(buf), buf);
}

static int removal_cb(int result, void *usrdata)
{
	s32 fd = (s32) usrdata;
	if (fd == deviceId)
	{
		stop_reading();
		deviceId = 0;
		reportButtons = 0;
		for (int i = 0; i < 4; i++)
		{
			reportSticks[i] = 0;
			sticks[i] = 0;
		}
		held = down = up = 0;
		xboxButtonDown = false;
		nextPlayer = false;
		UsbInput_DeferClose(fd);
		UsbInput_Rescan();
	}
	return 1;
}

// adapted from RetroArch input/drivers_hid/wiiusb_hid.c#wiiusb_get_description()
void wiiusb_get_description(const usb_device_entry *device, usb_devdesc *devdesc)
{
   unsigned char c;
   unsigned i, k;

   for (c = 0; c < devdesc->bNumConfigurations; c++)
   {
	  const usb_configurationdesc *config = &devdesc->configurations[c];

	  for (i = 0; i < (int)config->bNumInterfaces; i++)
	  {
		 const usb_interfacedesc *inter = &config->interfaces[i];

		 for (k = 0; k < (int)inter->bNumEndpoints; k++)
		 {
			const usb_endpointdesc *epdesc = &inter->endpoints[k];
			bool is_int = (epdesc->bmAttributes & 0x03)	 == USB_ENDPOINT_INTERRUPT;
			bool is_out = (epdesc->bEndpointAddress & 0x80) == USB_ENDPOINT_OUT;
			bool is_in  = (epdesc->bEndpointAddress & 0x80) == USB_ENDPOINT_IN;

			if (is_int)
			{
			   if (is_in)
			   {
				  //endpoint_in = epdesc->bEndpointAddress;
				  //endpoint_in_max_size = epdesc->wMaxPacketSize;
			   }
			   if (is_out)
			   {
				  endpoint_out = epdesc->bEndpointAddress;
				  //endpoint_out_max_size = epdesc->wMaxPacketSize;
			   }
			}
		 }
		 break;
	  }
   }
}

static void attach(const usb_device_entry *dev_entry, u8 dev_count)
{
	if (deviceId != 0)
	{
		return;
	}

	// a replug request only stands while a controller that needs it is plugged in
	replugRequired = false;

	for (int i = 0; i < dev_count; ++i)
	{
		if (!isXBOX360(dev_entry[i]))
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
			// You have to replug the XBOX360 controller!
			replugRequired = true;
			USB_CloseDevice(&fd);
			continue;
		}

		if (isXBOX360Gamepad(devdesc) && USB_SetConfiguration(fd, bConfigurationValue) >= 0)
		{
			deviceId = fd;
			replugRequired = false;
			wiiusb_get_description(&dev_entry[i], &devdesc);
			USB_FreeDescriptors(&devdesc);
			turnOnLED();
			USB_DeviceRemovalNotifyAsync(fd, &removal_cb, (void*) fd);
			break;
		}
		else
		{
			USB_FreeDescriptors(&devdesc);
			USB_CloseDevice(&fd);
		}
	}
}

static bool matches(const usb_device_entry *dev)
{
	return isXBOX360(*dev);
}

static bool isAttached(void)
{
	return deviceId != 0;
}

const UsbInputDriver XBOX360_UsbDriver = { "XBOX360", &matches, &attach, &isAttached };

void XBOX360_ScanPads(void)
{
	if (deviceId == 0)
	{
		return;
	}

	// player switching is applied here, once per frame, not from ButtonsHeld()
	// (which is called once per channel and would change player mid-frame)
	if (nextPlayer)
	{
		nextPlayer = false;
		increasePlayer();
	}

	start_reading(deviceId, endpoint_in, bMaxPacketSize);

	u32 buttons = reportButtons;
	down = buttons & ~held;
	up = held & ~buttons;
	held = buttons;

	// raw 16-bit to -128..127, the same range as the other pads
	for (int i = 0; i < 4; i++)
	{
		sticks[i] = reportSticks[i] >> 8;
	}
}

u32 XBOX360_ButtonsHeld(int chan)
{
	return XBOX360_Connected(chan) ? held : 0;
}

u32 XBOX360_ButtonsDown(int chan)
{
	return XBOX360_Connected(chan) ? down : 0;
}

u32 XBOX360_ButtonsUp(int chan)
{
	return XBOX360_Connected(chan) ? up : 0;
}

s16 XBOX360_lStickX(int chan) { return XBOX360_Connected(chan) ? sticks[0] : 0; }
s16 XBOX360_lStickY(int chan) { return XBOX360_Connected(chan) ? sticks[1] : 0; }
s16 XBOX360_rStickX(int chan) { return XBOX360_Connected(chan) ? sticks[2] : 0; }
s16 XBOX360_rStickY(int chan) { return XBOX360_Connected(chan) ? sticks[3] : 0; }

bool XBOX360_Connected(int chan)
{
	return deviceId != 0 && chan == player;
}

char* XBOX360_Status(void)
{
	if (replugRequired)
		return "please replug";
	return deviceId ? "connected" : "not found";
}

#endif

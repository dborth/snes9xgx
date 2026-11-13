#include <gccore.h>
#include <ogc/usb.h>
#include "xbox360.h"
#include "usbinput.h"

#define USB_CLASS_XBOX360 0xFF
#define XBOX360_VID 0x045e
#define XBOX360_PID 0x028e
#define XBOX360_TRIGGER_THRESHOLD 128

static s32 deviceId = 0;
static u8 endpoint_in = 0x81;
static u8 endpoint_out = 0x01; // some controllers require 0x02 (updated below)
static u8 bConfigurationValue = 1;
static u8 player = 0;

// Written from the USB read callback, latched once per frame by XBOX360_ScanPads()
static volatile u32 reportButtons = 0;
static volatile s16 reportSticks[4] = { 0, 0, 0, 0 };  // raw [-32768, 32767]: lx, ly, rx, ry

static UsbInputButtons padButtons;
static s16 sticks[4] = { 0, 0, 0, 0 };  // -128..127: lx, ly, rx, ry
static bool xboxButtonDown = false;
static bool nextPlayer = false;

static bool isXBOX360(usb_device_entry dev)
{
	return dev.vid == XBOX360_VID && dev.pid == XBOX360_PID;
}

static bool isXBOX360Gamepad(usb_devdesc devdesc)
{
	return devdesc.idVendor == XBOX360_VID && devdesc.idProduct == XBOX360_PID && UsbInput_FirstEndpoint(&devdesc) == endpoint_in;
}

static void onReport(const u8 *report, s32 length)
{
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

	if (length == 20)
	{
		u32 b = 0;
		b |= (report[2] & 0x01) ? XBOX360_BUTTON_UP    : 0;
		b |= (report[2] & 0x02) ? XBOX360_BUTTON_DOWN  : 0;
		b |= (report[2] & 0x04) ? XBOX360_BUTTON_LEFT  : 0;
		b |= (report[2] & 0x08) ? XBOX360_BUTTON_RIGHT : 0;
		b |= (report[2] & 0x10) ? XBOX360_BUTTON_START : 0;
		b |= (report[2] & 0x20) ? XBOX360_BUTTON_BACK  : 0;
		b |= (report[2] & 0x40) ? XBOX360_BUTTON_L3    : 0;
		b |= (report[2] & 0x80) ? XBOX360_BUTTON_R3    : 0;

		b |= (report[3] & 0x01) ? XBOX360_BUTTON_LB : 0;
		b |= (report[3] & 0x02) ? XBOX360_BUTTON_RB : 0;
		b |= (report[3] & 0x10) ? XBOX360_BUTTON_A  : 0;
		b |= (report[3] & 0x20) ? XBOX360_BUTTON_B  : 0;
		b |= (report[3] & 0x40) ? XBOX360_BUTTON_X  : 0;
		b |= (report[3] & 0x80) ? XBOX360_BUTTON_Y  : 0;

		// analog triggers, reported as buttons past a threshold
		b |= (report[4] > XBOX360_TRIGGER_THRESHOLD) ? XBOX360_BUTTON_LT : 0;
		b |= (report[5] > XBOX360_TRIGGER_THRESHOLD) ? XBOX360_BUTTON_RT : 0;

		reportButtons = b;

		// sticks stay analog: the caller turns them into directions where it wants that
		reportSticks[0] = (int16_t)((report[7]  << 8) | report[6]);
		reportSticks[1] = (int16_t)((report[9]  << 8) | report[8]);
		reportSticks[2] = (int16_t)((report[11] << 8) | report[10]);
		reportSticks[3] = (int16_t)((report[13] << 8) | report[12]);

		// XBOX button to switch to next player, once per press (on the rising edge)
		bool guideDown = (report[3] & 0x04) == 0x04;
		if (guideDown && !xboxButtonDown)
		{
			nextPlayer = true;
		}
		xboxButtonDown = guideDown;
	}
}

static u8 ATTRIBUTE_ALIGN(32) buf[32];
static UsbReader reader = { .buf = buf, .size = sizeof(buf), .onReport = &onReport };

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

void rumble(s32, u8 left, u8 right)
{
	uint8_t ATTRIBUTE_ALIGN(32) buf[] = { 0x00, 0x08, 0x00, left, right, 0x00, 0x00, 0x00 };
	USB_WriteIntrMsg(deviceId, endpoint_out, sizeof(buf), buf);
}

static int removal_cb(int, void *usrdata)
{
	s32 fd = (s32) usrdata;
	if (fd == deviceId)
	{
		UsbReader_Stop(&reader);
		deviceId = 0;
		reportButtons = 0;
		for (int i = 0; i < 4; i++)
		{
			reportSticks[i] = 0;
			sticks[i] = 0;
		}
		UsbInputButtons_Clear(&padButtons);
		xboxButtonDown = false;
		nextPlayer = false;
		UsbInput_DeferClose(fd);
		UsbInput_Rescan();
	}
	return 1;
}

// adapted from RetroArch input/drivers_hid/wiiusb_hid.c#wiiusb_get_description()
void wiiusb_get_description(const usb_device_entry *, usb_devdesc *devdesc)
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

static UsbAttachResult attach(const usb_device_entry *dev_entry, u8 dev_count)
{
	bool replug = false;

	for (int i = 0; i < dev_count; ++i)
	{
		if (!isXBOX360(dev_entry[i]))
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

		if (!isXBOX360Gamepad(devdesc) || USB_SetConfiguration(fd, bConfigurationValue) < 0)
		{
			UsbInput_Discard(&fd, &devdesc);
			continue;
		}

		deviceId = fd;
		wiiusb_get_description(&dev_entry[i], &devdesc);
		USB_FreeDescriptors(&devdesc);
		UsbReader_Bind(&reader, fd, endpoint_in);
		turnOnLED();
		USB_DeviceRemovalNotifyAsync(fd, &removal_cb, (void*) fd);
		break;
	}

	return UsbInput_AttachResult(deviceId != 0, replug);
}

static bool matches(const usb_device_entry *dev)
{
	return isXBOX360(*dev);
}

static bool isAttached(void)
{
	return deviceId != 0;
}

const UsbInputDriver XBOX360_UsbDriver = { "XBOX360", &matches, &attach, &isAttached, NULL };

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

	UsbReader_Start(&reader);

	UsbInputButtons_Latch(&padButtons, reportButtons);

	// raw 16-bit to -128..127, the same range as the other pads
	for (int i = 0; i < 4; i++)
	{
		sticks[i] = reportSticks[i] >> 8;
	}
}

u32 XBOX360_ButtonsHeld(int chan)
{
	return XBOX360_Connected(chan) ? padButtons.held : 0;
}

u32 XBOX360_ButtonsDown(int chan)
{
	return XBOX360_Connected(chan) ? padButtons.down : 0;
}

u32 XBOX360_ButtonsUp(int chan)
{
	return XBOX360_Connected(chan) ? padButtons.up : 0;
}

s16 XBOX360_lStickX(int chan) { return XBOX360_Connected(chan) ? sticks[0] : 0; }
s16 XBOX360_lStickY(int chan) { return XBOX360_Connected(chan) ? sticks[1] : 0; }
s16 XBOX360_rStickX(int chan) { return XBOX360_Connected(chan) ? sticks[2] : 0; }
s16 XBOX360_rStickY(int chan) { return XBOX360_Connected(chan) ? sticks[3] : 0; }

bool XBOX360_Connected(int chan)
{
	return deviceId != 0 && chan == player;
}

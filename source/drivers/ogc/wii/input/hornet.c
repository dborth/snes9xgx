#include <gccore.h>
#include "hornet.h"
#include "usbinput.h"

#define HORNET_VID 0x0079
#define HORNET_PID 0x0011

static s32 deviceId = 0;
static u8 endpoint = 0;
static u8 bMaxPacketSize = 0;
static u32 jp;

static bool isHornetGamepad(usb_device_entry dev)
{
	return dev.vid == HORNET_VID && dev.pid == HORNET_PID;
}

static int removal_cb(int, void *usrdata)
{
	s32 fd = (s32) usrdata;
	if (fd == deviceId)
	{
		deviceId = 0;
		jp = 0;
		UsbInput_DeferClose(fd);
		UsbInput_Rescan();
	}
	return 1;
}

static UsbAttachResult attach(const usb_device_entry *dev_entry, u8 dev_count)
{
	bool replug = false;

	for (int i = 0; i < dev_count; ++i)
	{
		if (!isHornetGamepad(dev_entry[i]))
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

		deviceId = fd;
		endpoint = UsbInput_FirstEndpoint(&devdesc);
		bMaxPacketSize = devdesc.bMaxPacketSize0;
		USB_FreeDescriptors(&devdesc);
		USB_DeviceRemovalNotifyAsync(fd, &removal_cb, (void*) fd);
		break;
	}

	return UsbInput_AttachResult(deviceId != 0, replug);
}

static bool matches(const usb_device_entry *dev)
{
	return isHornetGamepad(*dev);
}

static bool isAttached(void)
{
	return deviceId != 0;
}

const UsbInputDriver Hornet_UsbDriver = { "Hornet", &matches, &attach, &isAttached, NULL };

void Hornet_ScanPads(void)
{
	if (deviceId == 0)
	{
		return;
	}

	uint8_t ATTRIBUTE_ALIGN(32) buf[bMaxPacketSize];
	s32 res = USB_ReadIntrMsg(deviceId, endpoint, sizeof(buf), buf);
	if (res < 0)
	{
		return;
	}

	// Button layout
	// A=5,2F
	// B=5,1F
	// X=6,01
	// Y=5,8F
	// Select=6,10 ; Hornet button label = "9"
	// Start=6,20  ; Hornet button label = "10"
	// Up=4,00
	// Right=3,FF
	// Left=3,00
	// Down=4,FF
	// L=6,04
	// R=6,08
	// Unused=6,02 ; Hornet button label = "6"
	// Unused=5,4F ; Hornet button label = "3"

	jp = 0;
	jp |= (buf[4] == 0x00) ? PAD_BUTTON_UP	: 0;
	jp |= (buf[4] == 0xFF) ? PAD_BUTTON_DOWN  : 0;
	jp |= (buf[3] == 0x00) ? PAD_BUTTON_LEFT  : 0;
	jp |= (buf[3] == 0xFF) ? PAD_BUTTON_RIGHT : 0;

	jp |= ((buf[5] & 0x2F) == 0x2F) ? PAD_BUTTON_A : 0;
	jp |= ((buf[5] & 0x1F) == 0x1F) ? PAD_BUTTON_B : 0;
	jp |= ((buf[6] & 0x01) == 0x01) ? PAD_BUTTON_X : 0;
	jp |= ((buf[5] & 0x8F) == 0x8F) ? PAD_BUTTON_Y : 0;

	jp |= ((buf[6] & 0x04) == 0x04) ? PAD_TRIGGER_L : 0;
	jp |= ((buf[6] & 0x08) == 0x08) ? PAD_TRIGGER_R : 0;

	jp |= ((buf[6] & 0x20) == 0x20) ? PAD_BUTTON_START : 0;
	jp |= ((buf[6] & 0x10) == 0x10) ? PAD_TRIGGER_Z	: 0; // Hornet select button maps to Z

	jp |= ((buf[6] & 0x02) == 0x02) ? PAD_BUTTON_Y : 0; // Hornet button 6 maps to Y
	jp |= ((buf[5] & 0x4F) == 0x4F) ? PAD_BUTTON_B : 0; // Hornet button 3 maps to B
}

u32 Hornet_ButtonsHeld(int chan)
{
	if (deviceId == 0)
	{
		return 0;
	}
	if (chan != 0)
	{
		return 0;
	}
	return jp;
}

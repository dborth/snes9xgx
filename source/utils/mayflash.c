#ifdef HW_RVL
#include <gccore.h>

#define MAYFLASH_VID 0x0E8F
#define MAYFLASH_PID 0x3013	

static bool setup = false;
static bool replugRequired = false;
static s32 deviceId = 0;
static u8 endpoint = 0;
static u8 bMaxPacketSize = 0;
static u32 jpMayflash[2];

static bool isMayflashGamepad(usb_device_entry dev)
{
	return dev.vid == MAYFLASH_VID && dev.pid == MAYFLASH_PID;
}

static u8 getEndpoint(usb_devdesc devdesc)
{
	if (devdesc.configurations == NULL || devdesc.configurations->interfaces == NULL ||
			devdesc.configurations->interfaces->endpoints == NULL)
	{
		return -1;
	}
	return devdesc.configurations->interfaces->endpoints->bEndpointAddress;
}

static int removal_cb(int result, void *usrdata)
{
	s32 fd = (s32) usrdata;
	if (fd == deviceId)
	{
		deviceId = 0;
	}
	return 1;
}

static void open()
{
	if (deviceId != 0)
	{
		return;
	}

	usb_device_entry dev_entry[8];
	u8 dev_count;
	if (USB_GetDeviceList(dev_entry, 8, USB_CLASS_HID, &dev_count) < 0)
	{
		return;
	}

	for (int i = 0; i < dev_count; ++i)
	{
		if (!isMayflashGamepad(dev_entry[i]))
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
			// You have to replug the controller!
			replugRequired = true;
			USB_CloseDevice(&fd);
			break;
		}

		deviceId = fd;
		replugRequired = false;
		endpoint = getEndpoint(devdesc);
		bMaxPacketSize = devdesc.bMaxPacketSize0;
		USB_DeviceRemovalNotifyAsync(fd, &removal_cb, (void*) fd);
		break;
	}

	setup = true;
}

void Mayflash_ScanPads()
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

	// buf[0] contains the port returned
	// you have to make 2 calls to get the status, even if you're only interested in one port
	// because it is not sure which port is returned first

	// 1 = Right port
	// 2 = Left port

	// Button layout
	// A=5,2F
	// B=5,4F
	// X=5,1F
	// Y=5,8F
	// Select=6,10 
	// Start=6,20  
	// Up=4,00
	// Right=3,FF
	// Left=3,00
	// Down=4,FF
	// L=6,04
	// R=6,08

	u32 jp = 0;
	jp |= (buf[4] == 0x00) ? PAD_BUTTON_UP	: 0;
	jp |= (buf[4] == 0xFF) ? PAD_BUTTON_DOWN  : 0;
	jp |= (buf[3] == 0x00) ? PAD_BUTTON_LEFT  : 0;
	jp |= (buf[3] == 0xFF) ? PAD_BUTTON_RIGHT : 0;

	jp |= ((buf[5] & 0x2F) == 0x2F) ? PAD_BUTTON_A : 0;
	jp |= ((buf[5] & 0x4F) == 0x4F) ? PAD_BUTTON_B : 0;
	jp |= ((buf[5] & 0x1F) == 0x1F) ? PAD_BUTTON_X : 0;
	jp |= ((buf[5] & 0x8F) == 0x8F) ? PAD_BUTTON_Y : 0;

	jp |= ((buf[6] & 0x04) == 0x04) ? PAD_TRIGGER_L : 0;
	jp |= ((buf[6] & 0x08) == 0x08) ? PAD_TRIGGER_R : 0;

	jp |= ((buf[6] & 0x20) == 0x20) ? PAD_BUTTON_START : 0;
	jp |= ((buf[6] & 0x10) == 0x10) ? PAD_TRIGGER_Z	: 0; // SNES select button maps to Z

	// Required, otherwise if the returned port isn't the one we are looking for, jp will be set to zero,
	// and held buttons are not possible

	jpMayflash[buf[0] - 1] = jp;
}

u32 Mayflash_ButtonsHeld(int chan)
{
	if(!setup)
	{
		open();
	}
	if (deviceId == 0)
	{
		return 0;
	}
	return jpMayflash[chan];
}

char* Mayflash_Status()
{
	open();
	if (replugRequired)
		return "please replug";
	return deviceId ? "connected" : "not found";
}

#endif

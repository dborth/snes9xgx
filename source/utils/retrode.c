#ifdef HW_RVL
#include <gccore.h>

#define RETRODE_VID 0x0403
#define RETRODE_PID 0x97C1

static bool setup = false;
static bool replugRequired = false;
static s32 deviceId = 0;
static u8 endpoint = 0;
static u8 bMaxPacketSize = 0;

static u32 jpRetrode[4];

static bool isRetrode(usb_device_entry dev)
{
	return dev.vid == RETRODE_VID && dev.pid == RETRODE_PID;
}

static bool isRetrodeGamepad(usb_devdesc devdesc)
{
	if (devdesc.idVendor != RETRODE_VID || devdesc.idProduct != RETRODE_PID ||
		devdesc.configurations == NULL || devdesc.configurations->interfaces == NULL ||
		devdesc.configurations->interfaces->endpoints == NULL)
	{
		return false;
	}
	return devdesc.configurations->interfaces->bInterfaceSubClass == 0;
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

	// Retrode has two entries in USB_GetDeviceList(), one for gamepads and one for SNES mouse
	for (int i = 0; i < dev_count; ++i)
	{
		if (!isRetrode(dev_entry[i]))
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
			// You have to replug the Retrode controller!
			replugRequired = true;
			USB_CloseDevice(&fd);
			break;
		}

		if (isRetrodeGamepad(devdesc))
		{
			deviceId = fd;
			replugRequired = false;
			endpoint = getEndpoint(devdesc);
			bMaxPacketSize = devdesc.bMaxPacketSize0;
			USB_DeviceRemovalNotifyAsync(fd, &removal_cb, (void*) fd);
			break;
		}
		else
		{
			USB_CloseDevice(&fd);
		}
	}

	setup = true;
}

void Retrode_ScanPads()
{
	if (deviceId == 0)
	{
		return;
	}

	uint8_t ATTRIBUTE_ALIGN(32) buf[bMaxPacketSize];

	// Retrode gamepad endpoint returns 5 bytes with gamepad events
	if (USB_ReadIntrMsg(deviceId, endpoint, sizeof(buf), buf) != 5)
	{
		return;
	}

	// buf[0] contains the port returned
	// you have to make 4 calls to get the status, even if you are only interested in one port
	// because it is not sure which port is returned first
	// 1 = left SNES
	// 2 = right SNES
	// 3 = left Genesis/MD
	// 4 = right Genesis/MD

	// Button layout
	// A=3,10
	// B=3,01
	// X=3,20
	// Y=3,02
	// L=3,40
	// R=3,80
	// Up=2,9C
	// Down=2,64
	// Left=1,9C
	// Right=1,64
	// Start=3,08
	// Select=3,04

	u32 jp = 0;
	jp |= ((buf[2] & 0x9C) == 0x9C) ? PAD_BUTTON_UP	: 0;
	jp |= ((buf[2] & 0x64) == 0x64) ? PAD_BUTTON_DOWN  : 0;
	jp |= ((buf[1] & 0x9C) == 0x9C) ? PAD_BUTTON_LEFT  : 0;
	jp |= ((buf[1] & 0x64) == 0x64) ? PAD_BUTTON_RIGHT : 0;

	jp |= (buf[3] & 0x10) ? PAD_BUTTON_A : 0;
	jp |= (buf[3] & 0x01) ? PAD_BUTTON_B : 0;
	jp |= (buf[3] & 0x20) ? PAD_BUTTON_X : 0;
	jp |= (buf[3] & 0x02) ? PAD_BUTTON_Y : 0;

	jp |= (buf[3] & 0x40) ? PAD_TRIGGER_L : 0;
	jp |= (buf[3] & 0x80) ? PAD_TRIGGER_R : 0;

	jp |= (buf[3] & 0x08) ? PAD_BUTTON_START : 0;
	jp |= (buf[3] & 0x04) ? PAD_TRIGGER_Z	: 0; // SNES select button maps to Z

	// Required, otherwise if the returned port isn't the one we are looking for, jp will be set to zero,
	// and held buttons are not possible w/o saving the state.
	jpRetrode[buf[0] - 1] = jp;
}

u32 Retrode_ButtonsHeld(int chan)
{
	if(!setup)
	{
		open();
	}
	if (deviceId == 0)
	{
		return 0;
	}
	return jpRetrode[chan];
}

char* Retrode_Status()
{
	open();
	if (replugRequired)
		return "please replug";
	return deviceId ? "connected" : "not found";
}

#endif

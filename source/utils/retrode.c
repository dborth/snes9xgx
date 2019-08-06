#ifdef HW_RVL
#include <gccore.h>

static bool retrodeSetup = false;
static s32 deviceIdRetrode = 0;
static u8 endpointRetrode = 0;
static u8 bMaxPacketSizeRetrode = 0;

static u32 jpRetrode[4];

static bool isRetrodeGamepad(usb_devdesc devdesc)
{
	if (devdesc.idVendor != 0x0403 || devdesc.idProduct != 0x97C1 ||
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

static void openRetrode()
{
	usb_device_entry dev_entry[8];
	u8 dev_count;
	if (USB_GetDeviceList(dev_entry, 8, USB_CLASS_HID, &dev_count) < 0)
	{
		return;
	}

	// Retrode has two entries in USB_GetDeviceList(), one for gamepads and one for SNES mouse
	for (int i = 0; i < dev_count; ++i)
	{
		s32 fd;
		if (USB_OpenDevice(dev_entry[i].device_id, dev_entry[i].vid, dev_entry[i].pid, &fd) < 0)
		{
			continue;
		}

		usb_devdesc devdesc;
		if (USB_GetDescriptors(fd, &devdesc) < 0 || !isRetrodeGamepad(devdesc))
		{
			USB_CloseDevice(&fd);
			continue;
		}
		deviceIdRetrode = fd;
		endpointRetrode = getEndpoint(devdesc);
		bMaxPacketSizeRetrode = devdesc.bMaxPacketSize0;
	}
}

void Retrode_ScanPads()
{
	if(!retrodeSetup)
	{
		retrodeSetup = true;
		openRetrode();
	}

	if (deviceIdRetrode == 0)
	{
		return;
	}

	uint8_t ATTRIBUTE_ALIGN(32) buf[bMaxPacketSizeRetrode];

	if (USB_ReadIntrMsg(deviceIdRetrode, endpointRetrode, sizeof(buf), buf) != 5)
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

	// Retrode gamepad endpoint returns 5 bytes with gamepad events
	u32 jp12 = 0;
	jp12 |= ((buf[2] & 0x9C) == 0x9C) ? PAD_BUTTON_UP    : 0;
	jp12 |= ((buf[2] & 0x64) == 0x64) ? PAD_BUTTON_DOWN  : 0;
	jp12 |= ((buf[1] & 0x9C) == 0x9C) ? PAD_BUTTON_LEFT  : 0;
	jp12 |= ((buf[1] & 0x64) == 0x64) ? PAD_BUTTON_RIGHT : 0;

	jp12 |= (buf[3] & 0x10) ? PAD_BUTTON_A : 0;
	jp12 |= (buf[3] & 0x01) ? PAD_BUTTON_B : 0;
	jp12 |= (buf[3] & 0x20) ? PAD_BUTTON_X : 0;
	jp12 |= (buf[3] & 0x02) ? PAD_BUTTON_Y : 0;

	jp12 |= (buf[3] & 0x40) ? PAD_TRIGGER_L : 0;
	jp12 |= (buf[3] & 0x80) ? PAD_TRIGGER_R : 0;

	jp12 |= (buf[3] & 0x08) ? PAD_BUTTON_START : 0;
	jp12 |= (buf[3] & 0x04) ? PAD_TRIGGER_Z    : 0; // SNES select button maps to Z

	// Required, otherwise if the returned port isn't the one we are looking for, jp will be set to zero,
	// and held buttons are not possible w/o saving the state.
	jpRetrode[buf[0] - 1] = jp12;
}

u32 Retrode_ButtonsHeld(int chan)
{
	if (deviceIdRetrode == 0)
	{
		return 0;
	}
	return jpRetrode[chan];
}

#endif

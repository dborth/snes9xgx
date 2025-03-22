#ifdef HW_RVL
#include <gccore.h>

#define MAYFLASH_PC044_VID 0x0E8F
#define MAYFLASH_PC044_PID 0x3013	
#define MAYFLASH_MF105_VID 0x2F24
#define MAYFLASH_MF105_PID 0x00F1

static bool setup = false;
static bool replugRequired = false;
static s32 deviceId = 0; 
static s32 secondDeviceId = 0; //Need to keep track of 2 device IDs, since MF105 enumerates as 2 devices
static u8 endpoint = 0;
static u8 secondEndpoint = 0; //Need to keep track of 2 endpoints, since MF105 enumerates as 2 devices
static u8 bMaxPacketSize = 0; //Size of usb HID packets sent by device
static u32 jpMayflash[2]; //Array containing inputs for player 1 (index 0) and player 2 (index 1)
static s8 mayflashDeviceType = -1; //-1 for unkown/uninitialized, 0 for PC044, 1 for MF105

static bool isMayflashGamepad(usb_device_entry dev)
{
	return (dev.vid == MAYFLASH_PC044_VID && dev.pid == MAYFLASH_PC044_PID) || 
	(dev.vid == MAYFLASH_MF105_VID && dev.pid == MAYFLASH_MF105_PID);
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
// Opens the device gets the device Id(s), endpoint(s), packet size, etc
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
		//set the device type to the given adapter
		if (dev_entry[i].vid == MAYFLASH_PC044_VID && dev_entry[i].pid == MAYFLASH_PC044_PID)
		{
			mayflashDeviceType = 0;
		}
		else if (dev_entry[i].vid == MAYFLASH_MF105_VID && dev_entry[i].pid == MAYFLASH_MF105_PID)
		{
			mayflashDeviceType = 1;
			//If first device ID is uninitialized, initialize it now
			if (deviceId == 0)
			{
				deviceId = fd;
			}
			else
			{
				secondDeviceId = deviceId;
				secondEndpoint = endpoint;
			}
		}

		deviceId = fd;
		replugRequired = false;
		endpoint = getEndpoint(devdesc);
		bMaxPacketSize = devdesc.bMaxPacketSize0;
		USB_DeviceRemovalNotifyAsync(fd, &removal_cb, (void*) fd);
		//May need to continue searching for the other MF105
		if (mayflashDeviceType == 0 || secondDeviceId != 0)
		{
			break;
		}
	}

	setup = true;
}

u32 getButtonMappingPC044(const uint8_t *buf) 
{
//provided a buffer from a PC044, gets the currently pressed buttons and returns it as a u32
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

    // Directional buttons
    jp |= (buf[4] == 0x00) ? PAD_BUTTON_UP : 0;
    jp |= (buf[4] == 0xFF) ? PAD_BUTTON_DOWN : 0;
    jp |= (buf[3] == 0x00) ? PAD_BUTTON_LEFT : 0;
    jp |= (buf[3] == 0xFF) ? PAD_BUTTON_RIGHT : 0;

    // Action buttons
    jp |= ((buf[5] & 0x2F) == 0x2F) ? PAD_BUTTON_A : 0;
    jp |= ((buf[5] & 0x4F) == 0x4F) ? PAD_BUTTON_B : 0;
    jp |= ((buf[5] & 0x1F) == 0x1F) ? PAD_BUTTON_X : 0;
    jp |= ((buf[5] & 0x8F) == 0x8F) ? PAD_BUTTON_Y : 0;

    // Triggers
    jp |= ((buf[6] & 0x04) == 0x04) ? PAD_TRIGGER_L : 0;
    jp |= ((buf[6] & 0x08) == 0x08) ? PAD_TRIGGER_R : 0;

    // Start and Select (mapped to Z)
    jp |= ((buf[6] & 0x20) == 0x20) ? PAD_BUTTON_START : 0;
    jp |= ((buf[6] & 0x10) == 0x10) ? PAD_TRIGGER_Z : 0; // SNES select button maps to Z

    return jp;
}

u32 getButtonMappingMF105(const uint8_t *buf) 
{
//provided a buffer from a MF105, gets the currently pressed buttons and returns it as a u32
	//Button Inputs
	u32 jp = 0;
	jp |= ((buf[0] & 0x01) == 0x01) ? PAD_BUTTON_Y : 0;
	jp |= ((buf[0] & 0x02) == 0x02) ? PAD_BUTTON_B : 0;
	jp |= ((buf[0] & 0x04) == 0x04) ? PAD_BUTTON_A : 0;
	jp |= ((buf[0] & 0x08) == 0x08) ? PAD_BUTTON_X : 0;
	jp |= ((buf[0] & 0x10) == 0x10) ? PAD_TRIGGER_L : 0;
	jp |= ((buf[0] & 0x20) == 0x20) ? PAD_TRIGGER_R : 0;
	jp |= ((buf[1] & 0x01) == 0x01) ? PAD_TRIGGER_Z	: 0; // SNES select button maps to Z
	jp |= ((buf[1] & 0x02) == 0x02) ? PAD_BUTTON_START : 0;
	//Direction Inputs
	switch (buf[2]) {
		case 0x00: jp |= PAD_BUTTON_UP; break;
		case 0x01: jp |= (PAD_BUTTON_UP | PAD_BUTTON_RIGHT); break;
		case 0x02: jp |= PAD_BUTTON_RIGHT; break;
		case 0x03: jp |= (PAD_BUTTON_DOWN | PAD_BUTTON_RIGHT); break;
		case 0x04: jp |= PAD_BUTTON_DOWN; break;
		case 0x05: jp |= (PAD_BUTTON_DOWN | PAD_BUTTON_LEFT); break;
		case 0x06: jp |= PAD_BUTTON_LEFT; break;
		case 0x07: jp |= (PAD_BUTTON_UP | PAD_BUTTON_LEFT); break;
		case 0x08: break; // Neutral (no direction pressed)
	}

    return jp;
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

	// Process inputs for the PC044 type adapter
	if (mayflashDeviceType == 0)
	{
	// Required, otherwise if the returned port isn't the one we are looking for, jp will be set to zero,
	// and held buttons are not possible
	jpMayflash[buf[0] - 1] = getButtonMappingMF105(buf);	
	}
	//Mapping for the M105 Adapter
	else if (mayflashDeviceType == 1)
	{
	// First enumerated device is treated as player one, second as player two
	jpMayflash[0] = getButtonMappingMF105(buf); 
	
	//now get inputs for the second device
	res = USB_ReadIntrMsg(secondDeviceId, secondEndpoint, sizeof(buf), buf);
	if (res < 0)
	{
		return;
	}
	jpMayflash[1] = getButtonMappingMF105(buf); 
	}

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

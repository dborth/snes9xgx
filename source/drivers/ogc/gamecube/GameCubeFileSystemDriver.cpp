/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * GameCubeFileSystemDriver.cpp
 *
 * GameCube storage device enumeration + mounting: SD Gecko slots A/B,
 * SD2SP2 (port2), GC Loader, and DVD. Slots A/B and port2 are polled for
 * hotplug via a real EXI presence probe; GC Loader and DVD are not
 ***************************************************************************/
#include <stdio.h>
#include <sdcard/gcsd.h>
#include <ogc/dvd.h>

#include "GameCubeFileSystemDriver.h"

static DISC_INTERFACE * GetDiscSlotA()    { return get_io_gcsda(); }
static DISC_INTERFACE * GetDiscSlotB()    { return get_io_gcsdb(); }
static DISC_INTERFACE * GetDiscPort2()    { return get_io_gcsd2(); }
static DISC_INTERFACE * GetDiscGcLoader() { return &__io_gcode; }

// autoMountAtStartup: true for carda/cardb/port2 - all three go through
// the same cheap, safe EXI presence probe. GC Loader is startup-only
// (pollable=false) - see the descriptor field comment.
static const OgcFatSlotDescriptor gameCubeFatSlots[] =
{
	{ DEVICE_SD_SLOTA,    GetDiscSlotA,    "carda",    "carda:/",    "SD Gecko Slot A", "SD card not found!", false, true,  true  },
	{ DEVICE_SD_SLOTB,    GetDiscSlotB,    "cardb",    "cardb:/",    "SD Gecko Slot B", "SD card not found!", false, true,  true  },
	{ DEVICE_SD_PORT2,    GetDiscPort2,    "port2",    "port2:/",    "SD in SP2",       "SD card not found!", false, true,  true  },
	{ DEVICE_SD_GCLOADER, GetDiscGcLoader, "gcloader", "gcloader:/", "GC Loader",       "SD card not found!", false, false, false },
};

void GameCubeFileSystemDriver::init()
{
	DVD_Init();

	fatSlots     = gameCubeFatSlots;
	fatSlotCount = sizeof(gameCubeFatSlots) / sizeof(gameCubeFatSlots[0]);
	dvdDisc      = &__io_gcdvd;
	unsupportedFormatMessage = "Unsupported format - please use FAT32/exFAT.";

	smbDriver.init();

	initFatSlotsAndAutoMount();
}

const int * GameCubeFileSystemDriver::getValidLoadDevices(int & outCount) const
{
	static const int devices[] = { DEVICE_AUTO, DEVICE_SD_SLOTA, DEVICE_SD_SLOTB, DEVICE_SD_PORT2, DEVICE_SD_GCLOADER, DEVICE_DVD, DEVICE_SMB };
	outCount = sizeof(devices) / sizeof(devices[0]);
	return devices;
}

const int * GameCubeFileSystemDriver::getValidSaveDevices(int & outCount) const
{
	static const int devices[] = { DEVICE_AUTO, DEVICE_SD_SLOTA, DEVICE_SD_SLOTB, DEVICE_SD_PORT2, DEVICE_SD_GCLOADER, DEVICE_SMB };
	outCount = sizeof(devices) / sizeof(devices[0]);
	return devices;
}

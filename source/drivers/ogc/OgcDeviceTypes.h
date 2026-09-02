/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcDeviceTypes.h
 ***************************************************************************/
#pragma once

enum {
	DEVICE_AUTO = 0,
	DEVICE_SD,
	DEVICE_USB,
	DEVICE_DVD,
	DEVICE_SMB,
	DEVICE_SD_SLOTA,
	DEVICE_SD_SLOTB,
	DEVICE_SD_PORT2,
	DEVICE_SD_GCLOADER,
	DEVICE_LENGTH
};

const char pathPrefix[DEVICE_LENGTH][11] =
{ "", "sd:/", "usb:/", "dvd:/", "smb:/", "carda:/", "cardb:/", "port2:/", "gcloader:/" };

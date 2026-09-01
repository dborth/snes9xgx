/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcFileSystemDriver.cpp
 ***************************************************************************/
#include <fat.h>
#ifdef HW_RVL
#include <di/di.h>
#include <ogc/usbstorage.h>
#endif
#include "OgcFileSystemDriver.h"

void OgcFileSystemDriver::init()
{

}
void OgcFileSystemDriver::shutdown()
{
#ifdef HW_RVL
	fatUnmount("sd:");
	fatUnmount("usb:");
	DI_Close();
#else
	fatUnmount("port2:");
	fatUnmount("carda:");
	fatUnmount("cardb:");
	fatUnmount("gcloader:");
#endif
}

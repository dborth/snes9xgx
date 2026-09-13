/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * dvm_wut.c
 *
 * libdvm - wut backend integration
 ***************************************************************************/
#include "dvm_wut.h"
#include <dvm.h>
#include <fat.h>
#include <ntfs.h>
#include <stdalign.h>
#include <string.h>

static DvmWutUsbVolume g_usbVolumes[DVM_WUT_MAX_USB_VOLUMES];
static bool             g_driversRegistered = false;

bool dvmWutInit(void)
{
	if(g_driversRegistered)
		return true;

	// Both register idempotently (dvmRegisterFsDriver() no-ops on a
	// driver that's already registered).
	bool vfatOk  = dvmRegisterFsDriver(&g_vfatFsDriver);
	bool exfatOk = dvmRegisterFsDriver(&g_exfatFsDriver);
	bool ntfsOk  = dvmRegisterFsDriver(&g_ntfsFsDriver);

	g_driversRegistered = vfatOk && exfatOk && ntfsOk;
	return g_driversRegistered;
}

static DvmWutUsbVolume * findVolume(const char * name)
{
	for(int i = 0; i < DVM_WUT_MAX_USB_VOLUMES; i++)
		if(g_usbVolumes[i].name[0] != '\0' && strcmp(g_usbVolumes[i].name, name) == 0)
			return &g_usbVolumes[i];
	return NULL;
}

static DvmWutUsbVolume * claimVolumeSlot(const char * name)
{
	DvmWutUsbVolume * vol = findVolume(name);
	if(vol)
		return vol;

	for(int i = 0; i < DVM_WUT_MAX_USB_VOLUMES; i++)
	{
		if(g_usbVolumes[i].name[0] == '\0')
		{
			strncpy(g_usbVolumes[i].name, name, sizeof(g_usbVolumes[i].name) - 1);
			g_usbVolumes[i].name[sizeof(g_usbVolumes[i].name) - 1] = '\0';
			return &g_usbVolumes[i];
		}
	}
	return NULL;
}

bool dvmWutMountUsb(const char * name, DISC_INTERFACE * iface, unsigned cachePages, unsigned sectorsPerPage)
{
	if(!g_driversRegistered || !name || !iface)
		return false;

	DvmWutUsbVolume * vol = claimVolumeSlot(name);
	if(!vol || vol->isMounted)
		return false;

	DvmDisc * disc = dvmDiscCreate(iface);
	if(!disc)
		return false;

	if(cachePages)
	{
		DvmDisc * cachedDisc = dvmDiscCacheCreate(disc, cachePages, sectorsPerPage);
		if(!cachedDisc)
		{
			// Wrapping failed (eg. allocation failure) - fall back to the
			// raw, uncached disc rather than leaking it and handing a NULL
			// disc pointer down to dvmProbeMountDisc().
			cachedDisc = disc;
		}
		disc = cachedDisc;
	}

	// dvmProbeMountDisc() mounts every partition it recognizes, naming the
	// first "name" and any further ones "name2", "name3"... (see
	// dvm_prober.c).
	unsigned mounted = dvmProbeMountDisc(name, disc);
	if(!mounted)
	{
		disc->vt->destroy(disc);
		return false;
	}

	vol->disc = disc;
	vol->isMounted = true;
	return true;
}

void dvmWutUnmountUsb(const char * name)
{
	DvmWutUsbVolume * vol = findVolume(name);
	if(!vol || !vol->isMounted)
		return;

	// Drops the fat driver's reference on vol->disc; once that's the last
	// reference, this destroys the disc (flushing the cache first) and
	// shuts down the underlying DISC_INTERFACE - see dvmDiscAddUser()/
	// dvmDiscRemoveUser() in fat_driver.c and dvm_disc.c.
	dvmUnmountVolume(name);

	vol->disc = NULL;
	vol->isMounted = false;
}

bool dvmWutUsbStillPresent(const char * name)
{
	DvmWutUsbVolume * vol = findVolume(name);
	if(!vol || !vol->isMounted || !vol->disc)
		return false;

	alignas(LIBDVM_BUFFER_ALIGN) uint8_t scratch[512];
	return dvmDiscProbePresence((DvmDisc *)vol->disc, scratch);
}

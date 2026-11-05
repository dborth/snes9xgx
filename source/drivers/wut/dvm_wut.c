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

static DvmWutVolume volumes[DVM_WUT_MAX_VOLUMES];
static bool          driversRegistered = false;

bool dvmWutInit(void)
{
	if(driversRegistered)
		return true;

	// Both register idempotently (dvmRegisterFsDriver() no-ops on a
	// driver that's already registered).
	bool vfatOk  = dvmRegisterFsDriver(&g_vfatFsDriver);
	bool exfatOk = dvmRegisterFsDriver(&g_exfatFsDriver);
	bool ntfsOk  = dvmRegisterFsDriver(&g_ntfsFsDriver);

	driversRegistered = vfatOk && exfatOk && ntfsOk;
	return driversRegistered;
}

static DvmWutVolume * findVolume(const char * name)
{
	for(int i = 0; i < DVM_WUT_MAX_VOLUMES; i++)
		if(volumes[i].name[0] != '\0' && strcmp(volumes[i].name, name) == 0)
			return &volumes[i];
	return NULL;
}

static DvmWutVolume * claimVolumeSlot(const char * name)
{
	DvmWutVolume * vol = findVolume(name);
	if(vol)
		return vol;

	for(int i = 0; i < DVM_WUT_MAX_VOLUMES; i++)
	{
		if(volumes[i].name[0] == '\0')
		{
			strncpy(volumes[i].name, name, sizeof(volumes[i].name) - 1);
			volumes[i].name[sizeof(volumes[i].name) - 1] = '\0';
			return &volumes[i];
		}
	}
	return NULL;
}

bool dvmWutMountVolume(const char * name, DISC_INTERFACE * iface, unsigned cachePages, unsigned sectorsPerPage)
{
	return dvmWutMountVolumeEx(name, iface, cachePages, sectorsPerPage) == DVM_WUT_MOUNTED;
}

DvmWutMountResult dvmWutMountVolumeEx(const char * name, DISC_INTERFACE * iface, unsigned cachePages, unsigned sectorsPerPage)
{
	if(!driversRegistered || !name || !iface)
		return DVM_WUT_ERROR;

	DvmWutVolume * vol = claimVolumeSlot(name);
	if(!vol || vol->isMounted)
		return DVM_WUT_ERROR;

	DvmDisc * disc = dvmDiscCreate(iface);
	if(!disc)
		return DVM_WUT_NO_DEVICE; // iface->startup() failed (or isInserted() said no)

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

	// Can the disc actually be read? (A real, uncached read of sector 0.) This is what separates
	// "nothing usable there (yet)" from "a perfectly readable disc we have no driver for" - both
	// make dvmProbeMountDisc() return 0 below, but only the first is worth trying again.
	alignas(LIBDVM_BUFFER_ALIGN) uint8_t scratch[512]; // libdvm's sector size on Wii U is always 512
	const bool readable = dvmDiscProbePresence(disc, scratch);

	// dvmProbeMountDisc() mounts every partition it recognizes, naming the
	// first "name" and any further ones "name2", "name3"... (see
	// dvm_prober.c).
	unsigned mounted = readable ? dvmProbeMountDisc(name, disc) : 0;
	if(!mounted)
	{
		disc->vt->destroy(disc);
		return readable ? DVM_WUT_UNMOUNTABLE : DVM_WUT_NO_DEVICE;
	}

	vol->disc = disc;
	vol->isMounted = true;
	return DVM_WUT_MOUNTED;
}

void dvmWutUnmountVolume(const char * name)
{
	DvmWutVolume * vol = findVolume(name);
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

bool dvmWutVolumeStillPresent(const char * name)
{
	DvmWutVolume * vol = findVolume(name);
	if(!vol || !vol->isMounted || !vol->disc)
		return false;

	alignas(LIBDVM_BUFFER_ALIGN) uint8_t scratch[512];
	return dvmDiscProbePresence((DvmDisc *)vol->disc, scratch);
}

bool dvmWutGetVolumeLabel(const char * name, char * labelOut, size_t labelOutSize)
{
	if(!name || !labelOut || labelOutSize < 12)
		return false;

	labelOut[0] = '\0';

	// vfat/exfat
	if(fatGetVolumeLabel(name, labelOut) && labelOut[0] != '\0')
		return true;

	// ntfs
	labelOut[0] = '\0';
	return ntfsGetVolumeLabel(name, labelOut, labelOutSize) && labelOut[0] != '\0';
}

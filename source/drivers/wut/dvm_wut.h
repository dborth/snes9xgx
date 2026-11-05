/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * dvm_wut.h
 *
 * Wii U mounting on top of libdvm - USB1/2/3 (via Mocha_usbN_disc_interface)
 * Everything here requires Mocha (Mocha_InitLibrary() having already
 * succeeded) - without it there's no disc access at all.
 ***************************************************************************/
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <mocha/disc_interface.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DVM_WUT_MAX_VOLUMES 3  // 3 USB slots

//! One storage slot's dvm mount state. Purely internal bookkeeping for
//! dvm_wut.c itself - callers only ever deal in names.
typedef struct
{
	char  name[8];   //!< devoptab basename this volume was mounted as, eg. "usb1"
	bool  isMounted; //!< true while the volume is mounted through libdvm
	void* disc;      //!< opaque DvmDisc* - valid only while isMounted, NULL otherwise
} DvmWutVolume;

//! Registers libdvm's vfat + exfat + ntfs filesystem drivers. Call once at
//! startup before any dvmWutMountVolume() call. Safe to call more than once.
bool dvmWutInit(void);

//! Why a dvmWutMountVolumeEx() attempt did or didn't mount
typedef enum
{
	DVM_WUT_MOUNTED,     //!< mounted: at least one partition is reachable as "name:/"
	DVM_WUT_NO_DEVICE,   //!< the interface wouldn't start, or the disc couldn't be read: nothing there, or
	                     //!< not ready yet (eg. a drive still spinning up) - worth trying again later
	DVM_WUT_UNMOUNTABLE, //!< the disc reads fine but nothing on it is recognised - no MBR/VBR/GPT (a Wii U
	                     //!< WFS drive, or an unformatted one) or no partition with a supported filesystem.
	                     //!< Trying again changes nothing until the device itself changes.
	DVM_WUT_ERROR        //!< bad arguments, libdvm not initialised, no volume slot left, or already mounted
} DvmWutMountResult;

//! Probes iface for a mountable partition and, if found, mounts it as
//! "name:/" through libdvm (vfat or exfat or ntfs, whichever it actually turns
//! out to be - see dvmProbeMountDisc()). Safe to call repeatedly, 
//! genuinely re-probes hardware via iface->startup() each time (inside dvmDiscCreate()), 
//! and leaves iface shut down again if nothing mountable is found.
//!
//! cachePages/sectorsPerPage are passed straight to dvmDiscCacheCreate();
//! pass 0 cachePages to mount uncached (every access touches iface
//! directly - not recommended for normal use, but works, and is exactly
//! equivalent to what dvmWutVolumeStillPresent() forces on a cached mount).
bool dvmWutMountVolume(const char * name, DISC_INTERFACE * iface, unsigned cachePages, unsigned sectorsPerPage);

//! Same as dvmWutMountVolume(), but says why it didn't mount - in particular telling "nothing readable
//! there" (DVM_WUT_NO_DEVICE) from "a readable disc with nothing mountable on it" (DVM_WUT_UNMOUNTABLE).
//! The distinction costs nothing extra: a disc that can't be read is never handed to the partition prober.
DvmWutMountResult dvmWutMountVolumeEx(const char * name, DISC_INTERFACE * iface, unsigned cachePages, unsigned sectorsPerPage);

//! Unmounts "name:/" and releases the underlying DvmDisc, which shuts
//! down iface once nothing else references it (see fat_driver.c's
//! dvmDiscAddUser()/dvmDiscRemoveUser() pairing - the whole chain
//! self-cleans off this one call). Safe to call whether or not name is
//! currently mounted.
void dvmWutUnmountVolume(const char * name);

//! Genuine hardware liveness check for an already-mounted volume: forces
//! an uncached raw sector read through the DvmDisc dvmWutMountVolume()
//! created for it, under the same lock libdvm's cache uses for real file
//! I/O - see dvmDiscProbePresence() in dvm_cache.c. Unlike stat()-ing the
//! mount root (which libdvm's cache, like libfat's before it, can answer
//! entirely from memory without ever touching hardware again after
//! mount), this always re-touches the device. Returns false if name
//! isn't currently mounted through dvm_wut.
bool dvmWutVolumeStillPresent(const char * name);

//! Best-effort volume label for an already-mounted name
bool dvmWutGetVolumeLabel(const char * name, char * labelOut, size_t labelOutSize);

#ifdef __cplusplus
}
#endif

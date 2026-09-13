/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * dvm_wut.h
 *
 * Wii U USB mounting on top of libdvm
 * SD is deliberately out of scope here - WHBMountSdCard() already gives a
 * full native Cafe OS mount, so there's nothing for dvm to add there.
 ***************************************************************************/
#pragma once

#include <stdbool.h>
#include <mocha/disc_interface.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DVM_WUT_MAX_USB_VOLUMES 4

//! One USB storage slot's dvm mount state. Purely internal bookkeeping for
//! dvm_wut.c itself - callers only ever deal in names.
typedef struct
{
	char  name[8];   //!< devoptab basename this volume was mounted as, eg. "usb1"
	bool  isMounted;
	void* disc;      //!< opaque DvmDisc* - valid only while isMounted, NULL otherwise
} DvmWutUsbVolume;

//! Registers libdvm's vfat + exfat + ntfs filesystem drivers. Call once at
//! startup before any dvmWutMountUsb() call. Safe to call more than once.
bool dvmWutInit(void);

//! Probes iface for a mountable partition and, if found, mounts it as
//! "name:/" through libdvm (vfat or exfat or ntfs, whichever it actually turns
//! out to be - see dvmProbeMountDisc()). Safe to call repeatedly, 
//! genuinely re-probes hardware via iface->startup() each time (inside dvmDiscCreate()), 
//! and leaves iface shut down again if nothing mountable is found.
//!
//! cachePages/sectorsPerPage are passed straight to dvmDiscCacheCreate();
//! pass 0 cachePages to mount uncached (every access touches iface
//! directly - not recommended for normal use, but works, and is exactly
//! equivalent to what dvmWutUsbStillPresent() forces on a cached mount).
bool dvmWutMountUsb(const char * name, DISC_INTERFACE * iface, unsigned cachePages, unsigned sectorsPerPage);

//! Unmounts "name:/" and releases the underlying DvmDisc, which shuts
//! down iface once nothing else references it (see fat_driver.c's
//! dvmDiscAddUser()/dvmDiscRemoveUser() pairing - the whole chain
//! self-cleans off this one call). Safe to call whether or not name is
//! currently mounted.
void dvmWutUnmountUsb(const char * name);

//! Genuine hardware liveness check for an already-mounted volume: forces
//! an uncached raw sector read through the DvmDisc dvmWutMountUsb()
//! created for it, under the same lock libdvm's cache uses for real file
//! I/O - see dvmDiscProbePresence() in dvm_cache.c. Unlike stat()-ing the
//! mount root (which libdvm's cache, like libfat's before it, can answer
//! entirely from memory without ever touching hardware again after
//! mount), this always re-touches the device. Returns false if name
//! isn't currently mounted through dvm_wut.
bool dvmWutUsbStillPresent(const char * name);

#ifdef __cplusplus
}
#endif

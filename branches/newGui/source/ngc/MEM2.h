#ifndef MEM2_H
#define MEM2_H

#include "images/gfx_bg.h"

// Define a MegaByte
#define MB (1024*1024)

// MEM2 begins at MEM2_LO, the Starlet's Dedicated Memory begins at MEM2_HI
#define MEM2_LO   ((char*)0x90080000)
#define MEM2_HI   ((char*)0x933E0000)
#define MEM2_SIZE (MEM2_HI - MEM2_LO)

// menu backdrop unzipped
#define BGCACHE_SIZE (BG_RAW)	// about 0.6 MB
#define BGCACHE_LO   (MEM2_LO)
#define BGCACHE_HI   (BGCACHE_LO + BGCACHE_SIZE)

// texture memory for snes9x
#define SNESGFXCACHE_SIZE	(1024 * 512 * 2)	// 1MB
#define SNESGFXCACHE_LO		(BGCACHE_HI)
#define SNESGFXCACHE_HI		(SNESGFXCACHE_LO + SNESGFXCACHE_SIZE)

// texture cache for blended background
#define TEXCACHE1_SIZE	(640 * 480 * 4)		// ~1.2MB
#define TEXCACHE1_LO	(SNESGFXCACHE_HI)
#define TEXCACHE1_HI	(TEXCACHE1_LO + TEXCACHE1_SIZE)

// texture cache for menu overlay
#define TEXCACHE2_SIZE	(640 * 480 * 4)		// ~1.2MB
#define TEXCACHE2_LO	(TEXCACHE1_HI)
#define TEXCACHE2_HI	(TEXCACHE2_LO + TEXCACHE2_SIZE)

// rgb texture cache
#define GUICACHE_SIZE	(640 * 480 * 4)		// ~1.2MB
#define GUICACHE_LO		(TEXCACHE2_HI)
#define GUICACHE_HI		(GUICACHE_LO + GUICACHE_SIZE)

// filter memory (for hq2x, etc)
#define FILTERCACHE_SIZE	(256 * 240 * 2 * 4)		// 256*240*2bytes*4 (2xwidth,2xheight magnification) ~500 KB
#define FILTERCACHE_LO		(GUICACHE_HI)
#define FILTERCACHE_HI		(FILTERCACHE_LO + FILTERCACHE_SIZE)

// Unclaimed MEM2
#define UNCLAIMED_SIZE (MEM2_HI - FILTERCACHE_HI)
#define UNCLAIMED_LO   (FILTERCACHE_HI)
#define UNCLAIMED_HI   (MEM2_HI)

#endif


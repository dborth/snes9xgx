/****************************************************************************
 * Snes9x 1.50 
 *
 * Nintendo Gamecube Audio RAM
 *
 * softdev July 2006
 ****************************************************************************/
#ifndef _GCARAMI_

#define _GCARAMI_

#define AR_BACKDROP 0x8000
#define AR_SNESROM 0x200000

void ARAMPut (char *src, char *dst, int len);
void ARAMFetch (char *dst, char *src, int len);
void ARAMFetchSlow (char *dst, char *src, int len);

#endif

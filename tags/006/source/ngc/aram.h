/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 *
 * aram.h
 *
 * Gamecube Audio RAM storage
 ***************************************************************************/

#ifndef _GCARAMI_

#define _GCARAMI_

#define AR_BACKDROP 0x8000
#define AR_SNESROM 0x200000

void ARAMPut (char *src, char *dst, int len);
void ARAMFetch (char *dst, char *src, int len);
void ARAMFetchSlow (char *dst, char *src, int len);

#endif

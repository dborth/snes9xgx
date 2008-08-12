/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * softdev July 2006
 * crunchy2 May 2007
 *
 * memcardop.h
 *
 * Memory Card Routines.
 ****************************************************************************/

#ifndef _NGCMCSAVE_
#define _NGCMCSAVE_

int VerifyMCFile (unsigned char *buf, int slot, char *filename, int datasize);

int LoadBufferFromMC (unsigned char *buf, int slot, char *filename, bool8 silent);
int SaveBufferToMC (unsigned char *buf, int slot, char *filename, int datasize, bool8 silent);
int MountCard(int cslot, bool silent);
bool TestCard(int slot, bool silent);

#endif

/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May-June 2007
 * Tantric September 2008
 *
 * memcardop.cpp
 *
 * Memory Card routines
 ***************************************************************************/

#ifndef _NGCMCSAVE_
#define _NGCMCSAVE_

int VerifyMCFile (char *buf, int slot, char *filename, int datasize);
int LoadMCFile (char *buf, int slot, char *filename, bool silent);
int SaveMCFile (char *buf, int slot, char *filename, int datasize, bool silent);
int MountCard(int cslot, bool silent);
bool TestCard(int slot, bool silent);

#endif

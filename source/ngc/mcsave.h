/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * softdev July 2006
 * crunchy2 May 2007
 *
 * mcsave.cpp
 *
 * Memory Card Save Routines.
 ****************************************************************************/

#ifndef _NGCMCSAVE_
#define _NGCMCSAVE_

#define SAVEBUFFERSIZE ((512 * 1024) + 2048 + 64 + 4 + 4)

void ClearSaveBuffer ();

int VerifyMCFile (unsigned char *buf, int slot, char *filename, int datasize);

int LoadBufferFromMC (unsigned char *buf, int slot, char *filename, bool8 silent);
int SaveBufferToMC (unsigned char *buf, int slot, char *filename, int datasize, bool8 silent);

void LoadSRAMFromMC (int slot, int silent);
void SaveSRAMToMC (int slot, int silent);

void LoadPrefsFromMC (int slot, int silent);
void SavePrefsToMC (int slot, int silent);

#endif

/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May-June 2007
 * Tantric 2008-2009
 *
 * memcardop.cpp
 *
 * Memory Card routines
 ***************************************************************************/

#ifndef _MEMCARDOP_
#define _MEMCARDOP_

int ParseMCDirectory (int slot);
int LoadMCFile (char *buf, int slot, char *filename, bool silent);
int SaveMCFile (char *buf, int slot, char *filename, int datasize, bool silent);
bool TestMC(int slot, bool silent);
void SetMCSaveComments(char comments[2][32]);

#endif

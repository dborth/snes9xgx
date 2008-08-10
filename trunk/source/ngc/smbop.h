/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Wii/Gamecube Port
 * softdev July 2006
 * crunchy2 May 2007
 * Tantric August 2008
 *
 * smbop.h
 *
 * SMB support routines
 ****************************************************************************/

#ifndef _NGCSMB_

#define _NGCSMB_

bool InitializeNetwork(bool silent);
bool ConnectShare ();
int updateSMBdirname();
int parseSMBdirectory ();
int LoadSMBFile (char *filename, int length);
int LoadBufferFromSMB (char *filepath, bool8 silent);
int SaveBufferToSMB (char *filepath, int datasize, bool8 silent);

#endif

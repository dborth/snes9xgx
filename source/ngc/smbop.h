/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Tantric August 2008
 *
 * smbload.h
 *
 * SMB support routines
 ****************************************************************************/

#ifndef _NGCSMB_

#define _NGCSMB_

bool InitializeNetwork(bool silent);
bool ConnectShare (bool silent);
char * SMBPath(char * path);
int UpdateSMBdirname();
int ParseSMBdirectory ();
int LoadSMBFile (char * fbuffer, int length);
int LoadBufferFromSMB (char *filepath, bool silent);
int LoadBufferFromSMB (char * sbuffer, char *filepath, int length, bool silent);
int SaveBufferToSMB (char *filepath, int datasize, bool silent);
int SaveBufferToSMB (char * sbuffer, char *filepath, int datasize, bool silent);
#endif

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

#include <smb.h>

bool InitializeNetwork(bool silent);
bool ConnectShare (bool silent);
char * SMBPath(char * path);
int UpdateSMBdirname();
int ParseSMBdirectory ();
SMBFILE OpenSMBFile(char * filepath);
int LoadSMBSzFile(char * filepath, unsigned char * rbuffer);
int LoadSMBFile (char * sbuffer, char *filepath, int length, bool silent);
int SaveSMBFile (char * sbuffer, char *filepath, int length, bool silent);

extern SMBFILE smbfile;

#endif

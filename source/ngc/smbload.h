/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * softdev July 2006
 * crunchy2 May 2007
 *
 * smbload.cpp
 *
 * Load ROMS from a Network share.
 ****************************************************************************/

#ifndef _NGCSMB_

#define _NGCSMB_

void ConnectSMB ();
int parseSMBDirectory ();
int LoadSMBFile (char *filename, int length);
int LoadBufferFromSMB (char *filepath, bool8 silent);
int SaveBufferToSMB (char *filepath, int datasize, bool8 silent);

typedef struct
{
  char gcip[16];
  char gwip[16];
  char mask[16];
  char smbip[16];
  char smbuser[20];
  char smbpwd[20];
  char smbgcid[20];
  char smbsvid[20];
  char smbshare[20];
}
SMBINFO;


#endif

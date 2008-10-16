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
void SaveSRAMToSMB (bool8 silent);
void LoadSRAMFromSMB (bool8 silent);
void SavePrefsToSMB (bool8 silent);
void LoadPrefsFromSMB (bool8 silent);

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

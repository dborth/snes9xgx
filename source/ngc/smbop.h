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

void InitializeNetwork(bool silent);
bool ConnectShare (bool silent);
void CloseShare();

#endif

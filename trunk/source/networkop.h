/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2010
 *
 * networkop.h
 *
 * Network and SMB support routines
 ****************************************************************************/

#ifndef _NETWORKOP_H_
#define _NETWORKOP_H_

void UpdateCheck();
bool DownloadUpdate();
void InitializeNetwork(bool silent);
bool ConnectShare (bool silent);
void CloseShare();

extern bool updateFound;
extern bool inNetworkInit;

#endif

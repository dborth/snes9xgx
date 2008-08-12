/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Filesel - borrowed from GPP
 *
 * softdev July 2006
 * crunchy2 May 2007
 ****************************************************************************/

#ifndef _NGCFILESEL_
#define _NGCFILESEL_

#define SAVEBUFFERSIZE ((512 * 1024) + 2048 + 64 + 4 + 4)

void ClearSaveBuffer ();
int OpenROM (int method);
int autoLoadMethod();
int autoSaveMethod();
int FileSortCallback(const void *f1, const void *f2);

#endif

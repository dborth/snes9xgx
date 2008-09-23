/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * svpe & crunchy2 June 2007
 * Tantric September 2008
 *
 * dvd.h
 *
 * DVD I/O functions
 ***************************************************************************/

#ifndef _NGCDVD_
#define _NGCDVD_

int getpvd ();
int ParseDVDdirectory ();
int LoadDVDFile (unsigned char *buffer);
bool TestDVD();
int dvd_read (void *dst, unsigned int len, u64 offset);
bool SwitchDVDFolder(char dir[]);
void SetDVDDriveType();

#endif

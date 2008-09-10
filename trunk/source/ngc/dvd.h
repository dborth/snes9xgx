/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube DVD
 *
 * softdev July 2006
 * svpe & crunchy2 June 2007
 ****************************************************************************/

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

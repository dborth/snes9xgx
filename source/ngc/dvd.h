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

#ifndef _NGCDVD_H_
#define _NGCDVD_H_

int getpvd ();
int ParseDVDdirectory ();
int LoadDVDFile (unsigned char *buffer, int length);
bool TestDVD();
int dvd_read (void *dst, unsigned int len, u64 offset);
bool SwitchDVDFolder(char dir[]);
void SetDVDDriveType();
#ifdef HW_DOL
void dvd_motor_off ();
#endif

#endif

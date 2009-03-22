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

bool MountDVD(bool silent);
int ParseDVDdirectory(bool change);
void SetDVDdirectory(u64 dir, int length);
bool SwitchDVDFolder(char dir[]);

int LoadDVDFileOffset(unsigned char *buffer, int length);
int LoadDVDFile(char * buffer, char *filepath, int datasize, bool silent);
int dvd_safe_read (void *dst, unsigned int len, u64 offset);

void SetDVDDriveType();
#ifdef HW_DOL
void dvd_motor_off ();
void uselessinquiry ();
#endif

#endif

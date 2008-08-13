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

extern u64 rootdir;
extern int rootdirlength;

int getpvd ();
int ParseDVDdirectory ();
int LoadDVDFile (unsigned char *buffer);
int dvd_read (void *dst, unsigned int len, u64 offset);

#endif

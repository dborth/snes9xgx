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

#define MAXJOLIET 255
#define MAXDISPLAY 54
typedef struct
{
  u64 offset;
  unsigned int length;
  char flags;
  char filename[MAXJOLIET + 1];
  char displayname[MAXDISPLAY + 1];
} FILEENTRIES;

extern u64 rootdir;
extern int rootdirlength;
#define MAXFILES 2000			/** Restrict to 2000 files per dir **/
extern FILEENTRIES filelist[MAXFILES];

int getpvd ();
int ParseDVDdirectory ();
int LoadDVDFile (unsigned char *buffer);
int dvd_read (void *dst, unsigned int len, u64 offset);

#endif

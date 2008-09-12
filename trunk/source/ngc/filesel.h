/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Tantric August 2008
 *
 * filesel.h
 *
 * Generic file routines - reading, writing, browsing
 ****************************************************************************/

#ifndef _NGCFILESEL_
#define _NGCFILESEL_

#define SAVEBUFFERSIZE ((512 * 1024) + 2048 + 64 + 4 + 4)
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

#define MAXFILES 2000 // Restrict to 2000 files per dir
extern FILEENTRIES filelist[MAXFILES];

void ClearSaveBuffer ();
int OpenROM (int method);
int autoLoadMethod();
int autoSaveMethod();
int FileSortCallback(const void *f1, const void *f2);
void StripExt(char* returnstring, char * inputstring);

#endif

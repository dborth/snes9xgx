/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Michniewski 2008
 * Tantric August 2008
 *
 * filesel.h
 *
 * Generic file routines - reading, writing, browsing
 ****************************************************************************/

#ifndef _NGCFILESEL_
#define _NGCFILESEL_

#include <unistd.h>
#include <gccore.h>

#define MAXJOLIET 255
#define MAXDISPLAY 35

typedef struct
{
	char dir[MAXPATHLEN]; // directory path of browserList
	int numEntries; // # of entries in browserList
	int selIndex; // currently selected index of browserList
	int pageIndex; // starting index of browserList page display
} BROWSERINFO;

typedef struct
{
	u64 offset; // DVD offset
	unsigned int length; // file length
	tm mtime; // file modified time
	char isdir; // 0 - file, 1 - directory
	char filename[MAXJOLIET + 1]; // full filename
	char displayname[MAXDISPLAY + 1]; // name for browser display
} BROWSERENTRY;

extern BROWSERINFO browser;
extern BROWSERENTRY * browserList;
extern char rootdir[10];

extern unsigned long SNESROMSize;

bool MakeFilePath(char filepath[], int type, int method);
int OpenGameList(int method);
int autoLoadMethod();
int autoSaveMethod(bool silent);
int FileSortCallback(const void *f1, const void *f2);
void StripExt(char* returnstring, char * inputstring);
void ResetBrowser();
int BrowserChangeFolder(int method);
int BrowserLoadFile(int method);

#endif

/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Michniewski 2008
 * Tantric August 2008
 *
 * fileop.h
 *
 * File operations
 ****************************************************************************/

#ifndef _FILEOP_H_
#define _FILEOP_H_

#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <fat.h>
#include <unistd.h>

void InitDeviceThread();
void MountAllFAT();
void UnmountAllFAT();
bool ChangeInterface(int method, bool silent);
int ParseDirectory();
u32 LoadFile(char * rbuffer, char *filepath, u32 length, int method, bool silent);
u32 LoadFile(char filepath[], int method, bool silent);
u32 LoadSzFile(char * filepath, unsigned char * rbuffer);
u32 SaveFile(char * buffer, char *filepath, u32 datasize, int method, bool silent);
u32 SaveFile(char filepath[], u32 datasize, int method, bool silent);

extern char currFATdir[MAXPATHLEN];
extern FILE * file;
extern bool unmountRequired[];

#endif

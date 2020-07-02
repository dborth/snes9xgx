/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2020
 *
 * fileop.h
 *
 * File operations
 ****************************************************************************/

#ifndef _FILEOP_H_
#define _FILEOP_H_

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef HW_RVL
#define SAVEBUFFERSIZE (1024 * 1024 * 2) // leave room for IPS/UPS files and large images
#else
#define SAVEBUFFERSIZE (1024 * 1024 * 1)
#endif

void InitDeviceThread();
void ResumeDeviceThread();
void HaltDeviceThread();
void HaltParseThread();
void MountAllFAT();
void UnmountAllFAT();
bool FindDevice(char * filepath, int * device);
char * StripDevice(char * path);
bool ChangeInterface(int device, bool silent);
bool ChangeInterface(char * filepath, bool silent);
void CreateAppPath(char * origpath);
void FindAndSelectLastLoadedFile();
int ParseDirectory(bool waitParse = false, bool filter = true);
bool CreateDirectory(char * path);
void AllocSaveBuffer();
void FreeSaveBuffer();
size_t LoadFile(char * rbuffer, char *filepath, size_t length, size_t buffersize, bool silent);
size_t LoadFile(char * filepath, bool silent);
size_t LoadSzFile(char * filepath, unsigned char * rbuffer);
size_t LoadFont(char *filepath);
void LoadBgMusic();
size_t SaveFile(char * buffer, char *filepath, size_t datasize, bool silent);
size_t SaveFile(char * filepath, size_t datasize, bool silent);

extern unsigned char *savebuffer;
extern u8 *ext_font_ttf;
extern FILE * file;
extern bool unmountRequired[];
extern bool isMounted[];
extern int selectLoadedFile;

#endif

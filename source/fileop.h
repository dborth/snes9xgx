/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
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
#include "memmanager.h"

#define SAVEBUFFERSIZE (1024 * 512)

#if defined(HW_RVL) || defined(HW_DOL)
#define FILE_READ_CHUNK   4096
#define FILE_WRITE_CHUNK  4096
#define FILE_BUFFER_ALIGN 32
#else
#define FILE_READ_CHUNK   (256 * 1024)
#define FILE_WRITE_CHUNK  (256 * 1024)
#define FILE_BUFFER_ALIGN 0x40
#endif

void InitFileOpThreads();
void ResumeDeviceCheckingThread();
void HaltDeviceCheckingThread();
void HaltParseThread();
void MountAllFAT();
bool FindDevice(char * filepath, int * device);
char * StripDevice(char * path);
bool ChangeInterface(int device, bool silent);
bool ChangeInterface(char * filepath, bool silent);
bool ConnectShare(bool silent);
void CloseShare();
void CreateAppPath(char * origpath);
void FindAndSelectLastLoadedFile();
int ParseDirectory(bool waitParse = false, bool filter = true, const char * namePrefix = nullptr);
bool DirExists(const char * path);
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

// Background worker thread
typedef int (*BgTaskFn)(void *arg);
bool RunOnWorkerThread(BgTaskFn fn, void * arg = nullptr);
bool IsWorkerThreadFinished();
int GetWorkerThreadResult();

extern unsigned char *savebuffer;
extern uint8_t *ext_font_ttf;
extern FILE * file;
extern int selectLoadedFile;

#endif

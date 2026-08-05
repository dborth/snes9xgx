/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * memmanager.h
 *
 * Memory manager
 ***************************************************************************/

#ifndef _MEMMANAGER_H_
#define _MEMMANAGER_H_

#ifdef HW_RVL
#define SHAREDBUFFERSIZE (1024 * 1024 * 2) // leave room for IPS/UPS files and large images
#else
#define SHAREDBUFFERSIZE (1024 * 1024 * 1)
#endif

void InitMemManager();
void SwitchMemoryModeMenu();
void SwitchMemoryModeGame();

unsigned char * getSharedBuffer();
void ReleaseSharedBuffer();
void* extmem_malloc(u32 size);
void extmem_free(void *ptr);
int extmem_size_free();

#endif

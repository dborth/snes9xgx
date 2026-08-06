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

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

extern u8 * romPtr;

void InitMemManager();
void SwitchMemoryModeMenu();
void SwitchMemoryModeGame();
void* extmem_malloc(u32 size);
char* extmem_strdup(const char *s);
void extmem_free(void *ptr);
int extmem_size_free();

#ifdef __cplusplus
}
#endif

#endif

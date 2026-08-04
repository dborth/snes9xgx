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

enum
{
	MEMORY_MODE_MENU,
	MEMORY_MODE_GAME
};

void InitMemManager();
void SwitchMemoryMode(int mode);

void* extmem_malloc(u32 size);
void extmem_free(void *ptr);
int extmem_size_free();

#endif

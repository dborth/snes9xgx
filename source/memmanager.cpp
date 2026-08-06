/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * memmanager.cpp
 *
 * Memory manager
 ***************************************************************************/

#include <ogc/lwp_heap.h>
#include <ogc/system.h>
#include <malloc.h>
#include "snes9xgx.h"
#include "memmanager.h"
#include "filebrowser.h"
#include "fileop.h"
#include "snes9x/memmap.h"

#ifdef HW_DOL
#include "utils/vm/vm.h"
#endif

#define MEM2_SIZE		(42*1024*1024)

enum
{
	MEMORY_MODE_MENU,
	MEMORY_MODE_GAME
};

static heap_cntrl extmem_heap;
static int memoryMode = -1;
u8 * romPtr = NULL;

void InitMemManager ()
{
	#ifdef HW_DOL
	VM_Init(ARAM_SIZE, MRAM_BACKING); // Setup Virtual Memory with the entire ARAM
	__lwp_heap_init(&extmem_heap, (void *)ARAM_VM_BASE, ARAM_SIZE, 32);
	romPtr = (uint8 *)extmem_malloc(Memory.MAX_ROM_SIZE + 0x200 + 0x8000);
	#else
	void *mem2_heap_ptr = SYS_AllocArenaMem2Hi(MEM2_SIZE, 32);
	__lwp_heap_init(&extmem_heap, mem2_heap_ptr, MEM2_SIZE, 32);
	romPtr = (uint8 *) memalign(32,Memory.MAX_ROM_SIZE + 0x200 + 0x8000);
	#endif
}

void* extmem_malloc(u32 size)
{
	return __lwp_heap_allocate(&extmem_heap, size);
}

char* extmem_strdup(const char *s)
{
    if (!s)
        return NULL;

    size_t len = strlen(s) + 1;
    char *dup = (char *)extmem_malloc(len);

    if (dup)
        memcpy(dup, s, len);

    return dup;
}

void extmem_free(void *ptr)
{
	__lwp_heap_free(&extmem_heap, ptr);
}

int extmem_size_free()
{
	heap_iblock info;
	__lwp_heap_getinfo(&extmem_heap,&info);
	return info.free_size;
}

void SwitchMemoryModeMenu() {
	if(memoryMode == MEMORY_MODE_MENU)
		return;

	memoryMode = MEMORY_MODE_MENU;
	browserList = (BROWSERENTRY *)extmem_malloc(sizeof(BROWSERENTRY)*MAX_BROWSER_SIZE);
}

void SwitchMemoryModeGame() {
	if(memoryMode == MEMORY_MODE_GAME)
		return;

	memoryMode = MEMORY_MODE_GAME;
	extmem_free(browserList);
	browserList = NULL;
}

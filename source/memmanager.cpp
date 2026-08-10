/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * memmanager.cpp
 *
 * Memory manager
 ***************************************************************************/

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

static mspace extmem_space = NULL;
static int memoryMode = -1;
u8 * romPtr = NULL;

void InitMemManager ()
{
	void *base_ptr = NULL;
	size_t capacity = 0;

	#ifdef HW_DOL
	VM_Init(ARAM_SIZE, MRAM_BACKING); // Setup Virtual Memory with the entire ARAM
	base_ptr = (void *)ARAM_VM_BASE;
	capacity = ARAM_SIZE;
	#else
	base_ptr = SYS_AllocArenaMem2Hi(MEM2_SIZE, 32);
	capacity = MEM2_SIZE;
	#endif

	extmem_space = create_mspace_with_base(base_ptr, capacity, 0);
	mspace_set_footprint_limit(extmem_space, capacity);

	#ifdef HW_DOL
	romPtr = (uint8 *)extmem_malloc(Memory.MAX_ROM_SIZE + 0x200 + 0x8000);
	#else
	romPtr = (uint8 *) memalign(32, Memory.MAX_ROM_SIZE + 0x200 + 0x8000);
	#endif
}

void* extmem_malloc(u32 size)
{
	return mspace_malloc(extmem_space, size);
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
	mspace_free(extmem_space, ptr);
}

int extmem_size_free()
{
	struct mallinfo info = mspace_mallinfo(extmem_space);
	return info.fordblks;
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

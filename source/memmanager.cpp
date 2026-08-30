/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * memmanager.cpp
 *
 * Memory manager
 ***************************************************************************/

#include <malloc.h>
#include "snes9xgx.h"
#include "memmanager.h"
#include "filebrowser.h"
#include "fileop.h"
#include "snes9x/memmap.h"

#include "libgui/GuiImageData.h"

#ifdef HW_DOL
#include "utils/vm/vm.h"
#endif

#define MEM2_SIZE		(42*1024*1024)

enum
{
	MEMORY_MODE_MENU,
	MEMORY_MODE_GAME
};

static mspace aram_space = NULL;
static int memoryMode = -1;
u8 * romPtr = NULL;

void InitMemManager ()
{
	#ifdef HW_DOL
	VM_Init(ARAM_SIZE, MRAM_BACKING); // Setup Virtual Memory with the entire ARAM
	aram_space = create_mspace_with_base((void *)ARAM_VM_BASE, ARAM_SIZE, 0);
	mspace_set_footprint_limit(aram_space, ARAM_SIZE);
	romPtr = (uint8 *)extmem_malloc(Memory.MAX_ROM_SIZE + 0x200 + 0x8000);
	void * decodeScratch = extmem_malloc(IMAGE_DECODE_SCRATCH_SIZE);
	#else
	romPtr = (uint8 *) mem2_malloc(Memory.MAX_ROM_SIZE + 0x200 + 0x8000);
	void * decodeScratch = mem2_malloc(IMAGE_DECODE_SCRATCH_SIZE);
	#endif

	GuiImageData::setDecodeScratch(decodeScratch, decodeScratch ? IMAGE_DECODE_SCRATCH_SIZE : 0);

	SwitchMemoryModeMenu();
}

void* extmem_malloc(u32 size)
{
#if HW_RVL
	return mem2_malloc(size);
#else
	return mspace_malloc(aram_space, size);
#endif
}

char* extmem_strdup(const char *s)
{
#if HW_RVL
	return mem2_strdup(s);
#else
	if (!s)
		return NULL;

	size_t len = strlen(s) + 1;
	char *dup = (char *)extmem_malloc(len);

	if (dup)
		memcpy(dup, s, len);

	return dup;
#endif
}

void extmem_free(void *ptr)
{
#if HW_RVL
	return mem2_free(ptr);
#else
	mspace_free(aram_space, ptr);
#endif
}

int extmem_size_free()
{
#if HW_RVL
	return SYS_GetArena2Size();
#else
	struct mallinfo info = mspace_mallinfo(aram_space);
	return info.fordblks;
#endif
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

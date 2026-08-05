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
#include "snes9xgx.h"
#include "memmanager.h"
#include "filebrowser.h"
#include "fileop.h"

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

void InitMemManager ()
{
	#ifdef HW_DOL
	VM_Init(ARAM_SIZE, MRAM_BACKING); // Setup Virtual Memory with the entire ARAM
	__lwp_heap_init(&extmem_heap, (void *)ARAM_VM_BASE, ARAM_SIZE, 32);
	#else
	void *mem2_heap_ptr = SYS_AllocArenaMem2Hi(MEM2_SIZE, 32);
	__lwp_heap_init(&extmem_heap, mem2_heap_ptr, MEM2_SIZE, 32);
	#endif
}

void* extmem_malloc(u32 size)
{
	return __lwp_heap_allocate(&extmem_heap, size);
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

#ifdef HW_RVL
	savebuffer = (unsigned char *)extmem_malloc(SAVEBUFFERSIZE);
#else
	savebuffer = (unsigned char *)memalign(32,SAVEBUFFERSIZE);
#endif

	browserList = (BROWSERENTRY *)extmem_malloc(sizeof(BROWSERENTRY)*MAX_BROWSER_SIZE);
}

void SwitchMemoryModeGame() {
	if(memoryMode == MEMORY_MODE_GAME)
		return;

	memoryMode = MEMORY_MODE_GAME;

	free(savebuffer);
	extmem_free(browserList);
	savebuffer = NULL;
	browserList = NULL;
}

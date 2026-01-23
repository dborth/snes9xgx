/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2010-2023
 *
 * mem2.cpp
 *
 * MEM2 memory allocator
 ***************************************************************************/

#ifdef HW_RVL

#include <ogc/lwp_heap.h>
#include <ogc/system.h>

static heap_cntrl mem2_heap;

u32 InitMem2Manager () 
{
	int size = (20*1024*1024);
	void *mem2_heap_ptr = SYS_AllocArenaMem2Hi(size, 32);
	size = __lwp_heap_init(&mem2_heap, mem2_heap_ptr, size, 32);
	return size;
}

void* mem2_malloc(u32 size)
{
	return __lwp_heap_allocate(&mem2_heap, size);
}

bool mem2_free(void *ptr)
{
	return __lwp_heap_free(&mem2_heap, ptr);
}

#endif

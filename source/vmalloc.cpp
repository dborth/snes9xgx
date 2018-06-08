/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * emu_kidid 2015-2018
 *
 * vmalloc.cpp
 *
 * GC VM memory allocator
 ***************************************************************************/

#ifdef USE_VM

#include <ogc/machine/asm.h>
#include <ogc/lwp_heap.h>
#include <ogc/system.h>
#include <ogc/machine/processor.h>
#include "utils/vm/vm.h"

static heap_cntrl vm_heap;

static int vm_initialised = 0;

void InitVmManager () 
{
	__lwp_heap_init(&vm_heap, (void *)ARAM_VM_BASE, ARAM_SIZE, 32);
	vm_initialised = 1;
}

void* vm_malloc(u32 size)
{
	if(!vm_initialised) InitVmManager();
	return __lwp_heap_allocate(&vm_heap, size);
}

bool vm_free(void *ptr)
{
	if(!vm_initialised) InitVmManager();
	return __lwp_heap_free(&vm_heap, ptr);
}

int vm_size_free()
{
	if(!vm_initialised) InitVmManager();
	heap_iblock info;
	__lwp_heap_getinfo(&vm_heap,&info);
	return info.free_size;
}

#endif

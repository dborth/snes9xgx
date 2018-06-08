/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * emu_kidid 2015-2018
 *
 * vmalloc.h
 *
 * GC VM memory allocator
 ***************************************************************************/

#ifdef USE_VM

#ifndef _VMMANAGER_H_
#define _VMMANAGER_H_

void* vm_malloc(u32 size);
bool vm_free(void *ptr);
int vm_size_free();
#endif

#endif

/* Copyright 2013 tueidj All Rights Reserved
 * This code may not be used in any project
 * without explicit permission from the author.
 */

#ifndef _VM_H_
#define _VM_H_

#include <gccore.h>
#include <stdlib.h>
#include <malloc.h>
#include <errno.h>
#include <ogc/machine/processor.h>
#include <stdio.h>

#define KB 				(1024)
#define MB				(1024*KB)
#define MRAM_BACKING	(4*MB)			// Use 4MB to page our 16MB
#define ARAM_RESERVED	(64*KB)			// Reserved for DSP/AESND/etc
#define ARAM_VM_BASE	(0x7F000000)	// Map ARAM to here
#define ARAM_START		(ARAM_RESERVED + ARAM_VM_BASE) 
#define ARAM_SIZE		((16*MB) - ARAM_RESERVED)	// ARAM is ~16MB

// maximum virtual memory size
#define MAX_VM_SIZE      (256*1024*1024)
// maximum physical memory size
#define MAX_MEM_SIZE     (  8*1024*1024)
// minimum physical memory size
#define MIN_MEM_SIZE     (256*1024)
// page size as defined by hardware
#define PAGE_SIZE        4096
#define PAGE_MASK        (~(PAGE_SIZE-1))

#define VM_VSID          0
#define VM_SEGMENT       0x70000000

// use 64KB for PTEs
#define HTABMASK         0
#define PTE_SIZE         ((HTABMASK+1)*65536)
#define PTE_COUNT        (PTE_SIZE>>3)

//#define VM_FILENAME      "/tmp/pagefile.sys"

// keeps a record of each currently mapped page
typedef union
{
	u32 data;
	struct
	{
		u32 valid      :  1;
		u32 locked     :  1;
		u32 dirty      :  1;
		u32 pte_index  : 13;
		u32 page_index : 16;
	};
} p_map;

// maps VM addresses to mapped pages
typedef struct
{
	// data must be fetched when paging in?
	u16 committed  :  1;
	u16 p_map_index: 12;
} vm_map;

typedef union
{
	u32 data[2];
	struct
	{
		u32 valid  :  1;
		u32 VSID   : 24;
		u32 hash   :  1;
		u32 API    :  6;

		u32 RPN    : 20;
		u32 pad0   :  3;
		u32 R      :  1;
		u32 C      :  1;
		u32 WIMG   :  4;
		u32 pad1   :  1;
		u32 PP     :  2;
	};
} PTE;
typedef PTE* PTEG;

extern void* VM_Init(u32 VMSize, u32 MEMSize);
extern void VM_Deinit(void);

#endif

/* Copyright 2013 tueidj All Rights Reserved
 * This code may not be used in any project
 * without explicit permission from the author.
 */

#ifndef _VM_H_
#define _VM_H_
#ifdef HW_DOL
#include <gctypes.h>

#define MRAM_BACKING	(3*1024*1024) // Use 3MB to page our 16MB
#define ARAM_VM_BASE	(0x7F000000) // Map ARAM to here
#define ARAM_RESERVED	(16*1024) // Reserved for DSP
#define ARAM_SIZE		((16*1024*1024) - ARAM_RESERVED) // ARAM is ~16MB

#ifdef __cplusplus
extern "C" {
#endif

void* VM_Init(u32 VMSize, u32 MEMSize);
void VM_Deinit(void);

#ifdef __cplusplus
}
#endif
#endif
#endif

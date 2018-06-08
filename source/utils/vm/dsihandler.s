/* Copyright 2013 tueidj All Rights Reserved
 * This code may not be used in any project
 * without explicit permission from the author.
 */

#include <ppc-asm.h>
#include "asm.h"

	.extern vm_dsi_handler
	.extern default_exceptionhandler

FUNC_START(dsi_handler)
	stwu        sp,-EXCEPTION_FRAME_END(sp)
	stw         r6,GPR6_OFFSET(sp)
	stw         r7,GPR7_OFFSET(sp)
	stw         r8,GPR8_OFFSET(sp)
	stw         r9,GPR9_OFFSET(sp)
	stw         r10,GPR10_OFFSET(sp)
	stw         r11,GPR11_OFFSET(sp)
	stw         r12,GPR12_OFFSET(sp)
	mfdsisr     r4
	mfmsr       r3
	ori         r3,r3,MSR_RI
	mtmsr       r3
	
	addi        r3,sp,8
	bl          vm_dsi_handler
	
	# check if it was handled correctly
	cmplwi      r3,0

	lwz         r6,GPR6_OFFSET(sp)
	lwz         r7,GPR7_OFFSET(sp)
	lwz         r8,GPR8_OFFSET(sp)
	lwz         r9,GPR9_OFFSET(sp)
	lwz         r10,GPR10_OFFSET(sp)
	lwz         r11,GPR11_OFFSET(sp)
	lwz         r12,GPR12_OFFSET(sp)

	# clear MSR_RI
	mfmsr       r3
	rlwinm      r3,r3,0,31,29
	mtmsr       r3

	bne         1f
	
	# jump to libogc's default handler
	addi        sp,sp,EXCEPTION_FRAME_END
	b           default_exceptionhandler

1:	
	lwz         r3,CR_OFFSET(sp)
	lwz         r4,LR_OFFSET(sp)
	lwz         r5,CTR_OFFSET(sp)
	lwz         r0,XER_OFFSET(sp)
	mtcr        r3
	mtlr        r4
	mtctr       r5
	mtxer       r0
	lwz         r0,GPR0_OFFSET(sp)
	lwz         r5,GPR5_OFFSET(sp)
	
	lwz         r3,SRR0_OFFSET(sp)
	lwz         r4,SRR1_OFFSET(sp)
	mtsrr0      r3
	mtsrr1      r4
	lwz         r3,GPR3_OFFSET(sp)
	lwz         r4,GPR4_OFFSET(sp)
	lwz         sp,GPR1_OFFSET(sp)
	rfi
FUNC_END(dsi_handler)

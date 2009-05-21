/*-------------------------------------------------------------

maketex.cpp - Helper functions for GX texture conversion
Modified for a buffer with an offset (border)

Copyright (C) 2008
softdev
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/
#include <gctypes.h>

void
MakeTexture (const void *src, void *dst, s32 width, s32 height)
{
	register u32 tmp0=0,tmp1=0,tmp2=0,tmp3=0;

	__asm__ __volatile__ (
		"	srwi		%6,%6,2\n"
		"	srwi		%7,%7,2\n"
		"	subi		%3,%4,4\n"
		"	mr			%4,%3\n"
		"	subi		%4,%4,4\n"

		"2: mtctr		%6\n"
		"	mr			%0,%5\n"
		//
		"1: lwz			%1,0(%5)\n"			//1
		"	stwu		%1,8(%4)\n"
		"	lwz			%2,4(%5)\n"			//1
		"	stwu		%2,8(%3)\n"
		"	lwz			%1,1032(%5)\n"		//2
		"	stwu		%1,8(%4)\n"
		"	lwz			%2,1036(%5)\n"		//2
		"	stwu		%2,8(%3)\n"
		"	lwz			%1,2064(%5)\n"		//3
		"	stwu		%1,8(%4)\n"
		"	lwz			%2,2068(%5)\n"		//3
		"	stwu		%2,8(%3)\n"
		"	lwz			%1,3096(%5)\n"		//4
		"	stwu		%1,8(%4)\n"
		"	lwz			%2,3100(%5)\n"		//4
		"	stwu		%2,8(%3)\n"
		"	addi		%5,%5,8\n"
		"	bdnz		1b\n"
		"	addi		%5,%0,4128\n"		//5
		"	subic.		%7,%7,1\n"
		"	bne			2b"
		//		0			 1			  2			   3		   4		  5		    6		    7
		: "=&r"(tmp0), "=&r"(tmp1), "=&r"(tmp2), "=&r"(tmp3), "+r"(dst) : "r"(src), "r"(width), "r"(height)
	);
}

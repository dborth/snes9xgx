/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 *
 * aram.cpp
 *
 * Gamecube Audio RAM storage
 ***************************************************************************/

#ifdef HW_DOL

#include <gccore.h>
#include <string.h>

#include "aram.h"

#define ARAM_READ				1
#define ARAM_WRITE				0

#define TEMPSIZE 32768
static char tempbuffer[TEMPSIZE] ATTRIBUTE_ALIGN (32);

/****************************************************************************
 * ARAMPut
 *
 * Move data from MAIN memory to ARAM
 ***************************************************************************/
void
ARAMPut (char *src, char *dst, int len)
{
	DCFlushRange (src, len);
	AR_StartDMA (ARAM_WRITE, (u32) src, (u32) dst, len);
	while (AR_GetDMAStatus());
}

/****************************************************************************
 * ARAMFetch
 *
 * This function will move data from ARAM to MAIN memory
 ***************************************************************************/
void
ARAMFetch (char *dst, char *src, int len)
{
	DCInvalidateRange (dst, len);
	AR_StartDMA (ARAM_READ, (u32) dst, (u32) src, len);
	while (AR_GetDMAStatus ());
}

/****************************************************************************
 * ARAMFetchSlow
 *
 * Required as SNES memory may NOT be 32-byte aligned
 ***************************************************************************/
void ARAMFetchSlow(char *dst, char *src, int len)
{
	int t;

	if (len > TEMPSIZE)
	{
		t = 0;
		while (t < len)
		{
			ARAMFetch(tempbuffer, src + t, TEMPSIZE);

			if (t + TEMPSIZE > len)
			{
				memcpy(dst + t, tempbuffer, len - t);
			}
			else
				memcpy(dst + t, tempbuffer, TEMPSIZE);

			t += TEMPSIZE;
		}

	}
	else
	{
		ARAMFetch(tempbuffer, src, len);
		memcpy(dst, tempbuffer, len);
	}
}
#endif

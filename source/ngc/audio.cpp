/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 *
 * audio.cpp
 *
 * Audio driver
 * Audio is fixed to 32Khz/16bit/Stereo
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asndlib.h>

#include "snes9x.h"
#include "memmap.h"
#include "s9xdebug.h"
#include "cpuexec.h"
#include "ppu.h"
#include "apu.h"
#include "display.h"
#include "gfx.h"
#include "soundux.h"
#include "spc700.h"
#include "spc7110.h"
#include "controls.h"

#include "video.h"

extern int ConfigRequested;

/*** Double buffered audio ***/
#define AUDIOBUFFER 2048
static unsigned char soundbuffer[2][AUDIOBUFFER] __attribute__ ((__aligned__ (32)));
static int whichab = 0;	/*** Audio buffer flip switch ***/

#define AUDIOSTACK 16384
static lwpq_t audioqueue;
static lwp_t athread;
static uint8 astack[AUDIOSTACK];

/****************************************************************************
 * Audio Threading
 ***************************************************************************/
static void *
AudioThread (void *arg)
{
	LWP_InitQueue (&audioqueue);

	while (1)
	{
		whichab ^= 1;
		if (ConfigRequested)
			memset (soundbuffer[whichab], 0, AUDIOBUFFER);
		else
		{
			so.samples_mixed_so_far = so.play_position = 0;
			S9xMixSamples (soundbuffer[whichab], AUDIOBUFFER >> 1);
		}
		LWP_ThreadSleep (audioqueue);
	}

	return NULL;
}

/****************************************************************************
 * MixSamples
 * This continually calls S9xMixSamples On each DMA Completion
 ***************************************************************************/
static void
GCMixSamples ()
{
	if (!ConfigRequested)
	{
		AUDIO_StopDMA ();
		DCFlushRange (soundbuffer[whichab], AUDIOBUFFER);
		AUDIO_InitDMA ((u32) soundbuffer[whichab], AUDIOBUFFER);
		AUDIO_StartDMA ();

		LWP_ThreadSignal (audioqueue);
	}
}

/****************************************************************************
 * InitGCAudio
 ***************************************************************************/
void
InitGCAudio ()
{
	LWP_CreateThread (&athread, AudioThread, NULL, astack, AUDIOSTACK, 150);
}

void
SwitchAudioMode(int mode)
{
	if(mode == 0) // emulator
	{
		#ifndef NO_SOUND
		ASND_Pause(1);
		ASND_End();
		#endif
		AUDIO_SetDSPSampleRate(AI_SAMPLERATE_32KHZ);
		AUDIO_RegisterDMACallback(GCMixSamples);
	}
	else // menu
	{
		AUDIO_StopDMA();
		AUDIO_RegisterDMACallback(NULL);
		#ifndef NO_SOUND
		ASND_Init();
		ASND_Pause(0);
		#endif
	}
}

/****************************************************************************
 * AudioStart
 *
 * Called to kick off the Audio Queue
 ***************************************************************************/
void
AudioStart ()
{
	GCMixSamples ();
}

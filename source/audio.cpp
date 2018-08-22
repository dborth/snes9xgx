/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * Tantric 2008-2010
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

#include "video.h"

#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"
#include "snes9x/cpuexec.h"
#include "snes9x/ppu.h"
#include "snes9x/apu/apu.h"
#include "snes9x/display.h"
#include "snes9x/gfx.h"
#include "snes9x/spc7110.h"
#include "snes9x/controls.h"

extern int ScreenshotRequested;
extern int ConfigRequested;

/*** Double buffered audio ***/
#define SAMPLES_TO_PROCESS 1024
#define AUDIOBUFFER 2048
#define BUFFERCOUNT 16
static u8 soundbuffer[BUFFERCOUNT][AUDIOBUFFER] __attribute__ ((__aligned__ (32)));
static int playab = 0;
static int nextab = 0;
int available = 0;

static inline void updateAvailable(int diff) {
	available += diff;

	if(available < 0) {
		available = 0;
	}
	else if(available > BUFFERCOUNT-1) {
		available = BUFFERCOUNT-1;
	}
}

static void DMACallback () {
	if (!ScreenshotRequested && !ConfigRequested) {
		AUDIO_InitDMA ((u32) soundbuffer[playab], AUDIOBUFFER);
		updateAvailable(-1);
		playab = (playab + 1) % BUFFERCOUNT;
	}
}

static void S9xAudioCallback (void *data) {
	//S9xUpdateDynamicRate(); // TODO: what arguments should be passed here?
	S9xFinalizeSamples();
	int availableSamples = S9xGetSampleCount();

	if (ScreenshotRequested || ConfigRequested) {
		AUDIO_StopDMA();
	}
	else if(availableSamples >= SAMPLES_TO_PROCESS) {
		nextab = (nextab + 1) % BUFFERCOUNT;
		updateAvailable(1);
		S9xMixSamples (soundbuffer[nextab], SAMPLES_TO_PROCESS);
		DCFlushRange (soundbuffer[nextab], AUDIOBUFFER);

		if(playab == -1) {
			if(available > 2) {
				playab = 0;
				AUDIO_StartDMA();
			}
		}
	}
}

/****************************************************************************
 * InitAudio
 ***************************************************************************/
void
InitAudio ()
{
	#ifdef NO_SOUND
	AUDIO_Init (NULL);
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AUDIO_RegisterDMACallback(DMACallback);
	#else
	ASND_Init();
	#endif
}

/****************************************************************************
 * SwitchAudioMode
 *
 * Switches between menu sound and emulator sound
 ***************************************************************************/
void
SwitchAudioMode(int mode)
{
	if(mode == 0) // emulator
	{
		#ifndef NO_SOUND
		ASND_Pause(1);
		ASND_End();
		AUDIO_StopDMA();
		AUDIO_RegisterDMACallback(NULL);
		DSP_Halt();
		AUDIO_RegisterDMACallback(DMACallback);
		#endif
		S9xSetSamplesAvailableCallback(S9xAudioCallback, NULL);
	}
	else // menu
	{
		S9xSetSamplesAvailableCallback(NULL, NULL);
		#ifndef NO_SOUND
		DSP_Unhalt();
		ASND_Init();
		ASND_Pause(0);
		#else
		AUDIO_StopDMA();
		#endif
	}
}

/****************************************************************************
 * ShutdownAudio
 *
 * Shuts down audio subsystem. Useful to avoid unpleasant sounds if a
 * crash occurs during shutdown.
 ***************************************************************************/
void ShutdownAudio()
{
	AUDIO_StopDMA();
}

/****************************************************************************
 * AudioStart
 *
 * Called to kick off the Audio Queue
 ***************************************************************************/
void
AudioStart ()
{
	available = 0;
	nextab = 0;
	playab = -1;
}

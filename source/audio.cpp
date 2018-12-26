/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2019
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
static int unplayed = 0;

static inline void updateUnplayed(int diff) {
	unplayed += diff;

	if(unplayed < 0) {
		unplayed = 0;
	}
	else if(unplayed > BUFFERCOUNT-1) {
		unplayed = BUFFERCOUNT-1;
	}
}

static void DMACallback () {
	if (!ScreenshotRequested && !ConfigRequested) {
		updateUnplayed(-1);
		AUDIO_InitDMA ((u32) soundbuffer[playab], AUDIOBUFFER);
		playab = (playab + 1) % BUFFERCOUNT;
	}
}

static void S9xAudioCallback (void *data) {
	double rate = 1.0;

	if(unplayed > 8) {
		rate = 1.005;
	}
	else if(unplayed < 4) {
		rate = 0.995;
	}

	S9xUpdateDynamicRate(rate);
	S9xFinalizeSamples();

	if (ScreenshotRequested || ConfigRequested) {
		AUDIO_StopDMA();
	}
	else if(S9xGetSampleCount() >= SAMPLES_TO_PROCESS) {
		S9xMixSamples (soundbuffer[nextab], SAMPLES_TO_PROCESS);
		DCFlushRange (soundbuffer[nextab], AUDIOBUFFER);
		updateUnplayed(1);
		nextab = (nextab + 1) % BUFFERCOUNT;
		
		if(!Settings.TurboMode && ((nextab + 1) % BUFFERCOUNT) == playab) {
		 	// quick and dirty attempt to prevent reading and writing from/to the same buffer
			nextab = (nextab + BUFFERCOUNT/2) % BUFFERCOUNT;
		}

		if(playab == -1) {
			if(unplayed > 2) {
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
	unplayed = 0;
	nextab = 0;
	playab = -1;
}

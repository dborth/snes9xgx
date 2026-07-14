/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2023
 *
 * audio.cpp
 *
 * Audio driver
 * Audio is fixed to 48Khz/16bit/Stereo
 ***************************************************************************/
#ifndef NO_SOUND
#include <asndlib.h>
#endif
#include "video.h"

#include "snes9x/memmap.h"
#include "snes9x/apu/apu.h"

extern int ScreenshotRequested;
extern int ConfigRequested;

void AudioStart ();

/*** Double buffered audio ***/
#define SAMPLES_TO_PROCESS 1024
#define AUDIOBUFFER 2048
#define BUFFERCOUNT 16

// BUFFERCOUNT must be a power of two so the ring index can advance with a cheap
// bitwise mask (see nextIndex) instead of an integer modulo on the hot path.
static_assert((BUFFERCOUNT & (BUFFERCOUNT - 1)) == 0, "BUFFERCOUNT must be a power of two");

// Dynamic-rate control: nudge the emulated sample rate up or down slightly to
// keep the number of unplayed (queued) buffers within a comfortable band,
// preventing both buffer underruns (audio gaps) and overruns (added latency).
// The controller is stateful with hysteresis: it engages a correction when the
// queue crosses an outer threshold and only releases it once the queue returns
// past a tighter inner threshold near the target, avoiding boundary hunting.
#define UNPLAYED_HIGH_WATER 8       // above this we are building latency, slow down
#define UNPLAYED_HIGH_RELEASE 6     // stay slow until the queue drains back to here
#define UNPLAYED_LOW_RELEASE 6      // stay fast until the queue fills back to here
#define UNPLAYED_LOW_WATER 4        // below this we risk an underrun, speed up
#define UNPLAYED_START_LEVEL 4      // queue at least this many buffers before starting DMA
#define RATE_SLOW_DOWN 1.005        // emit samples slightly slower to drain the queue
#define RATE_SPEED_UP 0.995         // emit samples slightly faster to fill the queue
#define RATE_NEUTRAL 1.0

// Discrete state of the dynamic-rate controller. Kept separate from the rate
// multiplier so the hysteresis logic compares enums rather than floating-point
// values (exact, and robust against future tweaks to the multipliers).
enum RateState {
    RATE_STATE_NEUTRAL,
    RATE_STATE_DRAINING,  // running slow to shrink an over-full queue
    RATE_STATE_FILLING,   // running fast to grow an under-full queue
};

// Maximum allowed queued buffers (12 out of 16).
// Leaves a mandatory 4-buffer (~85ms) safety zone before playab.
#define MAX_QUEUED_BUFFERS 12

static u8 soundbuffer[BUFFERCOUNT][AUDIOBUFFER] __attribute__ ((__aligned__ (32)));

// These are shared between S9xAudioCallback and the DMA interrupt callback,
// so they must not be cached in registers across reads
static volatile int playab = 0;
static volatile int nextab = 0;
static bool dma_started = false;
static bool turbo_drop = false;
// Current dynamic-rate controller state. Only touched by S9xAudioCallback and
// AudioStart (both non-interrupt context), so it needs no synchronization.
// Persisting it gives the controller hysteresis across cycles.
static RateState rateState = RATE_STATE_NEUTRAL;

static inline int nextIndex(int current) {
	return (current + 1) & (BUFFERCOUNT - 1);
}

// Ring buffer occupancy calculation
static inline int getUnplayed() {
	return (nextab - playab + BUFFERCOUNT) & (BUFFERCOUNT - 1);
}

static void DMACallback() {
	AUDIO_InitDMA((u32) soundbuffer[playab], AUDIOBUFFER);
	playab = nextIndex(playab);
}

static void S9xAudioCallback (void *data) {
	int unplayed = getUnplayed();
	double rate = RATE_NEUTRAL;

	if (Settings.TurboMode) {
		rateState = RATE_STATE_NEUTRAL;

		// fast-forward hysteresis:
		// If dropping, keep dropping until the queue drains to 4.
		// If capturing, keep capturing until the queue fills to 12.
		if (turbo_drop && unplayed <= UNPLAYED_LOW_WATER) {
			turbo_drop = false;
		} else if (!turbo_drop && unplayed >= MAX_QUEUED_BUFFERS) {
			turbo_drop = true;
		}
	} else {
		turbo_drop = false; // Always reset when leaving turbo

		if(rateState == RATE_STATE_DRAINING) {
			if(unplayed <= UNPLAYED_HIGH_RELEASE) {
				rateState = RATE_STATE_NEUTRAL;
			}
		}
		else if(rateState == RATE_STATE_FILLING) {
			if(unplayed >= UNPLAYED_LOW_RELEASE) {
				rateState = RATE_STATE_NEUTRAL;
			}
		}

		if(unplayed > UNPLAYED_HIGH_WATER) {
			rateState = RATE_STATE_DRAINING;
		}
		else if(unplayed < UNPLAYED_LOW_WATER) {
			rateState = RATE_STATE_FILLING;
		}

		if(rateState == RATE_STATE_DRAINING) {
			rate = RATE_SLOW_DOWN;
		}
		else if(rateState == RATE_STATE_FILLING) {
			rate = RATE_SPEED_UP;
		}
	}

	S9xUpdateDynamicRate(rate);
	S9xFinalizeSamples();

	if (ScreenshotRequested || ConfigRequested) {
		// Stop playback while the screenshot/config overlay is active. Reset the
		// ring so that once the request clears, the start path below re-primes
		// and restarts DMA cleanly instead of leaving playback dead on a stale,
		// never-rearmed buffer index.
		AUDIO_StopDMA();
		AudioStart();
		return;
	}

	while(S9xGetSampleCount() >= SAMPLES_TO_PROCESS) {
		unplayed = getUnplayed();

		if(!turbo_drop && unplayed < MAX_QUEUED_BUFFERS) {
			S9xMixSamples(soundbuffer[nextab], SAMPLES_TO_PROCESS);
			DCFlushRange(soundbuffer[nextab], AUDIOBUFFER);
			nextab = nextIndex(nextab);

			// Handle initial DMA pre-roll / priming
			if(!dma_started && getUnplayed() >= UNPLAYED_START_LEVEL) {
				AUDIO_InitDMA((u32) soundbuffer[0], AUDIOBUFFER);
				playab = nextIndex(0);
				AUDIO_StartDMA();
				dma_started = true;
			}
		} else {
			// Buffer ring is full (Turbo mode running faster than real-time playback).
			// Safely drop excess samples to keep APU emulation moving.
			static u8 dummy[AUDIOBUFFER] __attribute__ ((__aligned__ (32)));
			S9xMixSamples(dummy, SAMPLES_TO_PROCESS);
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
	AUDIO_Init(NULL);
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
		// Reset the ring so playback re-primes from a known state instead of
		// resuming on stale indices left over from the previous session.
		AudioStart();
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
	nextab = 0;
	playab = 0;
	dma_started = false;
	turbo_drop = false;
	rateState = RATE_STATE_NEUTRAL;
}

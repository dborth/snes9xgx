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
#define UNPLAYED_START_LEVEL 2      // queue at least this many buffers before starting DMA
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

static u8 soundbuffer[BUFFERCOUNT][AUDIOBUFFER] __attribute__ ((__aligned__ (32)));

// These are shared between S9xAudioCallback and the DMA interrupt callback,
// so they must not be cached in registers across reads.
static volatile int playab = 0;
static volatile int nextab = 0;
static volatile int unplayed = 0;
static volatile bool dma_started = false;

// Current dynamic-rate controller state. Only touched by S9xAudioCallback and
// AudioStart (both non-interrupt context), so it needs no synchronization.
// Persisting it gives the controller hysteresis across cycles.
static RateState rateState = RATE_STATE_NEUTRAL;

static inline int nextIndex(int index) {
	// index is always in [0, BUFFERCOUNT) here, so the mask is equivalent to
	// (index + 1) % BUFFERCOUNT but avoids an integer division on the hot path.
	return (index + 1) & (BUFFERCOUNT - 1);
}

static inline void updateUnplayed(int diff) {
	// Read-modify-write of a value shared with the DMA interrupt callback.
	// Mask interrupts so an interrupt landing mid-update cannot lose a count.
	u32 level = IRQ_Disable();

	int updated = unplayed + diff;

	if(updated < 0) {
		updated = 0;
	}
	else if(updated > BUFFERCOUNT) {
		updated = BUFFERCOUNT;
	}

	unplayed = updated;

	IRQ_Restore(level);
}

static void DMACallback () {
	if (!ScreenshotRequested && !ConfigRequested && dma_started) {
		updateUnplayed(-1);
		AUDIO_InitDMA((u32) soundbuffer[playab], AUDIOBUFFER);
		playab = nextIndex(playab);
	}
}

static void S9xAudioCallback (void *data) {
	// rateState persists between calls to give the controller hysteresis: a
	// correction is held until the queue recovers past the inner release band,
	// then released, which avoids hunting at the outer thresholds.
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

	double rate = RATE_NEUTRAL;
	if(rateState == RATE_STATE_DRAINING) {
		rate = RATE_SLOW_DOWN;
	}
	else if(rateState == RATE_STATE_FILLING) {
		rate = RATE_SPEED_UP;
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
	}
	else if(S9xGetSampleCount() >= SAMPLES_TO_PROCESS) {
		// Invariant: never write into a buffer the DMA has not played yet.
		// The ring is full when every buffer is still queued (unplayed ==
		// BUFFERCOUNT). When full we drop this cycle's mixed samples, which
		// applies backpressure to the emulator. The dynamic-rate control above
		// keeps the queue off this ceiling during normal play, so this is a
		// rare, glitch-free safety valve; in turbo/fast-forward the producer
		// runs ahead and intentionally rides here, dropping surplus audio
		// instead of stalling.
		if(unplayed < BUFFERCOUNT) {
			S9xMixSamples(soundbuffer[nextab], SAMPLES_TO_PROCESS);
			DCFlushRange(soundbuffer[nextab], AUDIOBUFFER);
			updateUnplayed(1);
			nextab = nextIndex(nextab);

			if(!dma_started && unplayed > UNPLAYED_START_LEVEL) {
				// Prime the AI DMA registers with the first queued buffer before
				// starting the transfer. AUDIO_StartDMA alone would replay
				// whatever address/length was last programmed (undefined memory
				// on a fresh start); DMACallback only re-arms after a transfer
				// completes, so the very first buffer must be armed here. DMA is
				// still stopped at this point, so DMACallback cannot race playab.
				//
				// This start check lives inside the "ring has room" block, which
				// is safe only because UNPLAYED_START_LEVEL (2) is far below
				// BUFFERCOUNT (16) and the queue grows one buffer per cycle: we
				// always start at unplayed == START_LEVEL + 1, long before the
				// ring could fill while still stopped. Keep that margin if these
				// constants change, or playback could stall on a full, unstarted
				// ring.
				updateUnplayed(-1);
				AUDIO_InitDMA((u32) soundbuffer[0], AUDIOBUFFER);
				playab = nextIndex(0);
				AUDIO_StartDMA();
				dma_started = true;
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
	unplayed = 0;
	nextab = 0;
	playab = 0;
	dma_started = false;
	rateState = RATE_STATE_NEUTRAL;
}

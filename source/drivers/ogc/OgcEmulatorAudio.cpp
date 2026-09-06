/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcEmulatorAudio.cpp
 ***************************************************************************/
#include <ogc/audio.h>
#include <ogc/cache.h>
#include <unistd.h>
#include <string.h>

#include "OgcEmulatorAudio.h"
#include "../../snes9xgx.h"
#include "../../snes9x/apu/apu.h"

/*** Double buffered audio ***/
#define SAMPLES_TO_PROCESS 1024

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

// Maximum allowed queued buffers (12 out of 16).
// Leaves a mandatory 4-buffer (~85ms) safety zone before playab.
#define MAX_QUEUED_BUFFERS 12

// The single OgcEmulatorAudio instance currently registered with the DMA and
// Snes9x sample-ready trampolines below. There is only ever one emulator
// audio backend alive at a time.
static OgcEmulatorAudio* instance = nullptr;

void AudioDMACallback() {
	if (instance)
		instance->dmaCallback();
}

void S9xAudioCallback(void *data) {
	if (instance)
		instance->audioCallback();
}

OgcEmulatorAudio::OgcEmulatorAudio() :
	playab(0), nextab(0), dma_started(false), turbo_drop(false), rateState(RATE_STATE_NEUTRAL)
{
	memset(soundbuffer, 0, sizeof(soundbuffer));
	memset(dummy, 0, sizeof(dummy));
	instance = this;
}

OgcEmulatorAudio::~OgcEmulatorAudio() {
	if (instance == this)
		instance = nullptr;
}

void OgcEmulatorAudio::init() {
}

void OgcEmulatorAudio::dmaCallback() {
	AUDIO_InitDMA((uint32_t) soundbuffer[playab], AUDIOBUFFER);
	playab = nextIndex(playab);
}

void OgcEmulatorAudio::resetAudio() {
	nextab = 0;
	playab = 0;
	dma_started = false;
	turbo_drop = false;
	rateState = RATE_STATE_NEUTRAL;
}

void OgcEmulatorAudio::audioCallback() {
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

	if (appRequest == AppRequest::MENU) {
		// Stop playback while the screenshot/config overlay is active. Reset the
		// ring so that once the request clears, the start path below re-primes
		// and restarts DMA cleanly instead of leaving playback dead on a stale,
		// never-rearmed buffer index.
		AUDIO_StopDMA();
		resetAudio();
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
				AUDIO_InitDMA((uint32_t) soundbuffer[0], AUDIOBUFFER);
				playab = nextIndex(0);
				AUDIO_StartDMA();
				dma_started = true;
			}
		} else {
			// Buffer ring is full (Turbo mode running faster than real-time playback).
			// Safely drop excess samples to keep APU emulation moving.
			S9xMixSamples(dummy, SAMPLES_TO_PROCESS);
		}
	}
}

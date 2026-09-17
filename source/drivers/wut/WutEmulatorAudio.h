/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * WutEmulatorAudio.h
 *
 * EmulatorAudioDriver implementation for Wii U: an AX (sndcore2)
 * callback-driven ring buffer, built on AX voices. Owns a dedicated pair of
 * AXVoice (L/R, hard-panned) separate from WutAudioDriver's menu SFX/stream 
 * voices - via voice acquisition.
 *
 * AX voices simply read forward through the looped ring on their own. 
 * There is only one callback here: just audioCallback(), which both mixes 
 * new samples in (S9x-driven, "push" side) and lazily arms/starts the voices
 * once enough is buffered via started/armAndStartVoices().
 *
 * Foreground/background handling (WHBProcIsRunning() etc.) is not done
 * here - that's a WutAudioDriver::startEmulatorAudio()/stopEmulatorAudio()
 * mode-switch concern, same as it is for OgcEmulatorAudio's counterpart.
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <sndcore2/voice.h>
#include "../EmulatorAudioDriver.h"

// Snes9x sample-ready callback trampoline, registered with
// S9xSetSamplesAvailableCallback() which requires a bare C function pointer.
void S9xAudioCallback(void *data);

class WutEmulatorAudio : public EmulatorAudioDriver
{
	public:
		WutEmulatorAudio();
		~WutEmulatorAudio() override;

		void init() override;
		void resetAudio() override;

		//! No-op. Voices are armed lazily by audioCallback() itself once enough have queued.
		void start() {};

		//! Hard-stops both AX voices (AX_VOICE_STATE_STOPPED).
		void stop();

		// Called only via the S9xAudioCallback trampoline above.
		void audioCallback();

	private:
		void armAndStartVoices();
		int getUnplayedBuffers() const { return queuedFrames / CHUNK_FRAMES; }
		void syncQueuedFrames();

		// Chosen to match OgcEmulatorAudio's SAMPLES_TO_PROCESS/BUFFERCOUNT
		// exactly (512 stereo frames per chunk, 16 chunks of ring capacity)
		// so the dynamic-rate watermarks below carry over unchanged.
		static constexpr int CHUNK_FRAMES = 512;
		static constexpr int CHUNK_SAMPLES = CHUNK_FRAMES * 2; // interleaved L+R
		static constexpr int BUFFERCOUNT = 16;
		static constexpr int RING_FRAMES = CHUNK_FRAMES * BUFFERCOUNT;
		static_assert((RING_FRAMES % CHUNK_FRAMES) == 0, "RING_FRAMES must be an exact multiple of CHUNK_FRAMES");

		AXVoice* voiceL;
		AXVoice* voiceR;

		alignas(32) int16_t ringL[RING_FRAMES];
		alignas(32) int16_t ringR[RING_FRAMES];

		// Scratch for S9xMixSamples: interleaved LR PCM in, then
		// deinterleaved into ringL/ringR above. dummy[] is where samples
		// get mixed-and-discarded when the ring is full in turbo mode.
		uint8_t mixScratch[CHUNK_SAMPLES * 2] __attribute__ ((__aligned__ (32)));
		uint8_t dummy[CHUNK_SAMPLES * 2] __attribute__ ((__aligned__ (32)));

		// writeOffset is always advanced by exactly CHUNK_FRAMES, which
		// evenly divides RING_FRAMES, so a chunk write never has to wrap.
		uint32_t writeOffset;
		bool started;
		bool turboDrop;

		// Authoritative, software-owned count of frames queued ahead of the
		// hardware playhead, always clamped to [0, RING_FRAMES]. Deliberately
		// NOT derived by subtracting the hardware's current read position from
		// writeOffset on every call.
		uint32_t queuedFrames;
		uint32_t lastHwFrame;

		// Same discrete rate-controller states as OgcEmulatorAudio, for
		// the same reason (hysteresis compares enums, not floats).
		enum RateState {
			RATE_STATE_NEUTRAL,
			RATE_STATE_DRAINING,  // running slow to shrink an over-full queue
			RATE_STATE_FILLING,   // running fast to grow an under-full queue
		};
		RateState rateState;
};

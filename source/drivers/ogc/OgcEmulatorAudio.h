/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcEmulatorAudio.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include "../EmulatorAudioDriver.h"

// Hardware DMA callback trampoline, registered with AUDIO_RegisterDMACallback()
// which requires a bare C function pointer.
void AudioDMACallback();

// Snes9x sample-ready callback trampoline, registered with
// S9xSetSamplesAvailableCallback() which requires a bare C function pointer.
void S9xAudioCallback(void *data);

class OgcEmulatorAudio : public EmulatorAudioDriver
{
	public:
		OgcEmulatorAudio();
		~OgcEmulatorAudio() override;

		void init() override;
		void resetAudio() override;

		// Called only via the AudioDMACallback/S9xAudioCallback trampolines above.
		void dmaCallback();
		void audioCallback();

	private:
		// BUFFERCOUNT must be a power of two so the ring index can advance with
		// a cheap bitwise mask (see nextIndex) instead of an integer modulo on
		// the hot path.
		static constexpr int BUFFERCOUNT = 16;
		static constexpr int AUDIOBUFFER = 2048;

		// BUFFERCOUNT must be a power of two so the ring index can advance
		// with a cheap bitwise mask (nextIndex) instead of an integer modulo.
		static_assert((BUFFERCOUNT & (BUFFERCOUNT - 1)) == 0, "BUFFERCOUNT must be a power of two");

		static int nextIndex(int current) { return (current + 1) & (BUFFERCOUNT - 1); }
		int getUnplayed() const { return (nextab - playab + BUFFERCOUNT) & (BUFFERCOUNT - 1); }

		uint8_t soundbuffer[BUFFERCOUNT][AUDIOBUFFER] __attribute__ ((__aligned__ (32)));
		uint8_t dummy[AUDIOBUFFER] __attribute__ ((__aligned__ (32)));

		// Shared between audioCallback() and the DMA interrupt callback,
		// so they must not be cached in registers across reads
		volatile int playab;
		volatile int nextab;
		bool dma_started;
		bool turbo_drop;

		// Discrete state of the dynamic-rate controller. Kept separate from the
		// rate multiplier so the hysteresis logic compares enums rather than
		// floating-point values (exact, and robust against future tweaks to
		// the multipliers). Only touched by audioCallback() (non-interrupt
		// context), so it needs no synchronization.
		enum RateState {
			RATE_STATE_NEUTRAL,
			RATE_STATE_DRAINING,  // running slow to shrink an over-full queue
			RATE_STATE_FILLING,   // running fast to grow an under-full queue
		};
		RateState rateState;
};

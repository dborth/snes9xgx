/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutAudioDriver.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <sndcore2/core.h>
#include <sndcore2/voice.h>
#include "WutEmulatorAudio.h"
#include "../AudioDriver.h"
#include "../../libgui/GuiSoundOggPlayer.h"

//!Wii U AudioDriver: AX (sndcore2), 16 fixed AXVoice slots for one-shots
//!plus a dedicated stereo streaming path (two AXVoices, ring-buffered)
//!fed by a GuiSoundOggPlayer for the background stream.
class WutAudioDriver : public AudioDriver
{
	public:
		void init() override;
		void startMenuAudio() override;
		void startEmulatorAudio() override;
		void shutdown() override;

		WutEmulatorAudio* getEmulatorAudio() override { return emulatorAudio; }

		int32_t playVoice(const uint8_t* data, int32_t length, int volume) override;
		void stopVoice(int32_t voice) override;
		void pauseVoice(int32_t voice) override;
		void resumeVoice(int32_t voice) override;
		bool isVoicePlaying(int32_t voice) override;
		void setVoiceVolume(int32_t voice, int volume) override;

		void playStream(const uint8_t* data, int32_t length, bool loop, int volume) override;
		void stopStream() override;
		void pauseStream() override;
		void resumeStream() override;
		bool isStreamPlaying() override;
		void setStreamVolume(int volume) override;

		//!AX frame callback hook that refills the stream ring buffers.
		//!Not part of the AudioDriver interface.
		void handleStreamCallback();

	private:
		struct WutVoiceSlot {
			AXVoice* voice;
			bool active;
		};

		WutEmulatorAudio* emulatorAudio = nullptr;
		GuiSoundOggPlayer oggPlayer;
		WutVoiceSlot voices[16];
		int nextVoiceSlot;

		AXVoice* streamVoiceL;
		AXVoice* streamVoiceR;

		// Circular Ring Buffer (16,384 samples = 32KB per channel)
		static const uint32_t STREAM_BUFFER_SAMPLES = 16384;
		alignas(32) int16_t streamBufL[STREAM_BUFFER_SAMPLES];
		alignas(32) int16_t streamBufR[STREAM_BUFFER_SAMPLES];

		uint32_t writeOffset;
		bool eofSilenceWritten;
		int streamVolume;
};

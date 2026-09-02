#pragma once

#include <stdint.h>
#include <ogc/audio.h>
#include "OgcEmulatorAudio.h"
#include "../AudioDriver.h"
#include "../../snes9x/apu/apu.h"

class GameCubeAudioDriver : public AudioDriver
{
	public:
		void init() override { AUDIO_Init(NULL); AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ); AUDIO_RegisterDMACallback(AudioDMACallback); }
		void startMenuAudio() override { S9xSetSamplesAvailableCallback(NULL, NULL); AUDIO_StopDMA(); }
		void startEmulatorAudio() override { AudioReset(); S9xSetSamplesAvailableCallback(S9xAudioCallback, NULL); }
		void shutdown() override { AUDIO_StopDMA(); AUDIO_RegisterDMACallback(NULL); }

		int32_t playVoice(const uint8_t* data, int32_t length, int volume) override { return -1; }
		void stopVoice(int32_t voice) override {}
		void pauseVoice(int32_t voice) override {}
		void resumeVoice(int32_t voice) override {}
		bool isVoicePlaying(int32_t voice) override { return false; }
		void setVoiceVolume(int32_t voice, int volume) override {}

		void playStream(const uint8_t* data, int32_t length, bool loop, int volume) override {}
		void stopStream() override {}
		void pauseStream() override {}
		void resumeStream() override {}
		bool isStreamPlaying() override { return false; }
		void setStreamVolume(int volume) override {}
};

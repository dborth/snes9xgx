#pragma once

#include <stdint.h>
#include <ogc/audio.h>
#include "../AudioDriver.h"

class OgcNullAudioDriver : public AudioDriver
{
	public:
		void init() override { AUDIO_Init(NULL); AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ); }
		void start() override {}
		void stop() override {}
		void shutdown() override {}

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

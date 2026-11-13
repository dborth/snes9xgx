/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * GameCubeAudioDriver.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <ogc/audio.h>
#include "../OgcEmulatorAudio.h"
#include "../../AudioDriver.h"
#include "../../../snes9x/apu/apu.h"

class GameCubeAudioDriver : public AudioDriver
{
	public:
		~GameCubeAudioDriver() override { delete emulatorAudio; }

		void init() override { AUDIO_Init(NULL); AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ); AUDIO_RegisterDMACallback(AudioDMACallback); emulatorAudio = new OgcEmulatorAudio(); emulatorAudio->init(); }
		void startMenuAudio() override {}
		void stopMenuAudio() override {}
		void startEmulatorAudio() override {}
		void stopEmulatorAudio() override { emulatorAudio->stopAudio(); }
		void shutdown() override { AUDIO_StopDMA(); AUDIO_RegisterDMACallback(NULL); }

		OgcEmulatorAudio* getEmulatorAudio() override { return emulatorAudio; }

		int32_t playVoice(const uint8_t*, int32_t, int) override { return -1; }
		void stopVoice(int32_t) override {}
		void pauseVoice(int32_t) override {}
		void resumeVoice(int32_t) override {}
		bool isVoicePlaying(int32_t) override { return false; }
		void setVoiceVolume(int32_t, int) override {}

		void playStream(const uint8_t*, int32_t, bool, int) override {}
		void stopStream() override {}
		void pauseStream() override {}
		void resumeStream() override {}
		bool isStreamPlaying() override { return false; }
		void setStreamVolume(int) override {}

	private:
		OgcEmulatorAudio* emulatorAudio = nullptr;
};

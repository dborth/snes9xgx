/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcAudioDriver.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include "../AudioDriver.h"
#include "../../libgui/GuiSoundOggPlayer.h"

class OgcAudioDriver : public AudioDriver
{
	public:
		void init() override;
		void shutdown() override;
		void start() override;
		void stop() override;

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

		// Hardware callback hook
		void handleStreamCallback(int voice);

	private:
		GuiSoundOggPlayer oggPlayer;
		int streamVolume;
};

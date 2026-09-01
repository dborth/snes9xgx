/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * OgcAudioDriver.cpp
 ***************************************************************************/
#include <ogcsys.h>
#include <asndlib.h>
#include <unistd.h>

#include "OgcAudioDriver.h"

static OgcAudioDriver *instance = nullptr;

static void stream_callback(int voice) {
	if (instance)
		instance->handleStreamCallback(voice);
}

void OgcAudioDriver::handleStreamCallback(int voice) {
	if (voice != 0 || !oggPlayer.isPlaying() || oggPlayer.isPaused())
		return;

	int32_t size = 0;
	const uint8_t *buf = oggPlayer.getReadyBuffer(&size);

	if (buf && size > 0) {
		if (ASND_AddVoice(0, (void*) buf, size) == 0) {
			oggPlayer.consumeBuffer();
		}
	}
}

void OgcAudioDriver::init() {
	instance = this;
	ASND_Init();
	streamVolume = 127;
}

void OgcAudioDriver::start() {
	DSP_Unhalt();
	ASND_Init();
	ASND_Pause(0);
}

void OgcAudioDriver::stop() {
	ASND_Pause(1);
	ASND_End();
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
	DSP_Halt();
}

void OgcAudioDriver::shutdown() {
	stopStream();
	ASND_Pause(1);
	ASND_End();
	AUDIO_StopDMA();
	instance = nullptr;
}

int32_t OgcAudioDriver::playVoice(const uint8_t *data, int32_t length, int volume) {
	int32_t voice = ASND_GetFirstUnusedVoice();
	if (voice >= 0)
		ASND_SetVoice(voice, VOICE_STEREO_16BIT, 48000, 0, (uint8_t*) data, length, volume, volume, nullptr);
	return voice;
}

void OgcAudioDriver::stopVoice(int32_t voice) {
	ASND_StopVoice(voice);
}
void OgcAudioDriver::pauseVoice(int32_t voice) {
	ASND_PauseVoice(voice, 1);
}
void OgcAudioDriver::resumeVoice(int32_t voice) {
	ASND_PauseVoice(voice, 0);
}

bool OgcAudioDriver::isVoicePlaying(int32_t voice) {
	return ASND_StatusVoice(voice) == SND_WORKING || ASND_StatusVoice(voice) == SND_WAITING;
}

void OgcAudioDriver::setVoiceVolume(int32_t voice, int volume) {
	ASND_ChangeVolumeVoice(voice, volume, volume);
}

void OgcAudioDriver::playStream(const uint8_t *data, int32_t length, bool loop, int volume) {
	stopStream();
	streamVolume = volume;

	if (oggPlayer.play(data, length, 0, loop)) {
		int32_t size = 0;
		const uint8_t *buf = nullptr;

		// Block momentarily until the worker thread decodes the first buffer chunk
		while (oggPlayer.isPlaying() && !(buf = oggPlayer.getReadyBuffer(&size))) {
			usleep(1000);
		}

		if (buf && size > 0) {
			int format =(oggPlayer.getChannels() == 2) ? VOICE_STEREO_16BIT : VOICE_MONO_16BIT;
			ASND_SetVoice(0, format, oggPlayer.getSampleRate(), 0, (void*) buf, size, streamVolume, streamVolume, stream_callback);
			oggPlayer.consumeBuffer();
		}
	}
}

void OgcAudioDriver::stopStream() {
	ASND_StopVoice(0);
	oggPlayer.stop();
}

void OgcAudioDriver::pauseStream() {
	ASND_PauseVoice(0, 1);
	oggPlayer.pause(true);
}

void OgcAudioDriver::resumeStream() {
	ASND_PauseVoice(0, 0);
	oggPlayer.pause(false);
}

bool OgcAudioDriver::isStreamPlaying() {
	return oggPlayer.isPlaying();
}

void OgcAudioDriver::setStreamVolume(int volume) {
	streamVolume = volume;
	ASND_ChangeVolumeVoice(0, volume, volume);
}

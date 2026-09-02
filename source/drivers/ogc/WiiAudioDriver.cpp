/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
 * WiiAudioDriver.cpp
 ***************************************************************************/
#include <ogc/dsp.h>
#include <ogc/audio.h>
#include <asndlib.h>
#include <unistd.h>

#include "WiiAudioDriver.h"
#include "OgcEmulatorAudio.h"

#include "snes9x/apu/apu.h"

static WiiAudioDriver *instance = nullptr;

void WiiAudioDriver::init() {
	instance = this;
	ASND_Init();
	streamVolume = 127;
}

void WiiAudioDriver::startEmulatorAudio() {
	stopMenuAudio();
	AUDIO_RegisterDMACallback(AudioDMACallback);
	S9xSetSamplesAvailableCallback(S9xAudioCallback, NULL);
}

void WiiAudioDriver::stopEmulatorAudio() {
	S9xSetSamplesAvailableCallback(NULL, NULL);
}

void WiiAudioDriver::startMenuAudio() {
	stopEmulatorAudio();
	DSP_Unhalt();
	ASND_Init();
	ASND_Pause(0);
}

void WiiAudioDriver::stopMenuAudio() {
	stopStream();
	ASND_Pause(1);
	ASND_End();
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
	DSP_Halt();
}

void WiiAudioDriver::shutdown() {
	stopStream();
	ASND_Pause(1);
	ASND_End();
	AUDIO_StopDMA();
	instance = nullptr;
}

int32_t WiiAudioDriver::playVoice(const uint8_t *data, int32_t length, int volume) {
	int32_t voice = ASND_GetFirstUnusedVoice();
	if (voice >= 0)
		ASND_SetVoice(voice, VOICE_STEREO_16BIT, 48000, 0, (uint8_t*) data, length, volume, volume, nullptr);
	return voice;
}

void WiiAudioDriver::stopVoice(int32_t voice) {
	ASND_StopVoice(voice);
}
void WiiAudioDriver::pauseVoice(int32_t voice) {
	ASND_PauseVoice(voice, 1);
}
void WiiAudioDriver::resumeVoice(int32_t voice) {
	ASND_PauseVoice(voice, 0);
}

bool WiiAudioDriver::isVoicePlaying(int32_t voice) {
	return ASND_StatusVoice(voice) == SND_WORKING || ASND_StatusVoice(voice) == SND_WAITING;
}

void WiiAudioDriver::setVoiceVolume(int32_t voice, int volume) {
	ASND_ChangeVolumeVoice(voice, volume, volume);
}

static void stream_callback(int voice) {
	if (instance)
		instance->handleStreamCallback(voice);
}

void WiiAudioDriver::handleStreamCallback(int voice) {
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

void WiiAudioDriver::playStream(const uint8_t *data, int32_t length, bool loop, int volume) {
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

void WiiAudioDriver::stopStream() {
	ASND_StopVoice(0);
	oggPlayer.stop();
}

void WiiAudioDriver::pauseStream() {
	ASND_PauseVoice(0, 1);
	oggPlayer.pause(true);
}

void WiiAudioDriver::resumeStream() {
	ASND_PauseVoice(0, 0);
	oggPlayer.pause(false);
}

bool WiiAudioDriver::isStreamPlaying() {
	return oggPlayer.isPlaying();
}

void WiiAudioDriver::setStreamVolume(int volume) {
	streamVolume = volume;
	ASND_ChangeVolumeVoice(0, volume, volume);
}

/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutAudioDriver.cpp
 ***************************************************************************/
#include <coreinit/cache.h>
#include <string.h>
#include <unistd.h>
#include <proc_ui/procui.h>

#include "../Platform.h"
#include "WutAudioDriver.h"

static WutAudioDriver *instance = nullptr;

// Once the OS has taken the foreground away from us (HOME menu overlay,
// forced exit, etc.) we stop feeding/starting audio rather than continuing
// to drive AX in the background.
static inline bool isForeground() { return platform->getStatus() == Status::Running; }

static void wut_frame_callback() {
	if (instance)
		instance->handleStreamCallback();
}

template <int N>
static void buildChannelMix(AXVoiceDeviceMixData (&mix)[N], bool left, bool right) {
	memset(mix, 0, sizeof(mix));
	if (left)
		mix[0].bus[0].volume = 0x8000;
	if (right && N > 1)
		mix[1].bus[0].volume = 0x8000;
}

static constexpr int AX_TV_CHANNELS = 6;
static constexpr int AX_DRC_CHANNELS = 4;

void WutAudioDriver::init() {
	instance = this;
	AXInitParams params = {};
	params.renderer = AX_INIT_RENDERER_48KHZ;
	params.pipeline = AX_INIT_PIPELINE_SINGLE;
	AXInitWithParams(&params);
	AXRegisterAppFrameCallback(wut_frame_callback);

	nextVoiceSlot = 0;

	// Pre-allocate the 16 hardware SFX voices - mono, centered equally on
	// both output channels of both devices.
	AXVoiceDeviceMixData tvMixCentered[AX_TV_CHANNELS];
	AXVoiceDeviceMixData drcMixCentered[AX_DRC_CHANNELS];
	buildChannelMix(tvMixCentered, true, true);
	buildChannelMix(drcMixCentered, true, true);

	for (int i = 0; i < 16; i++) {
		voices[i].voice = AXAcquireVoice(31, 0, 0);
		voices[i].active = false;
		if (voices[i].voice) {
			AXVoiceBegin(voices[i].voice);
			AXSetVoiceType(voices[i].voice, 0);

			AXSetVoiceDeviceMix(voices[i].voice, AX_DEVICE_TYPE_TV, 0, tvMixCentered);
			AXSetVoiceDeviceMix(voices[i].voice, AX_DEVICE_TYPE_DRC, 0, drcMixCentered);
			AXVoiceEnd(voices[i].voice);
		}
	}

	streamVoiceL = AXAcquireVoice(31, 0, 0);
	streamVoiceR = AXAcquireVoice(31, 0, 0);

	if (streamVoiceL && streamVoiceR) {
		AXVoiceDeviceMixData tvMixL[AX_TV_CHANNELS], drcMixL[AX_DRC_CHANNELS];
		AXVoiceDeviceMixData tvMixR[AX_TV_CHANNELS], drcMixR[AX_DRC_CHANNELS];
		buildChannelMix(tvMixL, true, false);   // Hard-pan Left
		buildChannelMix(drcMixL, true, false);
		buildChannelMix(tvMixR, false, true);   // Hard-pan Right
		buildChannelMix(drcMixR, false, true);

		AXVoiceBegin(streamVoiceL);
		AXSetVoiceType(streamVoiceL, 0);
		AXSetVoiceDeviceMix(streamVoiceL, AX_DEVICE_TYPE_TV, 0, tvMixL);
		AXSetVoiceDeviceMix(streamVoiceL, AX_DEVICE_TYPE_DRC, 0, drcMixL);
		AXVoiceEnd(streamVoiceL);

		AXVoiceBegin(streamVoiceR);
		AXSetVoiceType(streamVoiceR, 0);
		AXSetVoiceDeviceMix(streamVoiceR, AX_DEVICE_TYPE_TV, 0, tvMixR);
		AXSetVoiceDeviceMix(streamVoiceR, AX_DEVICE_TYPE_DRC, 0, drcMixR);
		AXVoiceEnd(streamVoiceR);
	}

	streamVolume = 255;
	writeOffset = 0;
	eofSilenceWritten = false;

	emulatorAudio = new WutEmulatorAudio();
	emulatorAudio->init();
}

WutAudioDriver::~WutAudioDriver() {
	delete emulatorAudio;
}

void WutAudioDriver::startMenuAudio() {

}

void WutAudioDriver::stopMenuAudio() {
	stopStream();

	for (int i = 0; i < 16; i++)
		stopVoice(i);
}

void WutAudioDriver::startEmulatorAudio() {
	emulatorAudio->resetAudio();
	emulatorAudio->start();
}

void WutAudioDriver::stopEmulatorAudio() {
	emulatorAudio->stop();
}

void WutAudioDriver::shutdown() {
	stopEmulatorAudio();
	stopStream();
	AXDeregisterAppFrameCallback(wut_frame_callback);

	for (int i = 0; i < 16; i++) {
		if (voices[i].voice) {
			AXFreeVoice(voices[i].voice);
			voices[i].voice = nullptr;
		}
	}

	if (streamVoiceL) { AXFreeVoice(streamVoiceL); streamVoiceL = nullptr; }
	if (streamVoiceR) { AXFreeVoice(streamVoiceR); streamVoiceR = nullptr; }

	AXQuit();
	instance = nullptr;
}

int32_t WutAudioDriver::playVoice(const uint8_t *data, int32_t length, int volume) {
	if (!isForeground())
		return -1;

	int voiceIdx = nextVoiceSlot;
	nextVoiceSlot = (nextVoiceSlot + 1) % 16;

	WutVoiceSlot &slot = voices[voiceIdx];
	if (!slot.voice)
		return -1;

	AXSetVoiceState(slot.voice, 0); // Stop current if active
	slot.active = true;

	AXVoiceOffsets offsets;
	memset(&offsets, 0, sizeof(offsets));
	offsets.data = data;
	offsets.dataType = AX_VOICE_FORMAT_LPCM16;
	offsets.loopingEnabled = AX_VOICE_LOOP_DISABLED;
	offsets.endOffset = length / 2; // Conversion: bytes to 16-bit samples
	AXSetVoiceOffsets(slot.voice, &offsets);

	AXVoiceSrc src;
	memset(&src, 0, sizeof(src));
	uint32_t samplesPerSec = AXGetInputSamplesPerSec();
	src.ratio = (uint32_t)(0x00010000 * ((float) 48000 / (float) samplesPerSec));
	AXSetVoiceSrc(slot.voice, &src);
	AXSetVoiceSrcType(slot.voice, (src.ratio == 0x00010000) ? 0 : 1); // SRC bypass check

	AXVoiceVeData veData;
	veData.volume = (volume << 7); // Remap 0-255 to ~0-0x8000
	veData.delta = 0;
	AXSetVoiceVe(slot.voice, &veData);

	AXSetVoiceState(slot.voice, 1);
	return voiceIdx;
}

void WutAudioDriver::stopVoice(int32_t voice) {
	if (voice >= 0 && voice < 16 && voices[voice].voice) {
		AXSetVoiceState(voices[voice].voice, 0);
		voices[voice].active = false;
	}
}

void WutAudioDriver::pauseVoice(int32_t voice) {
	if (voice >= 0 && voice < 16 && voices[voice].voice)
		AXSetVoiceState(voices[voice].voice, 0);
}

void WutAudioDriver::resumeVoice(int32_t voice) {
	if (voice >= 0 && voice < 16 && voices[voice].voice)
		AXSetVoiceState(voices[voice].voice, 1);
}

bool WutAudioDriver::isVoicePlaying(int32_t voice) {
	if (voice >= 0 && voice < 16 && voices[voice].voice)
		return voices[voice].active && (voices[voice].voice->state == AX_VOICE_STATE_PLAYING);
	return false;
}

void WutAudioDriver::setVoiceVolume(int32_t voice, int volume) {
	if (voice >= 0 && voice < 16 && voices[voice].voice) {
		AXVoiceVeData veData;
		veData.volume = (volume << 7);
		veData.delta = 0;
		AXSetVoiceVe(voices[voice].voice, &veData);
	}
}

void WutAudioDriver::playStream(const uint8_t *data, int32_t length, bool loop, int volume) {
	// Don't gate this on isForeground(): the very first call (eg. the
	// startup bg_music track) can race Cafe OS's foreground-acquire signal,
	// and bailing out here silently drops it with nothing to ever retry it.
	// Instead we always prime the buffers/decoder below, and let
	// handleStreamCallback() (ticking every AX frame, ~3ms) continuously
	// reconcile the hardware voice state against isForeground() - the same
	// self-healing pattern WutEmulatorAudio::playSound() already uses for
	// the emulator's ring buffer voice.
	stopStream();
	streamVolume = volume;

	memset(streamBufL, 0, sizeof(streamBufL));
	memset(streamBufR, 0, sizeof(streamBufR));
	DCFlushRange(streamBufL, sizeof(streamBufL));
	DCFlushRange(streamBufR, sizeof(streamBufR));

	writeOffset = 0;
	eofSilenceWritten = false;

	if (oggPlayer.play(data, length, 0, loop)) {
		int32_t readySize = 0;
		const uint8_t *pcm = nullptr;

		while (oggPlayer.isPlaying() && !(pcm = oggPlayer.getReadyBuffer(&readySize))) {
			usleep(1000);
		}

		if (pcm && readySize > 0) {
			int samples = readySize / (oggPlayer.getChannels() == 2 ? 4 : 2);
			int16_t *pcm16 = (int16_t*) pcm;

			if (oggPlayer.getChannels() == 2) {
				for (int i = 0; i < samples; i++) {
					streamBufL[i] = pcm16[i * 2];
					streamBufR[i] = pcm16[i * 2 + 1];
				}
			} else {
				for (int i = 0; i < samples; i++) {
					streamBufL[i] = pcm16[i];
					streamBufR[i] = pcm16[i];
				}
			}

			DCFlushRange(streamBufL, samples * 2);
			DCFlushRange(streamBufR, samples * 2);
			writeOffset = samples;
			oggPlayer.consumeBuffer();

			AXSetVoiceState(streamVoiceL, 0);
			AXSetVoiceState(streamVoiceR, 0);

			AXVoiceOffsets offsetsL, offsetsR;
			memset(&offsetsL, 0, sizeof(offsetsL));
			offsetsL.data = streamBufL;
			offsetsL.dataType = AX_VOICE_FORMAT_LPCM16;
			offsetsL.loopingEnabled = AX_VOICE_LOOP_ENABLED;
			offsetsL.loopOffset = 0;
			offsetsL.endOffset = STREAM_BUFFER_SAMPLES - 1;
			AXSetVoiceOffsets(streamVoiceL, &offsetsL);

			offsetsR = offsetsL;
			offsetsR.data = streamBufR;
			AXSetVoiceOffsets(streamVoiceR, &offsetsR);

			AXVoiceSrc src;
			memset(&src, 0, sizeof(src));
			uint32_t samplesPerSec = AXGetInputSamplesPerSec();
			src.ratio = (uint32_t)(0x00010000 * ((float) oggPlayer.getSampleRate() / (float) samplesPerSec));
			AXSetVoiceSrc(streamVoiceL, &src);
			AXSetVoiceSrc(streamVoiceR, &src);

			uint16_t srcType = (src.ratio == 0x00010000) ? 0 : 1;
			AXSetVoiceSrcType(streamVoiceL, srcType);
			AXSetVoiceSrcType(streamVoiceR, srcType);

			AXVoiceVeData veData;
			veData.volume = (streamVolume << 7);
			veData.delta = 0;
			AXSetVoiceVe(streamVoiceL, &veData);
			AXSetVoiceVe(streamVoiceR, &veData);

			AXSetVoiceState(streamVoiceL, 1);
			AXSetVoiceState(streamVoiceR, 1);
		}
	}
}

void WutAudioDriver::handleStreamCallback() {
	if (!isForeground()) {
		// Lost (or don't yet have) the foreground - hold the hardware voices stopped directly
		if (streamVoiceL) AXSetVoiceState(streamVoiceL, 0);
		if (streamVoiceR) AXSetVoiceState(streamVoiceR, 0);
		return;
	}

	// We have the foreground. If there's an active, not-explicitly-paused stream whose hardware voices aren't running,
	// (re)start them here. Fully self-healing.
	if (streamVoiceL && streamVoiceR && oggPlayer.isPlaying() && !oggPlayer.isPaused() && streamVoiceL->state != AX_VOICE_STATE_PLAYING) {
		AXSetVoiceState(streamVoiceL, 1);
		AXSetVoiceState(streamVoiceR, 1);
	}

	if (!streamVoiceL || !streamVoiceR || oggPlayer.isPaused() || streamVoiceL->state != AX_VOICE_STATE_PLAYING)
		return;

	// Query exactly where the hardware is right now. streamBuf is the required base pointer.
	uint32_t currentOffset = AXGetVoiceCurrentOffsetEx(streamVoiceL, streamBufL);
	
	// Calculate how many valid, unplayed samples are queued ahead of the hardware playhead
	uint32_t queuedSamples = (writeOffset - currentOffset + STREAM_BUFFER_SAMPLES) % STREAM_BUFFER_SAMPLES;

	if (queuedSamples < (STREAM_BUFFER_SAMPLES / 2)) {
		int32_t readySize = 0;
		const uint8_t *pcm = oggPlayer.getReadyBuffer(&readySize);

		if (pcm && readySize > 0) {
			int samples = readySize / (oggPlayer.getChannels() == 2 ? 4 : 2);

			// Handle physical wrap-around if the new chunk crosses the end of streamBuf
			int samplesToEnd = STREAM_BUFFER_SAMPLES - writeOffset;
			int write1 = (samples < samplesToEnd) ? samples : samplesToEnd;
			int write2 = samples - write1;
			int16_t *pcm16 = (int16_t*) pcm;

			auto copySamples = [&](int dest, int src, int count) {
				if (oggPlayer.getChannels() == 2) {
					for (int i = 0; i < count; i++) {
						streamBufL[dest + i] = pcm16[(src + i) * 2];
						streamBufR[dest + i] = pcm16[(src + i) * 2 + 1];
					}
				} else {
					for (int i = 0; i < count; i++) {
						streamBufL[dest + i] = pcm16[src + i];
						streamBufR[dest + i] = pcm16[src + i];
					}
				}
			};

			if (write1 > 0) copySamples(writeOffset, 0, write1);
			if (write2 > 0) copySamples(0, write1, write2);

			if (write1 > 0) {
				DCFlushRange(&streamBufL[writeOffset], write1 * 2);
				DCFlushRange(&streamBufR[writeOffset], write1 * 2);
			}
			if (write2 > 0) {
				DCFlushRange(&streamBufL[0], write2 * 2);
				DCFlushRange(&streamBufR[0], write2 * 2);
			}

			writeOffset = (writeOffset + samples) % STREAM_BUFFER_SAMPLES;
			oggPlayer.consumeBuffer();

		} else if (!oggPlayer.isPlaying()) {

			// Natural End-Of-File reached. Scrub the unwritten remainder of the buffer with silence
			// so the DSP doesn't play old loop garbage before halting.
			if (!eofSilenceWritten) {
				uint32_t clearCount = STREAM_BUFFER_SAMPLES - queuedSamples;
				if (clearCount > 0) {
					uint32_t clear1 = (clearCount < (STREAM_BUFFER_SAMPLES - writeOffset)) ? clearCount : (STREAM_BUFFER_SAMPLES - writeOffset);
					uint32_t clear2 = clearCount - clear1;

					memset(&streamBufL[writeOffset], 0, clear1 * 2);
					memset(&streamBufR[writeOffset], 0, clear1 * 2);
					DCFlushRange(&streamBufL[writeOffset], clear1 * 2);
					DCFlushRange(&streamBufR[writeOffset], clear1 * 2);

					if (clear2 > 0) {
						memset(&streamBufL[0], 0, clear2 * 2);
						memset(&streamBufR[0], 0, clear2 * 2);
						DCFlushRange(&streamBufL[0], clear2 * 2);
						DCFlushRange(&streamBufR[0], clear2 * 2);
					}
				}
				eofSilenceWritten = true;
			}

			// Once the hardware finishes playing the very last valid samples, halt it.
			if (queuedSamples < 128) {
				AXSetVoiceState(streamVoiceL, 0);
				AXSetVoiceState(streamVoiceR, 0);
			}
		}
	}
}

void WutAudioDriver::stopStream() {
	oggPlayer.stop();
	if (streamVoiceL) AXSetVoiceState(streamVoiceL, 0);
	if (streamVoiceR) AXSetVoiceState(streamVoiceR, 0);
}

void WutAudioDriver::pauseStream() {
	oggPlayer.pause(true);
	if (streamVoiceL) AXSetVoiceState(streamVoiceL, 0);
	if (streamVoiceR) AXSetVoiceState(streamVoiceR, 0);
}

void WutAudioDriver::resumeStream() {
	if (streamVoiceL) AXSetVoiceState(streamVoiceL, 1);
	if (streamVoiceR) AXSetVoiceState(streamVoiceR, 1);
	oggPlayer.pause(false);
}

bool WutAudioDriver::isStreamPlaying() {
	return oggPlayer.isPlaying();
}

void WutAudioDriver::setStreamVolume(int volume) {
	streamVolume = volume;
	if (streamVoiceL && streamVoiceR) {
		AXVoiceVeData veData;
		veData.volume = (volume << 7);
		veData.delta = 0;
		AXSetVoiceVe(streamVoiceL, &veData);
		AXSetVoiceVe(streamVoiceR, &veData);
	}
}

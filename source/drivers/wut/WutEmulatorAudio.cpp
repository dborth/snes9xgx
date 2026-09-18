/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * WutEmulatorAudio.cpp
 ***************************************************************************/
#include <coreinit/cache.h>
#include <sndcore2/core.h>
#include <string.h>

#include "WutEmulatorAudio.h"
#include "../../snes9xgx.h"
#include "../../snes9x/apu/apu.h"

// Dynamic-rate control: nudge the emulated sample rate up or down slightly to
// keep the number of unplayed (queued) buffers within a comfortable band,
// preventing both buffer underruns (audio gaps) and overruns (added latency).
// Identical constants/hysteresis to OgcEmulatorAudio - only the mechanism
// used to measure "unplayed" differs (hardware voice playhead vs. a DMA
// IRQ-advanced index).
#define UNPLAYED_HIGH_WATER 8       // above this we are building latency, slow down
#define UNPLAYED_HIGH_RELEASE 6     // stay slow until the queue drains back to here
#define UNPLAYED_LOW_RELEASE 6      // stay fast until the queue fills back to here
#define UNPLAYED_LOW_WATER 4        // below this we risk an underrun, speed up
#define UNPLAYED_START_LEVEL 4      // queue at least this many buffers before starting voices
#define RATE_SLOW_DOWN 1.005        // emit samples slightly slower to drain the queue
#define RATE_SPEED_UP 0.995         // emit samples slightly faster to fill the queue
#define RATE_NEUTRAL 1.0

// Maximum allowed queued buffers (12 out of 16).
// Leaves a mandatory 4-buffer safety zone ahead of the hardware playhead.
#define MAX_QUEUED_BUFFERS 12

// The single WutEmulatorAudio instance currently registered with the
// S9x sample-ready trampoline below. There is only ever one emulator
// audio backend alive at a time.
static WutEmulatorAudio* instance = nullptr;

void S9xAudioCallback(void *data) {
	if (instance)
		instance->audioCallback();
}

WutEmulatorAudio::WutEmulatorAudio() :
	voiceL(nullptr), voiceR(nullptr), writeOffset(0), started(false), turboDrop(false),
	queuedFrames(0), lastHwFrame(0), rateState(RATE_STATE_NEUTRAL)
{
	memset(ringL, 0, sizeof(ringL));
	memset(ringR, 0, sizeof(ringR));
	memset(dummy, 0, sizeof(dummy));
	instance = this;
}

WutEmulatorAudio::~WutEmulatorAudio() {
	if (voiceL) AXFreeVoice(voiceL);
	if (voiceR) AXFreeVoice(voiceR);
	if (instance == this)
		instance = nullptr;
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

void WutEmulatorAudio::init() {
	voiceL = AXAcquireVoice(31, 0, 0);
	voiceR = AXAcquireVoice(31, 0, 0);

	AXVoiceDeviceMixData tvMixL[AX_TV_CHANNELS], drcMixL[AX_DRC_CHANNELS];
	AXVoiceDeviceMixData tvMixR[AX_TV_CHANNELS], drcMixR[AX_DRC_CHANNELS];
	buildChannelMix(tvMixL, true, false);   // Hard-pan Left
	buildChannelMix(drcMixL, true, false);
	buildChannelMix(tvMixR, false, true);   // Hard-pan Right
	buildChannelMix(drcMixR, false, true);

	if (voiceL) {
		AXVoiceBegin(voiceL);
		AXSetVoiceType(voiceL, 0);

		AXSetVoiceDeviceMix(voiceL, AX_DEVICE_TYPE_TV, 0, tvMixL);
		AXSetVoiceDeviceMix(voiceL, AX_DEVICE_TYPE_DRC, 0, drcMixL);

		AXVoiceVeData veData;
		veData.volume = 0x8000;
		veData.delta = 0;
		AXSetVoiceVe(voiceL, &veData);

		AXVoiceEnd(voiceL);
	}

	if (voiceR) {
		AXVoiceBegin(voiceR);
		AXSetVoiceType(voiceR, 0);

		AXSetVoiceDeviceMix(voiceR, AX_DEVICE_TYPE_TV, 0, tvMixR);
		AXSetVoiceDeviceMix(voiceR, AX_DEVICE_TYPE_DRC, 0, drcMixR);

		AXVoiceVeData veData;
		veData.volume = 0x8000;
		veData.delta = 0;
		AXSetVoiceVe(voiceR, &veData);

		AXVoiceEnd(voiceR);
	}

	S9xSetSamplesAvailableCallback(S9xAudioCallback, NULL);
	resetAudio();
}

void WutEmulatorAudio::resetAudio() {
	if (voiceL) { AXSetVoiceState(voiceL, AX_VOICE_STATE_STOPPED); AXSetVoiceCurrentOffset(voiceL, 0); }
	if (voiceR) { AXSetVoiceState(voiceR, AX_VOICE_STATE_STOPPED); AXSetVoiceCurrentOffset(voiceR, 0); }

	writeOffset = 0;
	started = false;
	turboDrop = false;
	queuedFrames = 0;
	lastHwFrame = 0;
	rateState = RATE_STATE_NEUTRAL;

	memset(ringL, 0, sizeof(ringL));
	memset(ringR, 0, sizeof(ringR));
	DCFlushRange(ringL, sizeof(ringL));
	DCFlushRange(ringR, sizeof(ringR));
}

void WutEmulatorAudio::stop() {
	if (voiceL) AXSetVoiceState(voiceL, AX_VOICE_STATE_STOPPED);
	if (voiceR) AXSetVoiceState(voiceR, AX_VOICE_STATE_STOPPED);
	started = false;
}

/****************************************************************************
 * armAndStartVoices
 *
 * Lazily called the first time enough is queued to start
 * ringL/ringR are fixed-address, fixed-size buffers, so offsets/src only
 * need to be (re-)established here, not on every mix.
 ***************************************************************************/
void WutEmulatorAudio::armAndStartVoices() {
	if (!voiceL || !voiceR)
		return;

	// The oldest sample still queued: playback must resume from here
	uint32_t startFrame = (writeOffset + RING_FRAMES - (queuedFrames % RING_FRAMES)) % RING_FRAMES;

	AXVoiceOffsets offsets;
	memset(&offsets, 0, sizeof(offsets));
	offsets.dataType = AX_VOICE_FORMAT_LPCM16;
	offsets.loopingEnabled = AX_VOICE_LOOP_ENABLED;
	offsets.loopOffset = 0;
	offsets.endOffset = RING_FRAMES - 1;
	offsets.currentOffset = startFrame;

	offsets.data = ringL;
	AXSetVoiceOffsets(voiceL, &offsets);
	offsets.data = ringR;
	AXSetVoiceOffsets(voiceR, &offsets);

	AXVoiceSrc src;
	memset(&src, 0, sizeof(src));
	uint32_t axRate = AXGetInputSamplesPerSec();
	src.ratio = (uint32_t) (0x00010000 * ((float) Settings.SoundPlaybackRate / (float) axRate));
	AXSetVoiceSrc(voiceL, &src);
	AXSetVoiceSrc(voiceR, &src);

	uint16_t srcType = (src.ratio == 0x00010000) ? 0 : 1; // SRC bypass check
	AXSetVoiceSrcType(voiceL, srcType);
	AXSetVoiceSrcType(voiceR, srcType);

	// queuedFrames is left exactly as it already was - it's already correct
	lastHwFrame = startFrame;

	AXSetVoiceState(voiceL, AX_VOICE_STATE_PLAYING);
	AXSetVoiceState(voiceR, AX_VOICE_STATE_PLAYING);
}

/****************************************************************************
 * syncQueuedFrames
 *
 * Refreshes queuedFrames (the authoritative "how much is unplayed" count)
 * against the hardware's actual read position, once voiceL has started.
 * Before that point queuedFrames is simply carried forward by every write
 * below (writeOffset and queuedFrames advance by the same CHUNK_FRAMES
 * each time), whatever its starting value already was - 0 after a fresh
 * resetAudio(), or a preserved backlog after a plain resume.
 *
 * Deliberately computed as a bounded per-call *delta* off the previous
 * hardware position, then clamped to what we know is queued - not as a
 * raw (writeOffset - currentFrame) subtraction.
 ***************************************************************************/
void WutEmulatorAudio::syncQueuedFrames() {
	if (!started || !voiceL)
		return;

	uint32_t hwFrame = AXGetVoiceCurrentOffsetEx(voiceL, ringL);
	uint32_t consumed = (hwFrame - lastHwFrame + RING_FRAMES) % RING_FRAMES;

	if (consumed > queuedFrames)
		consumed = queuedFrames;

	queuedFrames -= consumed;
	lastHwFrame = hwFrame;
}

void WutEmulatorAudio::audioCallback() {
	syncQueuedFrames();
	int unplayed = getUnplayedBuffers();
	double rate = RATE_NEUTRAL;

	if (Settings.TurboMode) {
		rateState = RATE_STATE_NEUTRAL;

		// fast-forward hysteresis:
		// If dropping, keep dropping until the queue drains to 4.
		// If capturing, keep capturing until the queue fills to 12.
		if (turboDrop && unplayed <= UNPLAYED_LOW_WATER) {
			turboDrop = false;
		} else if (!turboDrop && unplayed >= MAX_QUEUED_BUFFERS) {
			turboDrop = true;
		}
	} else {
		turboDrop = false; // Always reset when leaving turbo

		if (rateState == RATE_STATE_DRAINING) {
			if (unplayed <= UNPLAYED_HIGH_RELEASE) {
				rateState = RATE_STATE_NEUTRAL;
			}
		}
		else if (rateState == RATE_STATE_FILLING) {
			if (unplayed >= UNPLAYED_LOW_RELEASE) {
				rateState = RATE_STATE_NEUTRAL;
			}
		}

		if (unplayed > UNPLAYED_HIGH_WATER) {
			rateState = RATE_STATE_DRAINING;
		}
		else if (unplayed < UNPLAYED_LOW_WATER) {
			rateState = RATE_STATE_FILLING;
		}

		if (rateState == RATE_STATE_DRAINING) {
			rate = RATE_SLOW_DOWN;
		}
		else if (rateState == RATE_STATE_FILLING) {
			rate = RATE_SPEED_UP;
		}
	}

	S9xUpdateDynamicRate(rate);
	S9xFinalizeSamples();

	while (S9xGetSampleCount() >= CHUNK_SAMPLES) {
		unplayed = getUnplayedBuffers();

		if (!turboDrop && unplayed < MAX_QUEUED_BUFFERS) {
			S9xMixSamples(mixScratch, CHUNK_SAMPLES);

			const int16_t* interleaved = reinterpret_cast<const int16_t*>(mixScratch);
			for (int i = 0; i < CHUNK_FRAMES; i++) {
				ringL[writeOffset + i] = interleaved[i * 2];
				ringR[writeOffset + i] = interleaved[i * 2 + 1];
			}

			DCFlushRange(&ringL[writeOffset], CHUNK_FRAMES * sizeof(int16_t));
			DCFlushRange(&ringR[writeOffset], CHUNK_FRAMES * sizeof(int16_t));

			writeOffset = (writeOffset + CHUNK_FRAMES) % RING_FRAMES;
			queuedFrames += CHUNK_FRAMES;

			// Handle initial voice pre-roll / priming
			if (!started && getUnplayedBuffers() >= UNPLAYED_START_LEVEL) {
				armAndStartVoices();
				started = true;
			}
		} else {
			// Ring is full (Turbo mode running faster than real-time playback).
			// Safely drop excess samples to keep APU emulation moving.
			S9xMixSamples(dummy, CHUNK_SAMPLES);
		}
	}
}

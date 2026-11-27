/****************************************************************************
 * msu1audio.h
 *
 * MSU-1 audio source interface.
 *
 * msu1.cpp is format-agnostic: it asks MSU1Prefetch for 44.1 kHz stereo
 * frames and does not know where they come from. A track is served by an
 * MSU1AudioDecoder; there is one implementation per file format:
 *
 *   msu1pcm.cpp   .pcm  ("MSU1" header + raw 16-bit LE stereo)
 *
 * To add a format: implement MSU1AudioDecoder, add an Open function, and add
 * one line to S9xMSU1OpenAudioDecoder() in msu1.cpp. Nothing else changes -
 * prefetching, looping, seeking, savestates and the menu suspend/resume all
 * work on frames.
 *
 * Threading contract (this is what makes decoders safe to write):
 *  - A decoder is created on the emulation thread, then handed to MSU1Prefetch
 *    with setSource(). From that moment ONE thread (the prefetch thread) calls
 *    read()/seek()/loopFrame() and finally deletes it. The emulation thread
 *    never touches it again.
 *  - Because read() runs on the prefetch thread it may block on storage, and
 *    it may be CPU-heavy without hurting emulation.
 *  - Decoders need no locking and must not call into the emulator.
 ***************************************************************************/
#ifndef _MSU1AUDIO_H_
#define _MSU1AUDIO_H_

#include <stdint.h>

// The MSU-1 FILE rate. Decoders deliver this rate and nothing else. It is NOT the output rate:
// apu.cpp converts it afterwards (msu::resampler, Hermite) to Settings.SoundPlaybackRate (48 kHz)
// Prefetching and decoding all happen on the 44.1 kHz side, before that resampler.
#define MSU1_SAMPLE_RATE 44100

class MSU1AudioDecoder
{
	public:
		virtual ~MSU1AudioDecoder() {}

		//! Decode up to `frames` frames into dst: interleaved stereo int16 in
		//! NATIVE endianness at MSU1_SAMPLE_RATE (convert/resample here, off
		//! the emulation thread). Returns the number of frames delivered.
		//! Delivering fewer than requested means end of stream (or an
		//! unrecoverable read error); the prefetcher then loops or stops.
		virtual uint32_t read(int16_t * dst, uint32_t frames) = 0;

		//! Reposition so the next read() returns `frame` (0 = first audio
		//! frame of the track). Returns false if it cannot.
		virtual bool seek(uint32_t frame) = 0;

		//! Frame at which a repeating track restarts. Only consulted at end
		//! of stream; values >= the stream length are treated as 0.
		virtual uint32_t loopFrame() const = 0;
};

//! The one place that decides which decoder serves a track (defined in
//! msu1.cpp). Returns NULL if the track does not exist or is invalid.
MSU1AudioDecoder * S9xMSU1OpenAudioDecoder(uint16_t track);

//! PCM decoder (msu1pcm.cpp).
MSU1AudioDecoder * S9xMSU1OpenPcmDecoder(uint16_t track);

#endif

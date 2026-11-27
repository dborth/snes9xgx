/****************************************************************************
 * msu1prefetch.h
 *
 * Asynchronous read-ahead for MSU-1 audio.
 *
 * A worker thread owns the track's MSU1AudioDecoder and keeps a ring of
 * big blocks (BLOCK_FRAMES frames = 32 KB each) filled ahead of playback. The
 * emulation thread only copies frames out of memory and NEVER waits: if the
 * ring runs dry, pop() reports POP_BUFFERING and the caller outputs silence
 * until the pre-roll is refilled.
 *
 * Format-agnostic: it moves frames, not bytes. Looping, end of stream and
 * seeking all happen in frame units
 ***************************************************************************/
#ifndef _MSU1PREFETCH_H_
#define _MSU1PREFETCH_H_

#include <stdint.h>
#include <new>
#include <type_traits>
#include "msu1audio.h"
#include "drivers/ThreadDriver.h"
#include "drivers/Mutex.h"
#include "drivers/Cond.h"

class MSU1Prefetch
{
	public:
		enum
		{
			BLOCK_FRAMES   = 8192,	// 32 KB per block
			PREROLL_BLOCKS = 2		// blocks needed before (re)starting output
		};

		enum PopResult
		{
			POP_FRAME,		//!< a frame was returned
			POP_BUFFERING,	//!< no data yet: output silence, do not advance time-in-track
			POP_ENDED		//!< track ended and is not repeating; stream rewound to the start
		};

		constexpr MSU1Prefetch() {}

		//! One-time setup (idempotent): allocate the ring, create the sync
		//! objects, start the worker.
		//! Falls back to synchronous operation if the thread cannot be
		//! started; returns false only if the memory cannot be allocated.
		bool start(uint32_t blockCount);

		//! Full teardown. May wait for one in-flight read.
		void shutdown();

		bool running() const { return active; }
		bool isThreaded() const { return threaded; }

		//! Hand over a decoder (ownership passes to the prefetcher) and start
		//! prefetching at startFrame. NULL drops the current source. The
		//! previous decoder is closed by the worker, not by the caller.
		void setSource(MSU1AudioDecoder * decoder, uint32_t startFrame);

		//! Drop the source AND wait until the worker has closed its file, so the
		//! caller may hand the device to something else. Waits for at most one
		//! in-flight read.
		void detach();

		//! Reposition within the current source (flushes read-ahead).
		void seek(uint32_t frame);

		//! Whether the track repeats at end of stream. Only takes the lock when
		//! the value changes, so it is cheap to call every frame.
		void setRepeat(bool repeat);

		bool hasSource() const { return sourceSet; }

		//! Next stereo frame, never blocks. See PopResult.
		inline PopResult pop(int16_t & left, int16_t & right)
		{
			if (cur && curPos < curFrames)
			{
				const int16_t * p = cur->data + (curPos << 1);
				left = p[0];
				right = p[1];
				curPos++;
				posFrame = curStart + curPos;
				return POP_FRAME;
			}
			return popSlow(left, right);
		}

		//! Frame index of the next frame pop() will return (byte position in
		//! a .pcm file is 8 + 4 * position()).
		uint32_t position() const { return posFrame; }

		// ---- diagnostics (consumer thread) ----
		uint32_t readyBlocks() const;
		uint32_t blocksTotal() const { return blockCount; }
		uint32_t starvations() const { return starveCount; }

	private:
		enum { END_NONE = 0, END_LOOPED = 1, END_EOF = 2, END_ERROR = 3 };	// END_ERROR: nothing could be read/positioned; never retried

		struct Block
		{
			int16_t * data;
			uint32_t  frames;
			uint32_t  startFrame;	// frame index of data[0]
			uint32_t  loopTo;		// END_EOF: where a repeat would restart
			uint8_t   end;			// END_*
		};

		PopResult popSlow(int16_t & left, int16_t & right);
		PopResult finishEnded();
		bool takeBlock();
		void flushLocked();

		bool workAvailableLocked() const;
		bool workOnce();
		void commitBlock(uint32_t myGen, uint32_t frames, uint32_t start, uint8_t end, uint32_t loopTo, bool terminal);

		void run();
		static void * threadEntry(void * arg);
		static void wakeThunk();

		// ---- set by start(), constant afterwards ----
		bool active = false, threaded = false;
		Mutex * mutex = nullptr;			// the one lock
		Cond * workCond = nullptr;			// broadcast-only: every waiter re-checks its predicate
		Thread * thread = nullptr;
		alignas(8) unsigned char mutexStore[sizeof(Mutex)] = {};	// sync objects are built in place
		alignas(8) unsigned char condStore[sizeof(Cond)] = {};
		alignas(8) unsigned char threadStore[sizeof(Thread)] = {};
		uint8_t * ringRaw = nullptr;
		int16_t * ring = nullptr;
		Block * blocks = nullptr;
		uint32_t blockCount = 0;

		// ---- guarded by *mutex ----
		uint32_t readyCount = 0, holding = 0, rdIdx = 0, wrIdx = 0, gen = 0;
		bool cmdPending = false, replaceDecoder = false, seekPending = false;
		bool readerEof = false, needPreroll = true, busy = false;
		bool repeatShared = false;			// consumer writes (setRepeat), worker reads
		MSU1AudioDecoder * pendingDecoder = nullptr;
		uint32_t pendingSeek = 0;

		// ---- worker-owned ----
		MSU1AudioDecoder * dec = nullptr;
		bool decEof = false;
		uint32_t pos = 0, sinceSeek = 0;

		// ---- consumer-owned ----
		Block * cur = nullptr;
		uint32_t curPos = 0, curFrames = 0, curStart = 0, posFrame = 0;
		bool sourceSet = false, repeatCached = false;
		uint32_t starveCount = 0;

		static MSU1Prefetch * wakeTarget;
};

#endif

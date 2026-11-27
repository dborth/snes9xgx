/****************************************************************************
 * msu1prefetch.cpp
 ***************************************************************************/
#include "msu1prefetch.h"

#include <stdlib.h>

#ifndef MSU1_PF_ALLOC
#include "memmanager.h"
#if defined(HW_RVL)
#define MSU1_PF_ALLOC(n)	extmem_malloc(n)
#define MSU1_PF_FREE(p)		extmem_free(p)
#else
#define MSU1_PF_ALLOC(n)	malloc(n)
#define MSU1_PF_FREE(p)		free(p)
#endif
#endif

#define MSU1_PF_STACK 16384
#define MSU1_PF_PRIORITY ThreadPriority::Normal

MSU1Prefetch * MSU1Prefetch::wakeTarget = nullptr;

// ---------------------------------------------------------------------------
// lifetime
// ---------------------------------------------------------------------------

bool MSU1Prefetch::start(uint32_t count)
{
	if (active)
		return true;
	if (count < PREROLL_BLOCKS + 2)
		count = PREROLL_BLOCKS + 2;

	// One allocation: [block descriptors, padded to 32 bytes][ring]. The ring starts on a cache-line
	// boundary and is a multiple of 32 bytes, so a DMA read into a block shares no line with anything.
	const size_t descBytes = (((size_t)count * sizeof(Block)) + 31) & ~(size_t)31;
	const size_t ringBytes = (size_t)count * BLOCK_FRAMES * 2 * sizeof(int16_t);
	ringRaw = (uint8_t *)MSU1_PF_ALLOC(descBytes + ringBytes + 31);
	if (!ringRaw)
		return false;

	uint8_t * base = (uint8_t *)(((uintptr_t)ringRaw + 31) & ~(uintptr_t)31);
	blocks = (Block *)base;
	ring = (int16_t *)(base + descBytes);
	blockCount = count;
	for (uint32_t i = 0; i < count; i++)
	{
		blocks[i].data = ring + (size_t)i * BLOCK_FRAMES * 2;
		blocks[i].frames = blocks[i].startFrame = blocks[i].loopTo = 0;
		blocks[i].end = END_NONE;
	}

	mutex = new (mutexStore) Mutex;
	workCond = new (condStore) Cond;
	thread = new (threadStore) Thread;

	readyCount = holding = rdIdx = wrIdx = 0;
	cmdPending = replaceDecoder = seekPending = readerEof = busy = false;
	needPreroll = true;
	repeatShared = repeatCached;
	pendingDecoder = nullptr;
	dec = nullptr; decEof = false; pos = sinceSeek = 0;
	cur = nullptr; curPos = curFrames = curStart = posFrame = 0;
	sourceSet = false; starveCount = 0;
	active = true;

	wakeTarget = this;
	threaded = thread->start(threadEntry, this, MSU1_PF_STACK, MSU1_PF_PRIORITY, wakeThunk);
	return true;
}

void MSU1Prefetch::shutdown()
{
	if (!active)
		return;

	if (threaded)
	{
		thread->requestStop();
		thread->join();
	}

	// The worker is gone: everything is ours again.
	delete dec;
	dec = nullptr;
	delete pendingDecoder;
	pendingDecoder = nullptr;

	thread->~Thread();
	workCond->~Cond();
	mutex->~Mutex();
	thread = nullptr; workCond = nullptr; mutex = nullptr;

	MSU1_PF_FREE(ringRaw);
	ringRaw = nullptr; ring = nullptr; blocks = nullptr;

	cur = nullptr; curPos = curFrames = 0;
	readyCount = holding = 0;
	sourceSet = false;
	threaded = false;
	active = false;
	if (wakeTarget == this)
		wakeTarget = nullptr;
}

// ---------------------------------------------------------------------------
// consumer side
// ---------------------------------------------------------------------------

uint32_t MSU1Prefetch::readyBlocks() const
{
	if (!active)
		return 0;
	MutexLock lock(*mutex);
	return readyCount;
}

void MSU1Prefetch::setRepeat(bool r)
{
	if (r == repeatCached)
		return;
	repeatCached = r;
	if (!active)
		return;		// start() copies repeatCached
	MutexLock lock(*mutex);
	repeatShared = r;
}

void MSU1Prefetch::flushLocked()
{
	gen++;
	readyCount = 0;
	holding = 0;
	rdIdx = wrIdx;			// an in-flight fill (if any) is discarded by its generation check
	readerEof = false;
	needPreroll = true;
	cur = nullptr;
	curPos = curFrames = 0;
}

void MSU1Prefetch::setSource(MSU1AudioDecoder * d, uint32_t startFrame)
{
	if (!active)
	{
		delete d;
		return;
	}

	MSU1AudioDecoder * discard;
	{
		MutexLock lock(*mutex);
		discard = pendingDecoder;	// a hand-off the worker never picked up: still ours to close
		pendingDecoder = d;
		replaceDecoder = true;
		seekPending = (d != nullptr);
		pendingSeek = startFrame;
		cmdPending = true;
		flushLocked();
		posFrame = startFrame;
		workCond->signal();
	}
	delete discard;		// never under the lock: closing a file is I/O
	sourceSet = (d != nullptr);

	if (!threaded)
		workOnce();		// no worker: apply the command now (closes the old file promptly)
}

void MSU1Prefetch::detach()
{
	if (!active)
		return;

	setSource(nullptr, 0);

	if (!threaded)
		return;			// setSource() already applied it synchronously

	// Wait until the worker has applied the command and is idle: the file is closed when we return.
	MutexLock lock(*mutex);
	while ((cmdPending || busy) && thread->isRunning())
		workCond->wait(*mutex);
}

void MSU1Prefetch::seek(uint32_t frame)
{
	if (!active)
		return;

	{
		MutexLock lock(*mutex);
		seekPending = true;
		pendingSeek = frame;
		cmdPending = true;
		flushLocked();
		posFrame = frame;
		workCond->signal();
	}

	if (!threaded)
		workOnce();		// no worker: reposition now
}

bool MSU1Prefetch::takeBlock()
{
	for (;;)
	{
		{
			MutexLock lock(*mutex);

			if (cur)	// release the block we just finished: its slot is free again
			{
				cur = nullptr;
				holding = 0;
				workCond->signal();
			}

			uint32_t need = needPreroll ? (uint32_t)PREROLL_BLOCKS : 1;
			if (readyCount >= need || (readyCount > 0 && readerEof))
			{
				needPreroll = false;
				cur = &blocks[rdIdx];
				rdIdx = (rdIdx + 1) % blockCount;
				readyCount--;
				holding = 1;
				curPos = 0;
				curFrames = cur->frames;
				curStart = cur->startFrame;
				return true;
			}

			if (readyCount == 0 && !needPreroll)
			{
				needPreroll = true;		// ran dry mid-stream: re-buffer before resuming
				starveCount++;
			}

			if (threaded || !workAvailableLocked())
				return false;
		}

		// No worker thread: do the read here (blocking - the pre-prefetch behaviour, in 32 KB chunks).
		workOnce();
	}
}

MSU1Prefetch::PopResult MSU1Prefetch::finishEnded()
{
	seek(0);	// like the old rewind to the start: the next Play begins instantly from a full ring
	posFrame = 0;
	return POP_ENDED;
}

MSU1Prefetch::PopResult MSU1Prefetch::popSlow(int16_t & left, int16_t & right)
{
	if (!active)
		return POP_BUFFERING;

	for (;;)
	{
		if (cur)
		{
			if (curPos < curFrames)
			{
				const int16_t * p = cur->data + (curPos << 1);
				left = p[0];
				right = p[1];
				curPos++;
				posFrame = curStart + curPos;
				return POP_FRAME;
			}

			// Block exhausted: look at how it ended.
			uint8_t end = cur->end;
			uint32_t loopTo = cur->loopTo;

			if (end == END_ERROR)
				return finishEnded();		// stop; the game sees the track end. Never retried by repeat.

			if (end == END_EOF)
			{
				if (repeatCached)
				{
					// The worker stopped at the end because repeat was off then; it is on now.
					seek(loopTo);
					return POP_BUFFERING;
				}
				return finishEnded();
			}

			if (end == END_LOOPED && !repeatCached)
			{
				// The worker kept going past the loop point because repeat was on; it was switched off since.
				return finishEnded();
			}
			// END_NONE, or END_LOOPED while still repeating: continue with the next block.
		}

		if (!takeBlock())
			return POP_BUFFERING;
	}
}

// ---------------------------------------------------------------------------
// worker side
// ---------------------------------------------------------------------------

bool MSU1Prefetch::workAvailableLocked() const
{
	if (cmdPending)
		return true;
	return dec != nullptr && !decEof && (readyCount + holding) < blockCount;
}

//! Publish a finished block (or discard it if a command made it stale) and mark the worker idle.
void MSU1Prefetch::commitBlock(uint32_t myGen, uint32_t frames, uint32_t start, uint8_t end, uint32_t loopTo, bool terminal)
{
	MutexLock lock(*mutex);
	busy = false;
	if (gen == myGen)		// else a seek/new source arrived while we were reading: the data is stale
	{
		Block & b = blocks[wrIdx];
		b.frames = frames;
		b.startFrame = start;
		b.loopTo = loopTo;
		b.end = end;
		wrIdx = (wrIdx + 1) % blockCount;
		readyCount++;
		if (terminal)
			readerEof = true;	// lets the consumer start with fewer than PREROLL_BLOCKS (short tracks)
	}
	workCond->signal();			// a detach() may be waiting for idle
}

bool MSU1Prefetch::workOnce()
{
	Block * blk = nullptr;
	uint32_t myGen;
	MSU1AudioDecoder * newDec = nullptr;
	bool replace = false, doSeek = false;
	uint32_t seekTo = 0;

	{
		MutexLock lock(*mutex);
		myGen = gen;
		if (cmdPending)
		{
			newDec = pendingDecoder;
			pendingDecoder = nullptr;
			replace = replaceDecoder;
			replaceDecoder = false;
			doSeek = seekPending;
			seekTo = pendingSeek;
			seekPending = false;
			cmdPending = false;
			busy = true;
		}
		else if (dec != nullptr && !decEof && (readyCount + holding) < blockCount)
		{
			blk = &blocks[wrIdx];
			busy = true;
		}
		else
			return false;
	}

	if (!blk)
	{
		// A command: swap decoder and/or reposition. Closing the old file happens here, off the emulation thread.
		if (replace)
		{
			delete dec;
			dec = newDec;
		}
		decEof = false;
		pos = 0;
		sinceSeek = 0;

		if (dec && doSeek && !dec->seek(seekTo))
		{
			decEof = true;	// cannot position: stop the track, and do not let a repeat retry it forever
			commitBlock(myGen, 0, seekTo, END_ERROR, 0, true);
			return true;
		}
		if (dec && doSeek)
			pos = seekTo;

		MutexLock lock(*mutex);
		busy = false;
		workCond->signal();
		return true;
	}

	// Fill one block. No lock is held across the read.
	uint32_t start = pos;
	uint32_t got = dec->read(blk->data, BLOCK_FRAMES);
	pos += got;
	sinceSeek += got;

	uint8_t end = END_NONE;
	uint32_t loopTo = 0;
	bool terminal = false;

	if (got < BLOCK_FRAMES)		// end of stream
	{
		uint32_t lf = dec->loopFrame();
		loopTo = (lf < pos) ? lf : 0;

		bool repeat;
		{
			MutexLock lock(*mutex);
			repeat = repeatShared;
		}

		if (sinceSeek == 0)
		{
			// Nothing at all could be read since the last (re)position: a failing device or an empty track
			end = END_ERROR;
			terminal = true;
			decEof = true;
		}
		else if (repeat && dec->seek(loopTo))
		{
			end = END_LOOPED;
			pos = loopTo;
			sinceSeek = 0;
		}
		else
		{
			end = END_EOF;
			terminal = true;
			decEof = true;
		}
	}

	commitBlock(myGen, got, start, end, loopTo, terminal);
	return true;
}

void MSU1Prefetch::run()
{
	for (;;)
	{
		{
			MutexLock lock(*mutex);
			while (!thread->stopRequested() && !workAvailableLocked())
				workCond->wait(*mutex);
			if (thread->stopRequested())
				break;
		}
		workOnce();
	}
}

void * MSU1Prefetch::threadEntry(void * arg)
{
	static_cast<MSU1Prefetch *>(arg)->run();
	return nullptr;
}

void MSU1Prefetch::wakeThunk()
{
	MSU1Prefetch * p = wakeTarget;
	if (p && p->mutex)
	{
		MutexLock lock(*p->mutex);
		p->workCond->signal();	// the stop flag was set before this call; the worker re-checks it under the lock
	}
}

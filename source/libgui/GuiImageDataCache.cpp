/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiImageDataCache.cpp
 ***************************************************************************/

#include <utility>
#include <new>

#include "GuiImageDataCache.h"
#include "../drivers/ThreadDriver.h"

namespace {
	enum class State : uint8_t { Queued, Decoded, Ready, Failed };

	struct Entry {
		const uint8_t * pngData = nullptr;
		int maxw = 0;
		int maxh = 0;
		State state = State::Queued;
		GuiImageData::DecodedImage pixels; // valid only while state == Decoded
		GuiImageData image;                // valid only while state == Ready
	};

	Entry * entries = nullptr;
	int entryCount = 0;
	bool workerDone = true;
	Thread worker;

	Mutex & cacheMutex()
	{
		static Mutex m;
		return m;
	}

	void * workerMain(void *)
	{
		Mutex & m = cacheMutex();

		for(int i = 0; ; i++)
		{
			const uint8_t * png = nullptr;
			int maxw = 0, maxh = 0;
			{
				MutexLock guard(m);
				if(worker.stopRequested() || i >= entryCount)
				{
					workerDone = true;
					break;
				}
				if(entries[i].state != State::Queued)
					continue;
				png = entries[i].pngData;
				maxw = entries[i].maxw;
				maxh = entries[i].maxh;
			}

			GuiImageData::DecodedImage decoded = GuiImageData::decodeToRgba(png, maxw, maxh);

			MutexLock guard(m);
			if(decoded.valid())
			{
				entries[i].pixels = std::move(decoded);
				entries[i].state = State::Decoded;
			}
			else
			{
				entries[i].state = State::Failed;
			}
		}
		return nullptr;
	}
}

void GuiImageDataCache::preload(const uint8_t * const * pngList, int count, int maxw, int maxh)
{
	if(!pngList || count <= 0)
		return;

	if(!entries)
	{
		entries = new(std::nothrow) Entry[GUI_TEXTURE_CACHE_MAX_ENTRIES];
		if(!entries)
			return;
	}

	Mutex & m = cacheMutex();
	bool needStart = false;
	{
		MutexLock guard(m);
		for(int i = 0; i < count; i++)
		{
			const uint8_t * p = pngList[i];
			if(!p)
				continue;

			bool known = false;
			for(int j = 0; j < entryCount && !known; j++)
				known = entries[j].pngData == p && entries[j].maxw == maxw && entries[j].maxh == maxh;
			if(known)
				continue;

			if(entryCount >= GUI_TEXTURE_CACHE_MAX_ENTRIES)
				break;

			entries[entryCount].pngData = p;
			entries[entryCount].maxw = maxw;
			entries[entryCount].maxh = maxh;
			entryCount++;
			needStart = true;
		}

		// A worker that hasn't finished will pick up what was just appended
		if(needStart && workerDone)
			workerDone = false;
		else
			needStart = false;
	}

	if(!needStart)
		return;

	if(worker.isRunning())
		worker.join(); // previous worker already finished; reap it before restarting

	if(!worker.start(workerMain, nullptr, 64 * 1024, ThreadPriority::Low))
	{
		MutexLock guard(m);
		workerDone = true;
	}
}

GuiImageData * GuiImageDataCache::tryGet(const uint8_t * pngData, int maxw, int maxh)
{
	if(!entries || !pngData)
		return nullptr;

	Mutex & m = cacheMutex();
	Entry * e = nullptr;
	GuiImageData::DecodedImage pixels;
	{
		MutexLock guard(m);
		for(int i = 0; i < entryCount; i++)
		{
			if(entries[i].pngData == pngData && entries[i].maxw == maxw && entries[i].maxh == maxh)
			{
				e = &entries[i];
				break;
			}
		}

		if(!e)
			return nullptr;
		if(e->state == State::Ready)
			return &e->image;
		if(e->state != State::Decoded)
			return nullptr;

		pixels = std::move(e->pixels);
	}

	// Main/GPU thread: the worker never touches an entry once it's Decoded
	bool ok = e->image.uploadDecoded(std::move(pixels));

	MutexLock guard(m);
	e->state = ok ? State::Ready : State::Failed;
	return ok ? &e->image : nullptr;
}

void GuiImageDataCache::shutdown()
{
	if(worker.isRunning())
	{
		worker.requestStop();
		worker.join();
	}

	delete[] entries; // ~GuiImageData frees each cached texture
	entries = nullptr;
	entryCount = 0;
	workerDone = true;
}

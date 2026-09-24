/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiImageAsyncCache.cpp
 ***************************************************************************/

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

#include "GuiImageAsyncCache.h"
#include "../drivers/Platform.h"
#include "../drivers/FileSystemDriver.h"

GuiImageAsyncCache::GuiImageAsyncCache(int capacityIn, int prefetchRadiusIn, int maxImageWidthIn, int maxImageHeightIn, unsigned int rawFileBufferSizeIn)
	: capacity(capacityIn < 1 ? 1 : capacityIn)
	, prefetchRadius(prefetchRadiusIn < 0 ? 0 : prefetchRadiusIn)
	, maxImageWidth(maxImageWidthIn)
	, maxImageHeight(maxImageHeightIn)
	, rawFileBufferSize(rawFileBufferSizeIn)
{
	cache.reset(new CacheSlot[capacity]);

	prefetchSlotCount = std::max(1, prefetchRadius * 2);
	prefetchSlots.reset(new PrefetchSlot[prefetchSlotCount]);

	rawFileBuffer = static_cast<uint8_t *>(malloc(rawFileBufferSize));
	if(!rawFileBuffer)
		return;

	// No wake callback: Thread::start()'s wake callback is a bare  void(*)(void)
	// with no userdata. Thread::JoinAll() alone cannot wake this thread out of 
	// workCond.wait() - this object MUST be shutdown() on any exit
	threadRunning = thread.start(threadTrampoline, this, 48 * 1024, ThreadPriority::Low, nullptr);
}

GuiImageAsyncCache::~GuiImageAsyncCache()
{
	shutdown();
}

void GuiImageAsyncCache::shutdown()
{
	if(threadRunning)
	{
		thread.requestStop();
		wake();
		thread.join();
		threadRunning = false;
	}

	if(rawFileBuffer)
	{
		free(rawFileBuffer);
		rawFileBuffer = nullptr;
	}

	flush();
}

void GuiImageAsyncCache::flush()
{
	{
		MutexLock guard(mutex);
		primaryPending = false;
		primaryIndex = -1;
		for(int i = 0; i < prefetchSlotCount; i++)
			prefetchSlots[i].used = false;
		completedReady = false;
		completedImage = GuiImageData::DecodedImage();
		completedIndex = -1;
		generation++; // any job already popped by the worker carries the old generation and will be discarded
	}

	for(int i = 0; i < capacity; i++)
	{
		cache[i].index = -1;
		cache[i].resolved = false;
		cache[i].hasImage = false;
		cache[i].image.clear();
	}
	lruClock = 0;
}

GuiImageAsyncCache::CacheSlot * GuiImageAsyncCache::findSlot(int index)
{
	for(int i = 0; i < capacity; i++)
		if(cache[i].index == index)
			return &cache[i];
	return nullptr;
}

GuiImageAsyncCache::CacheSlot * GuiImageAsyncCache::acquireSlotFor(int index)
{
	CacheSlot * existing = findSlot(index);
	if(existing)
		return existing;

	for(int i = 0; i < capacity; i++)
		if(cache[i].index == -1)
			return &cache[i];

	CacheSlot * victim = &cache[0];
	for(int i = 1; i < capacity; i++)
		if(cache[i].lastUsed < victim->lastUsed)
			victim = &cache[i];

	victim->index = -1;
	victim->resolved = false;
	victim->hasImage = false;
	return victim;
}

void GuiImageAsyncCache::update()
{
	int index;
	GuiImageData::DecodedImage decoded;

	{
		MutexLock guard(mutex);
		if(!completedReady)
			return;
		index = completedIndex;
		decoded = std::move(completedImage);
		completedReady = false;
	}

	CacheSlot * slot = acquireSlotFor(index);
	slot->index = index;
	slot->lastUsed = ++lruClock;
	slot->resolved = true;
	slot->hasImage = decoded.valid() && slot->image.uploadDecoded(std::move(decoded));
}

void GuiImageAsyncCache::request(int index, const char * path)
{
	if(!path || !path[0] || index < 0 || !threadRunning)
		return;

	CacheSlot * cached = findSlot(index);
	if(cached)
	{
		cached->lastUsed = ++lruClock; // keep actively-viewed cache entries from looking stale to the LRU
		return;
	}

	MutexLock guard(mutex);
	primaryPending = true;
	primaryIndex = index;
	primaryGeneration = generation;
	snprintf(primaryPath, sizeof(primaryPath), "%s", path);
	workCond.signal();
}

void GuiImageAsyncCache::prefetch(int index, const char * path)
{
	if(!path || !path[0] || index < 0 || !threadRunning || prefetchRadius <= 0)
		return;

	CacheSlot * cached = findSlot(index);
	if(cached)
	{
		cached->lastUsed = ++lruClock;
		return;
	}

	MutexLock guard(mutex);

	// already queued?
	for(int i = 0; i < prefetchSlotCount; i++)
		if(prefetchSlots[i].used && prefetchSlots[i].index == index)
			return;

	for(int i = 0; i < prefetchSlotCount; i++)
	{
		if(!prefetchSlots[i].used)
		{
			prefetchSlots[i].used = true;
			prefetchSlots[i].index = index;
			prefetchSlots[i].generation = generation;
			snprintf(prefetchSlots[i].path, sizeof(prefetchSlots[i].path), "%s", path);
			workCond.signal();
			return;
		}
	}
	// queue full - a speculative prefetch just isn't worth making room for
}

GuiImageData * GuiImageAsyncCache::get(int index)
{
	CacheSlot * slot = findSlot(index);
	if(!slot || !slot->resolved || !slot->hasImage)
		return nullptr;
	return &slot->image;
}

void GuiImageAsyncCache::wake()
{
	MutexLock guard(mutex);
	workCond.signal();
}

// ---------------------------------------------------------------------
// Raw file read. Takes FileSystemDriver::getIoLock() around the actual 
// mount+read. Note this only protects callers that also take the lock.
// ---------------------------------------------------------------------
static bool FindDeviceForPath(const char * path, int * outDevice)
{
	if(!path || !path[0])
		return false;

	StorageDevice devices[MAX_STORAGE_DEVICES];
	int count = platform->getFileSystem()->enumerateStorageDevices(devices);

	for(int i = 0; i < count; i++)
	{
		size_t len = strlen(devices[i].prefix); // eg. "sd:/" -> compare against "sd:"
		if(len > 1 && strncmp(path, devices[i].prefix, len - 1) == 0)
		{
			*outDevice = devices[i].id;
			return true;
		}
	}
	return false;
}

static size_t ReadFileForDecode(const char * path, uint8_t * buffer, size_t bufferSize)
{
	int device;
	if(!FindDeviceForPath(path, &device))
		return 0;

	MutexLock ioGuard(FileSystemDriver::getIoLock());

	if(platform->getFileSystem()->mountStorageDevice(device) != MountResult::Success)
		return 0;

	size_t size = 0;
	FILE * f = fopen(path, "rb");
	if(f)
	{
		fseeko(f, 0, SEEK_END);
		long fsize = ftello(f);
		fseeko(f, 0, SEEK_SET);

		if(fsize > 0 && static_cast<size_t>(fsize) <= bufferSize)
			size = fread(buffer, 1, fsize, f);

		fclose(f);
	}
	return size;
}

void * GuiImageAsyncCache::threadTrampoline(void * arg)
{
	static_cast<GuiImageAsyncCache *>(arg)->threadLoop();
	return nullptr;
}

void GuiImageAsyncCache::threadLoop()
{
	mutex.lock();
	while(!thread.stopRequested())
	{
		bool havePrimary = primaryPending;
		int prefetchPick = -1;
		if(!havePrimary)
		{
			for(int i = 0; i < prefetchSlotCount; i++)
			{
				if(prefetchSlots[i].used)
				{
					prefetchPick = i;
					break;
				}
			}
		}

		if(!havePrimary && prefetchPick < 0)
		{
			workCond.wait(mutex);
			continue;
		}

		int jobIndex;
		char jobPath[GUI_IMAGE_CACHE_MAX_PATH];
		uint32_t jobGeneration;

		if(havePrimary)
		{
			jobIndex = primaryIndex;
			jobGeneration = primaryGeneration;
			memcpy(jobPath, primaryPath, sizeof(jobPath));
			primaryPending = false;
		}
		else
		{
			jobIndex = prefetchSlots[prefetchPick].index;
			jobGeneration = prefetchSlots[prefetchPick].generation;
			memcpy(jobPath, prefetchSlots[prefetchPick].path, sizeof(jobPath));
			prefetchSlots[prefetchPick].used = false;
		}

		processingIndex = jobIndex;
		mutex.unlock();

		GuiImageData::DecodedImage decoded;
		size_t bytesRead = ReadFileForDecode(jobPath, rawFileBuffer, rawFileBufferSize);
		if(bytesRead > 0)
			decoded = GuiImageData::decodeToRgba(rawFileBuffer, maxImageWidth, maxImageHeight);

		mutex.lock();
		processingIndex = -1;

		if(jobGeneration == generation)
		{
			// Overwrites (and frees) any previous undrained result - if the main thread hasn't kept up,
			// that older result is stale anyway (a newer request already superseded it).
			completedIndex = jobIndex;
			completedImage = std::move(decoded);
			completedReady = true;
		}
		// else: this job belonged to a listing that's since been
		// flushed away - drop it silently (decoded's buffer frees itself).
	}
	mutex.unlock();
}

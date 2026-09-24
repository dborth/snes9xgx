/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiImageAsyncCache.h
 *
 * Generic background PNG loader + small LRU texture cache. Fully platform-
 * and app-agnostic: just "load the PNG at this path, keyed by this
 * integer id, off the main thread, and remember a few of them."
 *
 * ---- Threading model ----
 * A single background thread does the slow part: reading the file and
 * decoding the PNG into plain RGBA8 via GuiImageData::decodeToRgba() -
 * CPU-only, no GPU calls. update(), called once per frame from the main
 * thread, does the fast, bounded part: uploading any newly-decoded image
 * to a texture via GuiImageData::uploadDecoded().
 ***************************************************************************/
#pragma once

#include <memory>

#include "GuiImageData.h"
#include "../drivers/ThreadDriver.h"

//! Max length (including nul) of a path passed to request()/prefetch().
#define GUI_IMAGE_CACHE_MAX_PATH 256

class GuiImageAsyncCache
{
	public:
		//!\param capacity Number of decoded images kept cached at once
		//!(LRU-evicted beyond that). Must be >= 1.
		//!\param prefetchRadius How many neighboring indices on each side
		//!of the current selection prefetch() calls will actually be
		//!honored for; 0 disables prefetching outright (prefetch() calls
		//!become no-ops). Meaningless (and harmless) alongside capacity 1.
		//!\param maxImageWidth,maxImageHeight Decoded images are resized
		//!to fit within these bounds (0 = no limit on that axis), same as
		//!GuiImageData::decodeToRgba().
		//!\param rawFileBufferSize Size, in bytes, of the persistent
		//!buffer the background thread reads a source PNG file into
		//!before decoding.
		GuiImageAsyncCache(int capacity, int prefetchRadius, int maxImageWidth = 0, int maxImageHeight = 0,
		                    unsigned int rawFileBufferSize = 512 * 1024);
		~GuiImageAsyncCache();

		GuiImageAsyncCache(const GuiImageAsyncCache &) = delete;
		GuiImageAsyncCache & operator=(const GuiImageAsyncCache &) = delete;

		//!Stops the background thread and frees every cached texture and
		//!the raw read buffer. Safe to call more than once. The
		//!destructor calls this too, but exit paths that need the GPU
		//!torn down deterministically (rather than whenever this object's
		//!destructor happens to run) should call it explicitly.
		void shutdown();

		//!Drops every cached and in-flight entry (see class comment).
		void flush();

		//!Call once per frame, before get(). Cheap; does nothing most frames.
		//!Must be called from the main/GPU thread.
		void update();

		//!Requests the image for index, loaded from path. Cheap and non-blocking
		void request(int index, const char * path);

		//!Speculatively requests the image for index, loaded from path, at lower priority
		//! than request(). A no-op if this cache was constructed with prefetchRadius 0.
		void prefetch(int index, const char * path);

		//!\return the ready-to-display image for index, or nullptr if
		//!it isn't loaded yet (still decoding, not yet requested, or
		//!confirmed to have no image at that path). Never blocks. Main
		//!thread only.
		GuiImageData * get(int index);

		int getCapacity() const { return capacity; }
		int getPrefetchRadius() const { return prefetchRadius; }

	private:
		struct CacheSlot
		{
			int index = -1;        //!< -1 = empty
			bool resolved = false; //!< true once decode/upload has been attempted
			bool hasImage = false; //!< only meaningful if resolved
			uint32_t lastUsed = 0;
			GuiImageData image;
		};

		struct PrefetchSlot
		{
			bool used = false;
			int index = -1;
			uint32_t generation = 0;
			char path[GUI_IMAGE_CACHE_MAX_PATH];
		};

		CacheSlot * findSlot(int index);
		CacheSlot * acquireSlotFor(int index);

		static void * threadTrampoline(void * arg);
		static void wakeTrampoline(void * arg); //!< Thread wake callback (needs `this`)
		void threadLoop();
		void wake();

		const int capacity;
		const int prefetchRadius;
		const int maxImageWidth;
		const int maxImageHeight;
		const unsigned int rawFileBufferSize;

		std::unique_ptr<CacheSlot[]> cache;
		std::unique_ptr<PrefetchSlot[]> prefetchSlots; // size: max(1, prefetchRadius*2)
		int prefetchSlotCount;
		uint32_t lruClock = 0;

		// ---- everything below is guarded by sync.mutex ----
		Mutex mutex;
		Cond workCond;

		bool primaryPending = false;
		int primaryIndex = -1;
		uint32_t primaryGeneration = 0;
		char primaryPath[GUI_IMAGE_CACHE_MAX_PATH];

		int processingIndex = -1;
		uint32_t generation = 0;

		bool completedReady = false;
		int completedIndex = -1;
		GuiImageData::DecodedImage completedImage;
		// ---- end guarded members ----

		Thread thread;
		bool threadRunning = false;
		uint8_t * rawFileBuffer = nullptr;
};

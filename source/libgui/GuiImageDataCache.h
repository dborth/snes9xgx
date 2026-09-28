/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiImageDataCache.h
 *
 * Optional cache of decoded+uploaded GuiImageData PNG assets
 * Internal to GuiImageData: decodeImage() asks tryGet() before
 * doing any real work, and on a hit adopts the cached texture as a
 * non-owned view instead of decoding+uploading a private copy. Callers
 * never see or reference this class.
 *
 * Key is the asset's PNG source pointer (+ maxw/maxh): bin2o'd assets are
 * static const arrays with a stable, unique address for the process life.
 * Never evicts - the working set is the app's own fixed asset list.
 ***************************************************************************/
#pragma once

#include "GuiImageData.h"

//!Max distinct (pngData, maxw, maxh) entries the cache can hold.
#define GUI_TEXTURE_CACHE_MAX_ENTRIES 128

class GuiImageDataCache
{
	public:
		//!Queues pngList[0..count) for background decode. Returns immediately.
		//!Duplicates skipped. Call from the main thread.
		static void preload(const uint8_t * const * pngList, int count, int maxw = 0, int maxh = 0);

		//!Internal (GuiImageData::decodeImage); main/GPU thread only.
		//!Returns the resident texture for the key, uploading it now if it has been decoded
		//!but not yet uploaded, or nullptr if it isn't available
		static GuiImageData * tryGet(const uint8_t * pngData, int maxw, int maxh);

		//!Stops the worker if still running and frees every cached texture and pixel buffer.
		//!Call before the video driver is torn down.
		static void shutdown();
};

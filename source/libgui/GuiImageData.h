/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiImageData.h
 ***************************************************************************/
#pragma once

#include <memory>
#include <cstdlib>

#include "../drivers/Mutex.h"

//!Decodes compressed image data (PNG) into a platform-native texture created
//!from it. Currently designed for use only with PNG files.
class GuiImageData
{
	public:
		//!Constructor
		GuiImageData();
		//!Constructor
		//!Converts the PNG format image data to a platform-native texture,
		//!allocated and owned by this object.
		//!\param i Source image data (PNG)
		//!\param w Max image width (0 = not set)
		//!\param h Max image height (0 = not set)
		GuiImageData(const uint8_t * i, int w=0, int h=0);
		//!Constructor
		//!Converts the PNG format image data to a platform-native texture,
		//!decoded directly into a caller-supplied buffer.
		//!\param i Source image data (PNG)
		//!\param dst Destination texture buffer, owned by the caller
		//!\param maxw Max image width (0 = not set)
		//!\param maxh Max image height (0 = not set)
		GuiImageData(const uint8_t * i, uint8_t * dst, int maxw=0, int maxh=0);
		//!Constructor
		//!Populates an image directly with an already-created platform-native
		//!texture
		//!\param t Platform-native texture
		//!\param w Image width
		//!\param h Image height
		//!\param takeOwnership If true (default), this object will destroy
		//!\c t when it is destroyed or replaced.
		GuiImageData(void * t, int w, int h, bool takeOwnership = true);
		//!Destructor
		~GuiImageData();
		//!Decodes new PNG data into this object's own texture, reusing the
		//!existing allocation whenever the platform's ImageRenderer says it can
		//!hold the new image instead of freeing and reallocating.
		//!\param pngData Source image data (PNG)
		//!\param maxw Max image width (0 = not set)
		//!\param maxh Max image height (0 = not set)
		//!\return true on success
		bool reload(const uint8_t * pngData, int maxw = 0, int maxh = 0);
		//!Gets the attached platform-native texture
		//!\return opaque texture handle
		void * getTexture() { return texture; }
		//!Gets the image width
		//!\return image width
		int getWidth() { return width; }
		//!Gets the image height
		//!\return image height
		int getHeight() { return height; }
		//!Sets the scratch buffer that every decode (the PNG-decoding
		//!constructors, and reload()) will stage its raw, uncompressed pixel
		//!rows into while it runs.
		//!\param buffer Scratch buffer, owned and sized by the caller
		//!\param size Size of buffer, in bytes
		static void setDecodeScratch(void * buffer, unsigned int size);
		static Mutex & scratchLock();

		//!CPU-only decode result from decodeToRgba(): raw RGBA8 pixel data
		//!that has NOT been uploaded to a platform texture yet.
		struct DecodedImage
		{
			std::unique_ptr<uint8_t, decltype(&free)> rgba{nullptr, free}; //!< row-major RGBA8, or null if decode failed
			int width = 0;
			int height = 0;
			//!False when maxw/maxh constrained this decode to a caller-specified pixel box
			bool assetScaled = true;
			bool valid() const { return rgba != nullptr; }
		};
		//!Decodes a PNG buffer into plain RGBA8 pixels, resizing to fit
		//!maxw/maxh the same way the decoding constructors/reload() do.
		//!\param pngData Source image data (PNG)
		//!\param maxw Max image width (0 = not set)
		//!\param maxh Max image height (0 = not set)
		//!\return a DecodedImage; check valid() before use
		static DecodedImage decodeToRgba(const uint8_t * pngData, int maxw = 0, int maxh = 0);
		//!Uploads a DecodedImage produced by decodeToRgba() into this
		//!object's own texture, reusing the existing allocation whenever
		//!ImageRenderer::canReuseTexture() allows (same policy as reload()). Must be
		//!called on the main/GPU thread.
		//!\param decoded Result of a prior decodeToRgba() call
		//!\return true on success
		bool uploadDecoded(DecodedImage && decoded);
		//!Decodes PNG data and fits it into a box of a given size, then masks it with
		//!a rounded rectangle: the image is centred in the box (never scaled), anything
		//!that falls outside the box or the rounded corners is cropped, and the mask edge
		//!is anti-aliased. Replaces this object's texture; its size becomes the box size.
		//!Use it for a picture that sits on a code-drawn button, so it can't spill over
		//!the button's border or corners.
		//!\param pngData Source image data (PNG)
		//!\param boxW Box width, in design-canvas pixels
		//!\param boxH Box height, in design-canvas pixels
		//!\param radius Corner radius of the box (clamped to half the shorter side)
		//!\param inset Shrinks the mask on every side, eg. to stay inside a stroke
		//!\param offsetX Moves the image horizontally from centre (negative = left)
		//!\param offsetY Moves the image vertically from centre (negative = up)
		//!\return true on success
		bool loadClipped(const uint8_t * pngData, int boxW, int boxH, float radius, float inset = 0.0f, float offsetX = 0.0f, float offsetY = 0.0f);
		//!Releases any owned texture and resets this object to the same
		//!(empty) state as a default-constructed GuiImageData. Useful for
		//!explicitly freeing GPU memory (eg. an evicted cache entry)
		//!before the object itself is destroyed.
		void clear();
	protected:
		void * texture; //!< Attached platform-native texture
		int height; //!< Height of image
		int width; //!< Width of image
	private:
		bool ownsTexture; //!< Whether this object may destroy/replace texture
		int capWidth; //!< Width texture was allocated to hold, if ownsTexture (0 otherwise)
		int capHeight; //!< Height texture was allocated to hold, if ownsTexture (0 otherwise)
		bool cachedView = false; //!< texture is a shared GuiImageDataCache texture we merely borrow; never written to or freed
		//!Decodes a PNG buffer, reusing the existing owned texture if it's
		//!already large enough, otherwise (re)allocating one.
		bool decodeImage(const uint8_t * pngData, int * width, int * height, int maxw, int maxh);
};

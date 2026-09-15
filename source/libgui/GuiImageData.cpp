/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiImageData.cpp
 ***************************************************************************/

#include <png.h>
#include <setjmp.h>

#include "Gui.h"

namespace {
	uint8_t * scratchBuffer = nullptr;
	unsigned int scratchBufferSize = 0;

	Mutex & getScratchMutex()
	{
		static Mutex scratchMutex;
		return scratchMutex;
	}

	void ErrorCb(png_structp png_ptr, png_const_charp) { longjmp(png_jmpbuf(png_ptr), 1); }
	void WarningCb(png_structp, png_const_charp) {}
}

void GuiImageData::setDecodeScratch(void * buffer, unsigned int size)
{
	scratchBuffer = static_cast<uint8_t *>(buffer);
	scratchBufferSize = buffer ? size : 0;
}

Mutex & GuiImageData::scratchLock()
{
	return getScratchMutex();
}

GuiImageData::GuiImageData()
{
	texture = nullptr;
	width = 0;
	height = 0;
	ownsTexture = false;
	capWidth = 0;
	capHeight = 0;
}

GuiImageData::GuiImageData(const uint8_t * i, int maxw, int maxh)
{
	texture = nullptr;
	width = 0;
	height = 0;
	ownsTexture = false;
	capWidth = 0;
	capHeight = 0;

	if(i)
		decodeImage(i, &width, &height, maxw, maxh);
}

GuiImageData::GuiImageData(const uint8_t * i, uint8_t * dst, int maxw, int maxh)
{
	texture = dst;
	width = 0;
	height = 0;
	ownsTexture = false;
	capWidth = 0;
	capHeight = 0;

	if(i) {
		decodeImage(i, &width, &height, maxw, maxh);
	}
}

GuiImageData::GuiImageData(void * tex, int w, int h, bool takeOwnership)
{
	texture = tex;
	width = w;
	height = h;
	ownsTexture = takeOwnership && tex;
	capWidth = ownsTexture ? w : 0;
	capHeight = ownsTexture ? h : 0;
}

GuiImageData::~GuiImageData()
{
	clear();
}

void GuiImageData::clear()
{
	if(ownsTexture && texture)
		platform->getVideo()->getImageRenderer()->destroyTexture(texture);

	texture = nullptr;
	width = 0;
	height = 0;
	ownsTexture = false;
	capWidth = 0;
	capHeight = 0;
}

struct PngMemoryData
{
	const uint8_t * data;
	size_t offset;
};

static void ReadPngDataCb(png_structp png_ptr, png_bytep data, png_size_t length)
{
	PngMemoryData * memData = static_cast<PngMemoryData *>(png_get_io_ptr(png_ptr));
	if(!memData)
		return;

	memcpy(data, memData->data + memData->offset, length);
	memData->offset += length;
}

bool GuiImageData::reload(const uint8_t * pngData, int maxw, int maxh)
{
	if(!pngData)
		return false;

	int w = 0, h = 0;
	return decodeImage(pngData, &w, &h, maxw, maxh);
}

GuiImageData::DecodedImage GuiImageData::decodeToRgba(const uint8_t * pngData, int maxw, int maxh)
{
	DecodedImage out;

	if(!pngData)
		return out;

	MutexLock scratchGuard(getScratchMutex());
	uint8_t * const localScratchBuffer = scratchBuffer;
	const unsigned int localScratchBufferSize = scratchBufferSize;

	if(!localScratchBuffer || localScratchBufferSize == 0)
		return out;

	if(png_sig_cmp(static_cast<png_const_bytep>(pngData), 0, 8))
		return out;

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, ErrorCb, WarningCb);
	if(!png_ptr)
		return out;

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
		return out;
	}

	if(setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return out;
	}

	PngMemoryData memData = { pngData, 0 };
	png_set_read_fn(png_ptr, &memData, ReadPngDataCb);

	png_read_info(png_ptr, info_ptr);

	png_uint_32 srcW, srcH;
	int bit_depth, color_type, interlace_type;
	png_get_IHDR(png_ptr, info_ptr, &srcW, &srcH, &bit_depth, &color_type, &interlace_type, nullptr, nullptr);

	png_uint_32 w = srcW, h = srcH;
	bool needsResize = (maxw > 0 && static_cast<int>(srcW) > maxw) || (maxh > 0 && static_cast<int>(srcH) > maxh);
	if(needsResize)
	{
		double wScale = maxw > 0 ? static_cast<double>(maxw) / srcW : 1e30;
		double hScale = maxh > 0 ? static_cast<double>(maxh) / srcH : 1e30;
		double scale = wScale < hScale ? wScale : hScale;
		w = static_cast<png_uint_32>(srcW * scale);
		h = static_cast<png_uint_32>(srcH * scale);
		if(w < 1) w = 1;
		if(h < 1) h = 1;
	}

	if(bit_depth == 16)
		png_set_strip_16(png_ptr);
	if(color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);
	if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);
	if(color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
	if(color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	png_read_update_info(png_ptr, info_ptr);
	unsigned int rowBytes = png_get_rowbytes(png_ptr, info_ptr);

	unsigned long long rowPtrBytes = static_cast<unsigned long long>(srcH) * sizeof(png_bytep);
	unsigned long long srcRgbaBytes = static_cast<unsigned long long>(rowBytes) * srcH;
	unsigned long long resizedRgbaBytes = needsResize ? static_cast<unsigned long long>(w) * h * 4 : 0;
	unsigned long long totalScratchBytes = rowPtrBytes + srcRgbaBytes + resizedRgbaBytes;
	if(totalScratchBytes > localScratchBufferSize)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return out;
	}

	// Final output buffer this call owns and returns - deliberately NOT part of the shared scratch allocation,
	// since the caller (eg: a background thread) will go on using it.
	std::unique_ptr<uint8_t, decltype(&free)> outRgba(static_cast<uint8_t *>(malloc(static_cast<size_t>(w) * h * 4)), free);
	if(!outRgba)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return out;
	}

	png_bytep * row_pointers = reinterpret_cast<png_bytep *>(localScratchBuffer);
	uint8_t * srcRgba = localScratchBuffer + rowPtrBytes;

	for(png_uint_32 i = 0; i < srcH; i++)
		row_pointers[i] = srcRgba + (static_cast<size_t>(i) * rowBytes);

	png_read_image(png_ptr, row_pointers);

	if(needsResize)
	{
		uint32_t xRatio = ((srcW << 16) / w) + 1;
		uint32_t yRatio = ((srcH << 16) / h) + 1;

		for(png_uint_32 y = 0; y < h; y++)
		{
			png_uint_32 sy = (y * yRatio) >> 16;
			if(sy >= srcH) sy = srcH - 1;
			const uint8_t * srcRow = srcRgba + static_cast<size_t>(sy) * rowBytes;
			uint8_t * dstRow = outRgba.get() + static_cast<size_t>(y) * w * 4;

			for(png_uint_32 x = 0; x < w; x++)
			{
				png_uint_32 sx = (x * xRatio) >> 16;
				if(sx >= srcW) sx = srcW - 1;
				memcpy(dstRow + x * 4, srcRow + sx * 4, 4);
			}
		}
	}
	else
	{
		memcpy(outRgba.get(), srcRgba, srcRgbaBytes);
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

	out.rgba = std::move(outRgba);
	out.width = w;
	out.height = h;
	return out;
}

bool GuiImageData::uploadDecoded(DecodedImage && decoded)
{
	if(!decoded.valid())
		return false;

	int w = decoded.width;
	int h = decoded.height;

	bool haveUsableTexture = texture && (!ownsTexture || (w <= capWidth && h <= capHeight));

	void * newTexture = texture;
	if(!haveUsableTexture)
	{
		newTexture = platform->getVideo()->getImageRenderer()->createTexture(w, h);
		if(!newTexture)
			return false;
	}

	platform->getVideo()->getImageRenderer()->loadTextureData(newTexture, decoded.rgba.get(), w, h);

	if(!haveUsableTexture)
	{
		if(ownsTexture && texture)
			platform->getVideo()->getImageRenderer()->destroyTexture(texture);
		texture = newTexture;
		ownsTexture = true;
		capWidth = w;
		capHeight = h;
	}

	width = w;
	height = h;
	return true;
}

bool GuiImageData::decodeImage(const uint8_t * pngData, int * outWidth, int * outHeight, int maxw, int maxh)
{
	DecodedImage decoded = decodeToRgba(pngData, maxw, maxh);
	if(!decoded.valid())
		return false;

	int w = decoded.width;
	int h = decoded.height;

	if(!uploadDecoded(std::move(decoded)))
		return false;

	if(outWidth) *outWidth = w;
	if(outHeight) *outHeight = h;
	return true;
}

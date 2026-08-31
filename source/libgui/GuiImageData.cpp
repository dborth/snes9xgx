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

	void ErrorCb(png_structp png_ptr, png_const_charp) { longjmp(png_jmpbuf(png_ptr), 1); }
	void WarningCb(png_structp, png_const_charp) {}
}

void GuiImageData::setDecodeScratch(void * buffer, unsigned int size)
{
	scratchBuffer = static_cast<uint8_t *>(buffer);
	scratchBufferSize = buffer ? size : 0;
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
	if(ownsTexture && texture)
	{
		platform->getVideo()->getImageRenderer()->destroyTexture(texture);
	}
	texture = nullptr;
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

bool GuiImageData::decodeImage(const uint8_t * pngData, int * outWidth, int * outHeight, int maxw, int maxh)
{
	if(!pngData)
		return false;

	if(png_sig_cmp(static_cast<png_const_bytep>(pngData), 0, 8))
		return false;

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, ErrorCb, WarningCb);
	if(!png_ptr)
		return false;

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
		return false;
	}

	if(setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return false;
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
	if(!scratchBuffer || totalScratchBytes > scratchBufferSize)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return false;
	}

	bool haveUsableTexture = texture && (!ownsTexture || (static_cast<int>(w) <= capWidth && static_cast<int>(h) <= capHeight));

	void * newTexture = texture;
	if(!haveUsableTexture)
	{
		newTexture = platform->getVideo()->getImageRenderer()->createTexture(w, h);
		if(!newTexture)
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
			return false;
		}
	}

	png_bytep * row_pointers = reinterpret_cast<png_bytep *>(scratchBuffer);
	uint8_t * srcRgba = scratchBuffer + rowPtrBytes;

	for(png_uint_32 i = 0; i < srcH; i++)
		row_pointers[i] = srcRgba + (static_cast<size_t>(i) * rowBytes);

	png_read_image(png_ptr, row_pointers);

	const uint8_t * finalRgba = srcRgba;
	if(needsResize)
	{
		uint8_t * resizedRgba = srcRgba + srcRgbaBytes;
		uint32_t xRatio = ((srcW << 16) / w) + 1;
		uint32_t yRatio = ((srcH << 16) / h) + 1;

		for(png_uint_32 y = 0; y < h; y++)
		{
			png_uint_32 sy = (y * yRatio) >> 16;
			if(sy >= srcH) sy = srcH - 1;
			const uint8_t * srcRow = srcRgba + static_cast<size_t>(sy) * rowBytes;
			uint8_t * dstRow = resizedRgba + static_cast<size_t>(y) * w * 4;

			for(png_uint_32 x = 0; x < w; x++)
			{
				png_uint_32 sx = (x * xRatio) >> 16;
				if(sx >= srcW) sx = srcW - 1;
				memcpy(dstRow + x * 4, srcRow + sx * 4, 4);
			}
		}

		finalRgba = resizedRgba;
	}

	platform->getVideo()->getImageRenderer()->loadTextureData(newTexture, finalRgba, w, h);

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
	if(outWidth) *outWidth = w;
	if(outHeight) *outHeight = h;

	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
	return true;
}

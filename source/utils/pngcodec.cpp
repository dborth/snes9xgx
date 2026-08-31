/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * pngcodec.cpp
 *
 * Minimal, platform-agnostic PNG encode/decode helpers.
 ***************************************************************************/

#include <png.h>
#include <setjmp.h>
#include <string.h>

#include "pngcodec.h"
#include "memmanager.h"

namespace
{
	struct PngMemReader
	{
		const uint8_t *data;
		size_t offset;
	};

	void ReadCb(png_structp png_ptr, png_bytep data, png_size_t length)
	{
		PngMemReader *reader = static_cast<PngMemReader *>(png_get_io_ptr(png_ptr));
		memcpy(data, reader->data + reader->offset, length);
		reader->offset += length;
	}

	struct PngMemWriter
	{
		uint8_t *buffer;
		uint32_t cursor;
	};

	void WriteCb(png_structp png_ptr, png_bytep data, png_size_t length)
	{
		PngMemWriter *writer = static_cast<PngMemWriter *>(png_get_io_ptr(png_ptr));
		memcpy(writer->buffer + writer->cursor, data, length);
		writer->cursor += length;
	}

	void FlushCb(png_structp) {}

	void ErrorCb(png_structp png_ptr, png_const_charp)
	{
		// Return control to the setjmp point on corrupt/truncated image data
		longjmp(png_jmpbuf(png_ptr), 1);
	}

	void WarningCb(png_structp, png_const_charp) {}
}

bool PNGGetImageSize(const uint8_t *src, int *outWidth, int *outHeight)
{
	if(!src || png_sig_cmp(const_cast<png_bytep>(src), 0, 8))
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

	PngMemReader reader = { src, 0 };
	png_set_read_fn(png_ptr, &reader, ReadCb);
	png_read_info(png_ptr, info_ptr);

	png_uint_32 w = 0, h = 0;
	int bitDepth = 0, colorType = 0, interlace = 0;
	png_get_IHDR(png_ptr, info_ptr, &w, &h, &bitDepth, &colorType, &interlace, nullptr, nullptr);

	if(outWidth) *outWidth = static_cast<int>(w);
	if(outHeight) *outHeight = static_cast<int>(h);

	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
	return true;
}

uint8_t * DecodePNGToRGBA8(const uint8_t *src, int width, int height)
{
	if(!src || width <= 0 || height <= 0)
		return nullptr;
	if(png_sig_cmp(const_cast<png_bytep>(src), 0, 8))
		return nullptr;

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, ErrorCb, WarningCb);
	if(!png_ptr)
		return nullptr;

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
		return nullptr;
	}

	uint8_t *dst = nullptr;
	png_bytep *rowPointers = nullptr;
	uint8_t *rowScratch = nullptr;

	if(setjmp(png_jmpbuf(png_ptr)))
	{
		if(rowPointers) extmem_free(rowPointers);
		if(rowScratch) extmem_free(rowScratch);
		if(dst) extmem_free(dst);
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return nullptr;
	}

	PngMemReader reader = { src, 0 };
	png_set_read_fn(png_ptr, &reader, ReadCb);
	png_read_info(png_ptr, info_ptr);

	png_uint_32 srcW = 0, srcH = 0;
	int bitDepth = 0, colorType = 0, interlace = 0;
	png_get_IHDR(png_ptr, info_ptr, &srcW, &srcH, &bitDepth, &colorType, &interlace, nullptr, nullptr);

	// The caller is expected to already know the image's real dimensions
	// (e.g. from PNGGetImageSize(), or because it wrote the PNG itself)
	if(static_cast<int>(srcW) != width || static_cast<int>(srcH) != height)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return nullptr;
	}

	if(bitDepth == 16)
		png_set_strip_16(png_ptr);
	if(colorType == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);
	if(colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);
	if(colorType == PNG_COLOR_TYPE_RGB || colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
	if(colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	png_read_update_info(png_ptr, info_ptr);
	unsigned int rowBytes = png_get_rowbytes(png_ptr, info_ptr);

	dst = static_cast<uint8_t *>(extmem_malloc(static_cast<uint32_t>(width) * height * 4));
	if(!dst)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return nullptr;
	}

	rowPointers = static_cast<png_bytep *>(extmem_malloc(sizeof(png_bytep) * height));
	if(!rowPointers)
	{
		extmem_free(dst);
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return nullptr;
	}

	if(rowBytes == static_cast<unsigned int>(width) * 4)
	{
		// Common case: after the transforms above each row is already
		// exactly width*4 bytes, so libpng can decode straight into dst
		for(int y = 0; y < height; y++)
			rowPointers[y] = dst + static_cast<size_t>(y) * rowBytes;
		png_read_image(png_ptr, rowPointers);
	}
	else
	{
		// Unexpected row padding - decode via a scratch buffer, then
		// compact each row down into the tightly-packed destination
		rowScratch = static_cast<uint8_t *>(extmem_malloc(static_cast<size_t>(rowBytes) * height));
		if(!rowScratch)
		{
			extmem_free(rowPointers);
			extmem_free(dst);
			png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
			return nullptr;
		}

		for(int y = 0; y < height; y++)
			rowPointers[y] = rowScratch + static_cast<size_t>(y) * rowBytes;

		png_read_image(png_ptr, rowPointers);

		for(int y = 0; y < height; y++)
			memcpy(dst + static_cast<size_t>(y) * width * 4, rowScratch + static_cast<size_t>(y) * rowBytes, width * 4);

		extmem_free(rowScratch);
		rowScratch = nullptr;
	}

	extmem_free(rowPointers);
	rowPointers = nullptr;

	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
	return dst;
}

uint8_t * EncodePNGFromRGB24(uint32_t width, uint32_t height, const void *rgb, uint32_t stride, uint32_t *outSize)
{
	if(!rgb || width == 0 || height == 0)
		return nullptr;

	if(stride == 0)
		stride = width * 3;

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if(!png_ptr)
		return nullptr;

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr)
	{
		png_destroy_write_struct(&png_ptr, nullptr);
		return nullptr;
	}

	// libpng doesn't know the compressed size ahead of time, so allocate a
	// generous upper bound (raw RGB size, plus the zlib/PNG filter/chunk
	// overhead, which can never make the output bigger than the input)
	uint32_t bufSize = width * height * 3 + (height * 16) + 8192;
	uint8_t *buffer = static_cast<uint8_t *>(extmem_malloc(bufSize));
	if(!buffer)
	{
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return nullptr;
	}

	png_bytep *rowPointers = nullptr;

	if(setjmp(png_jmpbuf(png_ptr)))
	{
		if(rowPointers) extmem_free(rowPointers);
		extmem_free(buffer);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return nullptr;
	}

	PngMemWriter writer = { buffer, 0 };
	png_set_write_fn(png_ptr, &writer, WriteCb, FlushCb);

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	rowPointers = static_cast<png_bytep *>(extmem_malloc(sizeof(png_bytep) * height));
	if(!rowPointers)
	{
		extmem_free(buffer);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return nullptr;
	}

	const uint8_t *src = static_cast<const uint8_t *>(rgb);
	for(uint32_t y = 0; y < height; y++)
		rowPointers[y] = const_cast<png_bytep>(src + static_cast<size_t>(y) * stride);

	png_set_rows(png_ptr, info_ptr, rowPointers);
	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, nullptr);
	png_write_end(png_ptr, nullptr);

	extmem_free(rowPointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	if(outSize) *outSize = writer.cursor;
	return buffer;
}

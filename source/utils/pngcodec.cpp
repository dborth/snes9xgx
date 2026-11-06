/****************************************************************************
 * Snes9x GX
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
	void * Alloc(size_t size)
	{
		return size <= 0xFFFFFFFFu ? extmem_malloc((uint32_t)size) : nullptr;
	}

	void Release(void * ptr)
	{
		if(ptr)
			extmem_free(ptr);
	}

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
		uint32_t capacity;
	};

	void WriteCb(png_structp png_ptr, png_bytep data, png_size_t length)
	{
		PngMemWriter *writer = static_cast<PngMemWriter *>(png_get_io_ptr(png_ptr));

		// Never write past the end of the buffer: the error returns to the setjmp point
		if(length > writer->capacity - writer->cursor)
			png_error(png_ptr, "PNG output buffer is full");

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

	// libpng reports an error by longjmp()ing back to the setjmp() call. Anything in a function that
	// calls setjmp() and is changed after it has an indeterminate value once the jump happens, so
	// the functions below that call setjmp() only work through a job structure and release nothing.
	// Everything that has to be freed is owned by their caller, which never sees a longjmp.
	struct PngDecodeJob
	{
		png_structp png;
		png_infop info;
		PngMemReader reader;
		int width;
		int height;
		size_t rowBytes;	// of a decoded row, once ReadHeader has run
		png_bytep * rows;
	};

	// Reads the header, checks the image is the size that was asked for, and sets up the conversion to RGBA8
	bool ReadHeader(PngDecodeJob & job)
	{
		if(setjmp(png_jmpbuf(job.png)))
			return false;

		png_set_read_fn(job.png, &job.reader, ReadCb);
		png_read_info(job.png, job.info);

		png_uint_32 srcW = 0, srcH = 0;
		int bitDepth = 0, colorType = 0, interlace = 0;
		png_get_IHDR(job.png, job.info, &srcW, &srcH, &bitDepth, &colorType, &interlace, nullptr, nullptr);

		// The caller is expected to already know the image's real dimensions
		// (e.g. from PNGGetImageSize(), or because it wrote the PNG itself)
		if(static_cast<int>(srcW) != job.width || static_cast<int>(srcH) != job.height)
			return false;

		if(bitDepth == 16)
			png_set_strip_16(job.png);
		if(colorType == PNG_COLOR_TYPE_PALETTE)
			png_set_palette_to_rgb(job.png);
		if(colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
			png_set_expand_gray_1_2_4_to_8(job.png);
		if(png_get_valid(job.png, job.info, PNG_INFO_tRNS))
			png_set_tRNS_to_alpha(job.png);
		if(colorType == PNG_COLOR_TYPE_RGB || colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_PALETTE)
			png_set_filler(job.png, 0xFF, PNG_FILLER_AFTER);
		if(colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
			png_set_gray_to_rgb(job.png);

		png_read_update_info(job.png, job.info);
		job.rowBytes = png_get_rowbytes(job.png, job.info);
		return true;
	}

	bool ReadRows(PngDecodeJob & job)
	{
		if(setjmp(png_jmpbuf(job.png)))
			return false;

		png_read_image(job.png, job.rows);
		return true;
	}

	struct PngEncodeJob
	{
		png_structp png;
		png_infop info;
		PngMemWriter writer;
		uint32_t width;
		uint32_t height;
		png_bytep * rows;
	};

	bool WriteImage(PngEncodeJob & job)
	{
		if(setjmp(png_jmpbuf(job.png)))
			return false;

		png_set_write_fn(job.png, &job.writer, WriteCb, FlushCb);
		png_set_IHDR(job.png, job.info, job.width, job.height, 8, PNG_COLOR_TYPE_RGB,
			PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_set_rows(job.png, job.info, job.rows);

		// writes the header, the image and the end of the file
		png_write_png(job.png, job.info, PNG_TRANSFORM_IDENTITY, nullptr);
		return true;
	}
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

	const size_t dstSize = static_cast<size_t>(width) * height * 4;
	if(dstSize > 0xFFFFFFFFu)
		return nullptr;

	PngDecodeJob job = {};
	job.png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, ErrorCb, WarningCb);
	if(!job.png)
		return nullptr;

	job.info = png_create_info_struct(job.png);
	if(!job.info)
	{
		png_destroy_read_struct(&job.png, nullptr, nullptr);
		return nullptr;
	}

	job.reader.data = src;
	job.width = width;
	job.height = height;

	uint8_t * dst = nullptr;
	uint8_t * scratch = nullptr;
	bool ok = false;

	if(ReadHeader(job))
	{
		const size_t tight = static_cast<size_t>(width) * 4;

		// Common case: after the transforms each row is already exactly width*4 bytes, so libpng
		// can decode straight into dst. Otherwise decode via a scratch buffer, then compact each
		// row down into the tightly-packed destination.
		const bool direct = (job.rowBytes == tight);

		dst = static_cast<uint8_t *>(Alloc(dstSize));
		job.rows = static_cast<png_bytep *>(Alloc(sizeof(png_bytep) * height));
		if(!direct && job.rowBytes > tight)
			scratch = static_cast<uint8_t *>(Alloc(job.rowBytes * height));

		if(dst && job.rows && (direct || scratch))
		{
			uint8_t * target = direct ? dst : scratch;

			for(int y = 0; y < height; y++)
				job.rows[y] = target + static_cast<size_t>(y) * job.rowBytes;

			ok = ReadRows(job);

			if(ok && !direct)
			{
				for(int y = 0; y < height; y++)
					memcpy(dst + static_cast<size_t>(y) * tight, scratch + static_cast<size_t>(y) * job.rowBytes, tight);
			}
		}
	}

	Release(scratch);
	Release(job.rows);
	png_destroy_read_struct(&job.png, &job.info, nullptr);

	if(!ok)
	{
		Release(dst);
		dst = nullptr;
	}
	return dst;
}

uint8_t * EncodePNGFromRGB24(uint32_t width, uint32_t height, const void *rgb, uint32_t stride, uint32_t *outSize)
{
	if(!rgb || width == 0 || height == 0)
		return nullptr;

	const uint32_t rowStride = (stride != 0) ? stride : width * 3;

	// libpng doesn't know the compressed size ahead of time, so allocate a
	// generous upper bound (raw RGB size, plus the zlib/PNG filter/chunk
	// overhead, which can never make the output bigger than the input)
	const uint64_t bufSize = static_cast<uint64_t>(width) * height * 3 + (static_cast<uint64_t>(height) * 16) + 8192;
	if(bufSize > 0xFFFFFFFFu)
		return nullptr;

	PngEncodeJob job = {};
	job.png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, ErrorCb, WarningCb);
	if(!job.png)
		return nullptr;

	job.info = png_create_info_struct(job.png);
	if(!job.info)
	{
		png_destroy_write_struct(&job.png, nullptr);
		return nullptr;
	}

	job.width = width;
	job.height = height;
	job.writer.buffer = static_cast<uint8_t *>(Alloc(static_cast<size_t>(bufSize)));
	job.writer.capacity = static_cast<uint32_t>(bufSize);
	job.rows = static_cast<png_bytep *>(Alloc(sizeof(png_bytep) * height));

	bool ok = false;

	if(job.writer.buffer && job.rows)
	{
		const uint8_t *src = static_cast<const uint8_t *>(rgb);
		for(uint32_t y = 0; y < height; y++)
			job.rows[y] = const_cast<png_bytep>(src + static_cast<size_t>(y) * rowStride);

		ok = WriteImage(job);
	}

	Release(job.rows);
	png_destroy_write_struct(&job.png, &job.info);

	if(!ok)
	{
		Release(job.writer.buffer);
		return nullptr;
	}

	if(outSize) *outSize = job.writer.cursor;
	return job.writer.buffer;
}

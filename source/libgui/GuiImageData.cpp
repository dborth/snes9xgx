/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiImageData.cpp
 ***************************************************************************/

#include <png.h>
#include <setjmp.h>

#include "Gui.h"

GuiImageData::GuiImageData(const uint8_t * i, int maxw, int maxh)
{
	texture = nullptr;
	width = 0;
	height = 0;

	if(i)
		decodeImage(i, &width, &height, maxw, maxh);
}

GuiImageData::GuiImageData(const uint8_t * i, uint8_t * dst, int maxw, int maxh)
{
	texture = dst;
	width = 0;
	height = 0;

	if(i) {
		decodeImage(i, &width, &height, maxw, maxh);
	}
}

GuiImageData::GuiImageData(void * tex, int w, int h)
{
	texture = tex;
	width = w;
	height = h;
}

GuiImageData::~GuiImageData()
{
	if(texture)
	{
		destroyTexture(texture);
		texture = nullptr;
	}
}

struct PngMemoryData
{
	const uint8_t * data;
	size_t offset;
};

void ReadPngDataCb(png_structp png_ptr, png_bytep data, png_size_t length)
{
	PngMemoryData * memData = static_cast<PngMemoryData *>(png_get_io_ptr(png_ptr));
	if(!memData)
		return;

	memcpy(data, memData->data + memData->offset, length);
	memData->offset += length;
}

void GuiImageData::decodeImage(const uint8_t * pngData, int * width, int * height, int maxw, int maxh)
{
	if(!pngData)
		return;

	if(png_sig_cmp(static_cast<png_const_bytep>(pngData), 0, 8))
		return;

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if(!png_ptr)
		return;

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
		return;
	}

	if(setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return;
	}

	PngMemoryData memData = { pngData, 0 };
	png_set_read_fn(png_ptr, &memData, ReadPngDataCb);

	png_read_info(png_ptr, info_ptr);

	png_uint_32 w, h;
	int bit_depth, color_type, interlace_type;
	png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type, &interlace_type, nullptr, nullptr);

	if((maxw > 0 && static_cast<int>(w) > maxw) || (maxh > 0 && static_cast<int>(h) > maxh))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		return;
	}

	if(width) *width = w;
	if(height) *height = h;

	void* hwTexture = nullptr;

	if(!texture)
	{
		hwTexture = createTexture(w, h);
		if(!hwTexture)
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
			return;
		}
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
	int rowBytes = png_get_rowbytes(png_ptr, info_ptr);

	uint8_t * rgba = static_cast<uint8_t *>(malloc(rowBytes * h));
	if(!rgba)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		destroyTexture(hwTexture);
		return;
	}

	png_bytep * row_pointers = static_cast<png_bytep *>(malloc(sizeof(png_bytep) * h));
	if(!row_pointers)
	{
		free(rgba);
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		destroyTexture(hwTexture);
		return;
	}

	for(png_uint_32 i = 0; i < h; i++)
		row_pointers[i] = rgba + (i * rowBytes);

	png_read_image(png_ptr, row_pointers);

	if(hwTexture) texture = hwTexture;

	loadTextureData(texture, rgba, w, h);

	free(row_pointers);
	free(rgba);
	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
}

/********************************************************************************************

PNGU Version : 0.2a

Coder : frontier

More info : http://frontier-dev.net

Modified by Tantric, 2009

********************************************************************************************/
#include <stdio.h>
#include <malloc.h>
#include "pngu.h"
#include "png.h"

// Constants
#define PNGU_SOURCE_BUFFER			1
#define PNGU_SOURCE_DEVICE			2

// Prototypes of helper functions
int pngu_info (IMGCTX ctx);
int pngu_decode (IMGCTX ctx, PNGU_u32 width, PNGU_u32 height, PNGU_u32 stripAlpha);
void pngu_free_info (IMGCTX ctx);
void pngu_read_data_from_buffer (png_structp png_ptr, png_bytep data, png_size_t length);
void pngu_write_data_to_buffer (png_structp png_ptr, png_bytep data, png_size_t length);
void pngu_flush_data_to_buffer (png_structp png_ptr);
int pngu_clamp (int value, int min, int max);

// PNGU Image context struct
struct _IMGCTX
{
	int source;
	void *buffer;
	char *filename;
	PNGU_u32 cursor;

	PNGU_u32 propRead;
	PNGUPROP prop;

	PNGU_u32 infoRead;
	png_structp png_ptr;
	png_infop info_ptr;
	FILE *fd;
	
	png_bytep *row_pointers;
	png_bytep img_data;
};

// PNGU Implementation

IMGCTX PNGU_SelectImageFromBuffer (const void *buffer)
{
	IMGCTX ctx = NULL;

	if (!buffer)
		return NULL;

	ctx = malloc (sizeof (struct _IMGCTX));
	if (!ctx)
		return NULL;

	ctx->buffer = (void *) buffer;
	ctx->source = PNGU_SOURCE_BUFFER;
	ctx->cursor = 0;
	ctx->filename = NULL;
	ctx->propRead = 0;
	ctx->infoRead = 0;

	return ctx;
}

IMGCTX PNGU_SelectImageFromDevice (const char *filename)
{
	IMGCTX ctx = NULL;

	if (!filename)
		return NULL;

	ctx = malloc (sizeof (struct _IMGCTX));
	if (!ctx)
		return NULL;

	ctx->buffer = NULL;
	ctx->source = PNGU_SOURCE_DEVICE;
	ctx->cursor = 0;

	ctx->filename = malloc (strlen (filename) + 1);
	if (!ctx->filename)
	{
		free (ctx);
		return NULL;
	}
	strcpy(ctx->filename, filename);

	ctx->propRead = 0;
	ctx->infoRead = 0;

	return ctx;
}

void PNGU_ReleaseImageContext (IMGCTX ctx)
{
	if (!ctx)
		return;

	if (ctx->filename)
		free (ctx->filename);

	if ((ctx->propRead) && (ctx->prop.trans))
		free (ctx->prop.trans);

	pngu_free_info (ctx);
	free (ctx);
}

int PNGU_GetImageProperties (IMGCTX ctx, PNGUPROP *imgprop)
{
	int res;

	if (!ctx->propRead)
	{
		res = pngu_info (ctx);
		if (res != PNGU_OK)
			return res;
	}

	*imgprop = ctx->prop;
	return PNGU_OK;
}

int PNGU_DecodeTo4x4RGBA8 (IMGCTX ctx, PNGU_u32 width, PNGU_u32 height, void *buffer, PNGU_u8 default_alpha)
{
	int result;
	PNGU_u32 x, y, qwidth, qheight;
	PNGU_u64 alphaMask;

	// width and height need to be divisible by four
	if ((width % 4) || (height % 4))
		return PNGU_INVALID_WIDTH_OR_HEIGHT;

	result = pngu_decode (ctx, width, height, 0);
	if (result != PNGU_OK)
		return result;

	// Init some variables
	qwidth = width >> 2;
	qheight = height >> 2;

	// Check is source image has an alpha channel
	if ( (ctx->prop.imgColorType == PNGU_COLOR_TYPE_GRAY_ALPHA) || (ctx->prop.imgColorType == PNGU_COLOR_TYPE_RGB_ALPHA) )
	{
		// Alpha channel present, copy image to the output buffer
		for (y = 0; y < qheight; y++)
			for (x = 0; x < qwidth; x++)
			{
				int blockbase = (y * qwidth + x) << 3;

				PNGU_u32 y4 = y << 2;
				PNGU_u32 x16 = x << 4;

				PNGU_u64 fieldA = *((PNGU_u64 *)(ctx->row_pointers[y4]+x16));
				PNGU_u64 fieldB = *((PNGU_u64 *)(ctx->row_pointers[y4]+x16+8));
				((PNGU_u64 *) buffer)[blockbase] = 
					((fieldA & 0xFF00000000ULL) << 24) | ((fieldA & 0xFF00000000000000ULL) >> 8) | 
					((fieldA & 0xFFULL) << 40) | ((fieldA & 0xFF000000ULL) << 8) | 
					((fieldB & 0xFF00000000ULL) >> 8) | ((fieldB & 0xFF00000000000000ULL) >> 40) | 
					((fieldB & 0xFFULL) << 8) | ((fieldB & 0xFF000000ULL) >> 24);
				((PNGU_u64 *) buffer)[blockbase+4] =
					((fieldA & 0xFFFF0000000000ULL) << 8) | ((fieldA & 0xFFFF00ULL) << 24) |
					((fieldB & 0xFFFF0000000000ULL) >> 24) | ((fieldB & 0xFFFF00ULL) >> 8);

				fieldA = *((PNGU_u64 *)(ctx->row_pointers[y4+1]+x16));
				fieldB = *((PNGU_u64 *)(ctx->row_pointers[y4+1]+x16+8));
				((PNGU_u64 *) buffer)[blockbase+1] = 
					((fieldA & 0xFF00000000ULL) << 24) | ((fieldA & 0xFF00000000000000ULL) >> 8) | 
					((fieldA & 0xFFULL) << 40) | ((fieldA & 0xFF000000ULL) << 8) | 
					((fieldB & 0xFF00000000ULL) >> 8) | ((fieldB & 0xFF00000000000000ULL) >> 40) | 
					((fieldB & 0xFFULL) << 8) | ((fieldB & 0xFF000000ULL) >> 24);
				((PNGU_u64 *) buffer)[blockbase+5] =
					((fieldA & 0xFFFF0000000000ULL) << 8) | ((fieldA & 0xFFFF00ULL) << 24) |
					((fieldB & 0xFFFF0000000000ULL) >> 24) | ((fieldB & 0xFFFF00ULL) >> 8);

				fieldA = *((PNGU_u64 *)(ctx->row_pointers[y4+2]+x16));
				fieldB = *((PNGU_u64 *)(ctx->row_pointers[y4+2]+x16+8));
				((PNGU_u64 *) buffer)[blockbase+2] = 
					((fieldA & 0xFF00000000ULL) << 24) | ((fieldA & 0xFF00000000000000ULL) >> 8) | 
					((fieldA & 0xFFULL) << 40) | ((fieldA & 0xFF000000ULL) << 8) | 
					((fieldB & 0xFF00000000ULL) >> 8) | ((fieldB & 0xFF00000000000000ULL) >> 40) | 
					((fieldB & 0xFFULL) << 8) | ((fieldB & 0xFF000000ULL) >> 24);
				((PNGU_u64 *) buffer)[blockbase+6] =
					((fieldA & 0xFFFF0000000000ULL) << 8) | ((fieldA & 0xFFFF00ULL) << 24) |
					((fieldB & 0xFFFF0000000000ULL) >> 24) | ((fieldB & 0xFFFF00ULL) >> 8);

				fieldA = *((PNGU_u64 *)(ctx->row_pointers[y4+3]+x16));
				fieldB = *((PNGU_u64 *)(ctx->row_pointers[y4+3]+x16+8));
				((PNGU_u64 *) buffer)[blockbase+3] = 
					((fieldA & 0xFF00000000ULL) << 24) | ((fieldA & 0xFF00000000000000ULL) >> 8) | 
					((fieldA & 0xFFULL) << 40) | ((fieldA & 0xFF000000ULL) << 8) | 
					((fieldB & 0xFF00000000ULL) >> 8) | ((fieldB & 0xFF00000000000000ULL) >> 40) | 
					((fieldB & 0xFFULL) << 8) | ((fieldB & 0xFF000000ULL) >> 24);
				((PNGU_u64 *) buffer)[blockbase+7] =
					((fieldA & 0xFFFF0000000000ULL) << 8) | ((fieldA & 0xFFFF00ULL) << 24) |
					((fieldB & 0xFFFF0000000000ULL) >> 24) | ((fieldB & 0xFFFF00ULL) >> 8);
			}
	}
	else
	{
		// No alpha channel present, copy image to the output buffer
		alphaMask = (((PNGU_u64)default_alpha) << 56) | (((PNGU_u64)default_alpha) << 40) |
					(((PNGU_u64)default_alpha) << 24) | (((PNGU_u64)default_alpha) << 8);

		for (y = 0; y < qheight; y++)
			for (x = 0; x < qwidth; x++)
			{
				int blockbase = (y * qwidth + x) << 3;

				PNGU_u32 y4 = y << 2;
				PNGU_u32 x12 = x * 12;

				PNGU_u64 field64 = *((PNGU_u64 *)(ctx->row_pointers[y4]+x12));
				PNGU_u64 field32 = (PNGU_u64) *((PNGU_u32 *)(ctx->row_pointers[y4]+x12+8));
				((PNGU_u64 *) buffer)[blockbase] = 
					(((field64 & 0xFF00000000000000ULL) >> 8) | (field64 & 0xFF00000000ULL) | 
					((field64 & 0xFF00ULL) << 8) | ((field32 & 0xFF0000ULL) >> 16) | alphaMask);
				((PNGU_u64 *) buffer)[blockbase+4] =
					(((field64 & 0xFFFF0000000000ULL) << 8) | ((field64 & 0xFFFF0000ULL) << 16) |
					((field64 & 0xFFULL) << 24) | ((field32 & 0xFF000000ULL) >> 8) | (field32 & 0xFFFFULL));

				field64 = *((PNGU_u64 *)(ctx->row_pointers[y4+1]+x12));
				field32 = (PNGU_u64) *((PNGU_u32 *)(ctx->row_pointers[y4+1]+x12+8));
				((PNGU_u64 *) buffer)[blockbase+1] = 
					(((field64 & 0xFF00000000000000ULL) >> 8) | (field64 & 0xFF00000000ULL) | 
					((field64 & 0xFF00ULL) << 8) | ((field32 & 0xFF0000ULL) >> 16) | alphaMask);
				((PNGU_u64 *) buffer)[blockbase+5] =
					(((field64 & 0xFFFF0000000000ULL) << 8) | ((field64 & 0xFFFF0000ULL) << 16) |
					((field64 & 0xFFULL) << 24) | ((field32 & 0xFF000000ULL) >> 8) | (field32 & 0xFFFFULL));

				field64 = *((PNGU_u64 *)(ctx->row_pointers[y4+2]+x12));
				field32 = (PNGU_u64) *((PNGU_u32 *)(ctx->row_pointers[y4+2]+x12+8));
				((PNGU_u64 *) buffer)[blockbase+2] = 
					(((field64 & 0xFF00000000000000ULL) >> 8) | (field64 & 0xFF00000000ULL) | 
					((field64 & 0xFF00ULL) << 8) | ((field32 & 0xFF0000ULL) >> 16) | alphaMask);
				((PNGU_u64 *) buffer)[blockbase+6] =
					(((field64 & 0xFFFF0000000000ULL) << 8) | ((field64 & 0xFFFF0000ULL) << 16) |
					((field64 & 0xFFULL) << 24) | ((field32 & 0xFF000000ULL) >> 8) | (field32 & 0xFFFFULL));

				field64 = *((PNGU_u64 *)(ctx->row_pointers[y4+3]+x12));
				field32 = (PNGU_u64) *((PNGU_u32 *)(ctx->row_pointers[y4+3]+x12+8));
				((PNGU_u64 *) buffer)[blockbase+3] = 
					(((field64 & 0xFF00000000000000ULL) >> 8) | (field64 & 0xFF00000000ULL) | 
					((field64 & 0xFF00ULL) << 8) | ((field32 & 0xFF0000ULL) >> 16) | alphaMask);
				((PNGU_u64 *) buffer)[blockbase+7] =
					(((field64 & 0xFFFF0000000000ULL) << 8) | ((field64 & 0xFFFF0000ULL) << 16) |
					((field64 & 0xFFULL) << 24) | ((field32 & 0xFF000000ULL) >> 8) | (field32 & 0xFFFFULL));
			}
	}
	
	// Free resources
	free (ctx->img_data);
	free (ctx->row_pointers);

	// Success
	return PNGU_OK;
}

int PNGU_EncodeFromRGB (IMGCTX ctx, PNGU_u32 width, PNGU_u32 height, void *buffer, PNGU_u32 stride)
{
	png_uint_32 rowbytes;
	PNGU_u32 y;

	// Erase from the context any readed info
	pngu_free_info (ctx);
	ctx->propRead = 0;

	// Check if the user has selected a file to write the image
	if (ctx->source == PNGU_SOURCE_BUFFER);	

	else if (ctx->source == PNGU_SOURCE_DEVICE)
	{
		// Open file
		if (!(ctx->fd = fopen (ctx->filename, "wb")))
			return PNGU_CANT_OPEN_FILE;
	}

	else
		return PNGU_NO_FILE_SELECTED;

	// Allocation of libpng structs
	ctx->png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!(ctx->png_ptr))
	{
		if (ctx->source == PNGU_SOURCE_DEVICE)
			fclose (ctx->fd);
        return PNGU_LIB_ERROR;
	}

    ctx->info_ptr = png_create_info_struct (ctx->png_ptr);
    if (!(ctx->info_ptr))
    {
		png_destroy_write_struct (&(ctx->png_ptr), (png_infopp)NULL);
		if (ctx->source == PNGU_SOURCE_DEVICE)
			fclose (ctx->fd);
        return PNGU_LIB_ERROR;
    }

	if (ctx->source == PNGU_SOURCE_BUFFER)
	{
		// Installation of our custom data writer function
		ctx->cursor = 0;
		png_set_write_fn (ctx->png_ptr, ctx, pngu_write_data_to_buffer, pngu_flush_data_to_buffer);
	}
	else if (ctx->source == PNGU_SOURCE_DEVICE)
	{
		// Default data writer uses function fwrite, so it needs to use our FILE*
		png_init_io (ctx->png_ptr, ctx->fd);
	}

	// Setup output file properties
    png_set_IHDR (ctx->png_ptr, ctx->info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, 
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	// Allocate memory to store the image in RGB format
	rowbytes = width * 3;
	if (rowbytes % 4)
		rowbytes = ((rowbytes >>2) + 1) <<2; // Add extra padding so each row starts in a 4 byte boundary
		
	ctx->img_data = malloc(rowbytes * height);
	memset(ctx->img_data, 0, rowbytes * height);
	
	if (!ctx->img_data)
	{
		png_destroy_write_struct (&(ctx->png_ptr), (png_infopp)NULL);
		if (ctx->source == PNGU_SOURCE_DEVICE)
			fclose (ctx->fd);
		return PNGU_LIB_ERROR;
	}

	ctx->row_pointers = malloc (sizeof (png_bytep) * height);
	memset(ctx->row_pointers, 0, sizeof (png_bytep) * height);
	
	if (!ctx->row_pointers)
	{
		png_destroy_write_struct (&(ctx->png_ptr), (png_infopp)NULL);
		if (ctx->source == PNGU_SOURCE_DEVICE)
			fclose (ctx->fd);
		return PNGU_LIB_ERROR;
	}

	for (y = 0; y < height; ++y)
	{
		ctx->row_pointers[y] = buffer + (y * rowbytes);
	}

	// Tell libpng where is our image data
	png_set_rows (ctx->png_ptr, ctx->info_ptr, ctx->row_pointers);

	// Write file header and image data
	png_write_png (ctx->png_ptr, ctx->info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	// Tell libpng we have no more data to write
	png_write_end (ctx->png_ptr, (png_infop) NULL);

	// Free resources
	free (ctx->img_data);
	free (ctx->row_pointers);
	png_destroy_write_struct (&(ctx->png_ptr), &(ctx->info_ptr));
	if (ctx->source == PNGU_SOURCE_DEVICE)
		fclose (ctx->fd);

	// Success
	return ctx->cursor;
}

int PNGU_EncodeFromGXTexture (IMGCTX ctx, PNGU_u32 width, PNGU_u32 height, void *buffer, PNGU_u32 stride)
{
	int res;
	PNGU_u32 x,y, tmpy1, tmpy2, tmpyWid, tmpxy;

	unsigned char * ptr = (unsigned char*)buffer;
	unsigned char * tmpbuffer = (unsigned char *)malloc(width*height*3);
	memset(tmpbuffer, 0, width*height*3);
	png_uint_32 offset;
	
	for(y=0; y < height; y++)
	{
		tmpy1 = y * 640*3;
		tmpy2 = y%4 << 2;
		tmpyWid = (((y >> 2)<<4)*width);

		for(x=0; x < width; x++)
		{
			offset = tmpyWid + ((x >> 2)<<6) + ((tmpy2+ x%4 ) << 1);
			tmpxy = x * 3 + tmpy1;

			tmpbuffer[tmpxy  ] = ptr[offset+1]; // R
			tmpbuffer[tmpxy+1] = ptr[offset+32]; // G
			tmpbuffer[tmpxy+2] = ptr[offset+33]; // B
		}
	}
	
	res = PNGU_EncodeFromRGB (ctx, width, height, tmpbuffer, stride);
	free(tmpbuffer);
	return res;
}

int pngu_info (IMGCTX ctx)
{
	png_byte magic[8];
	png_uint_32 width;
	png_uint_32 height;
	png_color_16p background;
	png_bytep trans;
	png_color_16p trans_values;
	int scale, i;

	// Check if there is a file selected and if it is a valid .png
	if (ctx->source == PNGU_SOURCE_BUFFER)
		memcpy (magic, ctx->buffer, 8);

	else if (ctx->source == PNGU_SOURCE_DEVICE)
	{
		// Open file
		if (!(ctx->fd = fopen (ctx->filename, "rb")))
			return PNGU_CANT_OPEN_FILE;

		// Load first 8 bytes into magic buffer
        if (fread (magic, 1, 8, ctx->fd) != 8)
		{
			fclose (ctx->fd);
			return PNGU_CANT_READ_FILE;
		}
	}

	else
		return PNGU_NO_FILE_SELECTED;;

	if (png_sig_cmp(magic, 0, 8) != 0)
	{
		if (ctx->source == PNGU_SOURCE_DEVICE)
			fclose (ctx->fd);
		return PNGU_FILE_IS_NOT_PNG;
	}

	// Allocation of libpng structs
	ctx->png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!(ctx->png_ptr))
	{
		if (ctx->source == PNGU_SOURCE_DEVICE)
			fclose (ctx->fd);
        return PNGU_LIB_ERROR;
	}

    ctx->info_ptr = png_create_info_struct (ctx->png_ptr);
    if (!(ctx->info_ptr))
    {
		if (ctx->source == PNGU_SOURCE_DEVICE)
			fclose (ctx->fd);
        png_destroy_read_struct (&(ctx->png_ptr), (png_infopp)NULL, (png_infopp)NULL);
        return PNGU_LIB_ERROR;
    }

	if (ctx->source == PNGU_SOURCE_BUFFER)
	{
		// Installation of our custom data provider function
		ctx->cursor = 0;
		png_set_read_fn (ctx->png_ptr, ctx, pngu_read_data_from_buffer);
	}
	else if (ctx->source == PNGU_SOURCE_DEVICE)
	{
		// Default data provider uses function fread, so it needs to use our FILE*
		png_init_io (ctx->png_ptr, ctx->fd);
		png_set_sig_bytes (ctx->png_ptr, 8); // We have read 8 bytes already to check PNG authenticity
	}

	// Read png header
	png_read_info (ctx->png_ptr, ctx->info_ptr);

	// Query image properties if they have not been queried before
	if (!ctx->propRead)
	{
		int ctxNumTrans;

		png_get_IHDR(ctx->png_ptr, ctx->info_ptr, &width, &height,
					(int *) &(ctx->prop.imgBitDepth), 
					(int *) &(ctx->prop.imgColorType),
					NULL, NULL, NULL);

		ctx->prop.imgWidth = width;
		ctx->prop.imgHeight = height;
		switch (ctx->prop.imgColorType)
		{
			case PNG_COLOR_TYPE_GRAY:
				ctx->prop.imgColorType = PNGU_COLOR_TYPE_GRAY;
				break;
			case PNG_COLOR_TYPE_GRAY_ALPHA:
				ctx->prop.imgColorType = PNGU_COLOR_TYPE_GRAY_ALPHA;
				break;
			case PNG_COLOR_TYPE_PALETTE:
				ctx->prop.imgColorType = PNGU_COLOR_TYPE_PALETTE;
				break;
			case PNG_COLOR_TYPE_RGB:
				ctx->prop.imgColorType = PNGU_COLOR_TYPE_RGB;
				break;
			case PNG_COLOR_TYPE_RGB_ALPHA:
				ctx->prop.imgColorType = PNGU_COLOR_TYPE_RGB_ALPHA;
				break;
			default:
				ctx->prop.imgColorType = PNGU_COLOR_TYPE_UNKNOWN;
				break;
		}

		// Constant used to scale 16 bit values to 8 bit values
		scale = 0;
		if (ctx->prop.imgBitDepth == 16)
			scale = 8;

		// Query background color, if any.
		ctx->prop.validBckgrnd = 0;

		switch(ctx->prop.imgColorType)
		{
			case PNGU_COLOR_TYPE_RGB:
			case PNGU_COLOR_TYPE_RGB_ALPHA:
			{
				if(png_get_bKGD (ctx->png_ptr, ctx->info_ptr, &background)){
					ctx->prop.validBckgrnd = 1;
					ctx->prop.bckgrnd.r = background->red >> scale;
					ctx->prop.bckgrnd.g = background->green >> scale;
					ctx->prop.bckgrnd.b = background->blue >> scale;
				}
				
				// Query list of transparent colors, if any.
				ctx->prop.numTrans = 0;
				ctx->prop.trans = NULL;
				
				if(png_get_tRNS (ctx->png_ptr, ctx->info_ptr, &trans, (int *) &(ctx->prop.numTrans), &trans_values)){
					ctxNumTrans = ctx->prop.numTrans;
					if(ctxNumTrans){
						ctx->prop.trans = malloc (sizeof (PNGUCOLOR) * ctxNumTrans);
						if (ctx->prop.trans)
							for (i = 0; i < ctxNumTrans; i++)
							{
								ctx->prop.trans[i].r = trans_values[i].red >> scale;
								ctx->prop.trans[i].g = trans_values[i].green >> scale;
								ctx->prop.trans[i].b = trans_values[i].blue >> scale;
							}
						else
							ctx->prop.numTrans = 0;
					}
				}
				
			}
			break;
			
			case PNGU_COLOR_TYPE_GRAY:
			case PNGU_COLOR_TYPE_GRAY_ALPHA:
			{
				if(png_get_bKGD (ctx->png_ptr, ctx->info_ptr, &background)){
					ctx->prop.validBckgrnd = 1;
					ctx->prop.bckgrnd.r = 
					ctx->prop.bckgrnd.g = 
					ctx->prop.bckgrnd.b = background->gray >> scale;
				}
				
				// Query list of transparent colors, if any.
				ctx->prop.numTrans = 0;
				ctx->prop.trans = NULL;
				
				if(png_get_tRNS (ctx->png_ptr, ctx->info_ptr, &trans, (int *) &(ctx->prop.numTrans), &trans_values)){
					ctxNumTrans = ctx->prop.numTrans;
					if(ctxNumTrans){
						ctx->prop.trans = malloc (sizeof (PNGUCOLOR) * ctxNumTrans);
						if (ctx->prop.trans)
							for (i = 0; i < ctxNumTrans; i++)
								ctx->prop.trans[i].r = 
								ctx->prop.trans[i].g = 
								ctx->prop.trans[i].b = trans_values[i].gray >> scale;
						else
							ctx->prop.numTrans = 0;
					}
				}
				
			}
			break;
			
			default:
			
			// It was none of those things, 
			{
				// Query list of transparent colors, if any.
				ctx->prop.numTrans = 0;
				ctx->prop.trans = NULL;
			}
			break;
		}

		ctx->propRead = 1;
	}

	// Success
	ctx->infoRead = 1;

	return PNGU_OK;
}

int pngu_decode (IMGCTX ctx, PNGU_u32 width, PNGU_u32 height, PNGU_u32 stripAlpha)
{
	png_uint_32 rowbytes;
	png_uint_32 i, propImgHeight;

	// Read info if it hasn't been read before
	if (!ctx->infoRead)
	{
		int c = pngu_info (ctx);
		if (c != PNGU_OK)
			return c;
	}

	// Check if the user has specified the real width and height of the image
	if ( (ctx->prop.imgWidth != width) || (ctx->prop.imgHeight != height) )
		return PNGU_INVALID_WIDTH_OR_HEIGHT;

	// Check if color type is supported by PNGU
	if ( (ctx->prop.imgColorType == PNGU_COLOR_TYPE_PALETTE) || (ctx->prop.imgColorType == PNGU_COLOR_TYPE_UNKNOWN) )
		return PNGU_UNSUPPORTED_COLOR_TYPE;

	// Scale 16 bit samples to 8 bit
	if (ctx->prop.imgBitDepth == 16)
        png_set_strip_16 (ctx->png_ptr);

	// Remove alpha channel if we don't need it
	if (stripAlpha && ((ctx->prop.imgColorType == PNGU_COLOR_TYPE_RGB_ALPHA) || (ctx->prop.imgColorType == PNGU_COLOR_TYPE_GRAY_ALPHA)))
        png_set_strip_alpha (ctx->png_ptr);

	// Expand 1, 2 and 4 bit samples to 8 bit
	if (ctx->prop.imgBitDepth < 8)
        png_set_packing (ctx->png_ptr);

	// Transform grayscale images to RGB
	if ( (ctx->prop.imgColorType == PNGU_COLOR_TYPE_GRAY) || (ctx->prop.imgColorType == PNGU_COLOR_TYPE_GRAY_ALPHA) )
		png_set_gray_to_rgb (ctx->png_ptr);

	// Flush transformations
	png_read_update_info (ctx->png_ptr, ctx->info_ptr);

	// Allocate memory to store the image
	rowbytes = png_get_rowbytes (ctx->png_ptr, ctx->info_ptr);

	if (rowbytes & 3)
		rowbytes = ((rowbytes >> 2) + 1) << 2; // Add extra padding so each row starts in a 4 byte boundary

	ctx->img_data = malloc (rowbytes * ctx->prop.imgHeight);
	if (!ctx->img_data)
	{
		pngu_free_info (ctx);
		return PNGU_LIB_ERROR;
	}

	ctx->row_pointers = malloc (sizeof (png_bytep) * ctx->prop.imgHeight);
	if (!ctx->row_pointers)
	{
		free (ctx->img_data);
		pngu_free_info (ctx);
		return PNGU_LIB_ERROR;
	}

	propImgHeight = ctx->prop.imgHeight;
	for (i = 0; i < propImgHeight; ++i)
		ctx->row_pointers[i] = ctx->img_data + (i * rowbytes);

	// Transform the image and copy it to our allocated memory
	png_read_image (ctx->png_ptr, ctx->row_pointers);

	// Free resources
	pngu_free_info (ctx);

	// Success
	return PNGU_OK;
}

void pngu_free_info (IMGCTX ctx)
{
	if (ctx->infoRead)
	{
		if (ctx->source == PNGU_SOURCE_DEVICE)
			fclose (ctx->fd);

		png_destroy_read_struct (&(ctx->png_ptr), &(ctx->info_ptr), (png_infopp)NULL);

		ctx->infoRead = 0;
	}
}

// Custom data provider function used for reading from memory buffers.
void pngu_read_data_from_buffer (png_structp png_ptr, png_bytep data, png_size_t length)
{
	IMGCTX ctx = (IMGCTX) png_get_io_ptr (png_ptr);
	memcpy (data, ctx->buffer + ctx->cursor, length);
	ctx->cursor += length;
}

// Custom data writer function used for writing to memory buffers.
void pngu_write_data_to_buffer (png_structp png_ptr, png_bytep data, png_size_t length)
{
	IMGCTX ctx = (IMGCTX) png_get_io_ptr (png_ptr);
	memcpy (ctx->buffer + ctx->cursor, data, length);
	ctx->cursor += length;
}

// Custom data flusher function used for writing to memory buffers.
void pngu_flush_data_to_buffer (png_structp png_ptr)
{
	// Nothing to do here
}

// Function used in YCbYCr to RGB decoding
int pngu_clamp (int value, int min, int max)
{
	if (value < min)
		value = min;
	else if (value > max)
		value = max;

	return value;
}

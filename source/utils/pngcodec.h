/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * pngcodec.h
 *
 * Minimal, platform-agnostic PNG encode/decode helpers.
 ***************************************************************************/
#ifndef _PNGCODEC_H_
#define _PNGCODEC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Reads just the PNG header to get the image dimensions, without decoding
// any pixel data. Useful for sizing a destination buffer.
// Returns true on success.
bool PNGGetImageSize(const uint8_t *src, int *outWidth, int *outHeight);

// Decodes a PNG (already fully loaded into memory) into a newly allocated
// flat, row-major RGBA8 buffer (4 bytes/pixel, tightly packed, no tiling).
// width/height must match the image's actual dimensions exactly - use
// PNGGetImageSize() first if they aren't already known.
// Returns NULL on failure. Caller frees the result with mem1_free().
uint8_t * DecodePNGToRGBA8(const uint8_t *src, int width, int height);

// Encodes a flat, row-major RGB24 buffer (3 bytes/pixel) into a complete
// PNG file, held entirely in memory. stride is the row length in bytes
// (0 = tightly packed, width*3).
// Returns a newly allocated buffer containing the encoded PNG and sets
// *outSize to its length, or NULL on failure.
uint8_t * EncodePNGFromRGB24(uint32_t width, uint32_t height, const void *rgb, uint32_t stride, uint32_t *outSize);

#ifdef __cplusplus
}
#endif

#endif

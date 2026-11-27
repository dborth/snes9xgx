/****************************************************************************
 * msu1pcm.cpp
 *
 * MSU-1 .pcm decoder: 8-byte header ("MSU1" + 32-bit LE loop point in
 * frames), then raw 16-bit little-endian stereo at 44.1 kHz.
 * Implements MSU1AudioDecoder; see msu1audio.h for the threading contract.
 ***************************************************************************/
#include "snes9x.h"
#include "msu1.h"
#include "msu1audio.h"
#include "apu/blargg_endian.h"
#include <stdio.h>
#include <string.h>

#define PCM_HEADER_BYTES 8

class MSU1PcmDecoder : public MSU1AudioDecoder
{
	public:
		MSU1PcmDecoder(STREAM s, uint32_t loop) : stream(s), loop(loop) {}

		~MSU1PcmDecoder()
		{
			if (stream)
				CLOSE_STREAM(stream);
		}

		uint32_t read(int16_t * dst, uint32_t frames) override
		{
			size_t bytes = READ_STREAM((char *)dst, (size_t)frames * 4, stream);
			// With -DZLIB the stream is a gzFile, and gzread() returns -1 on error, which fStream::read()
			// hands back as a huge size_t. Anything above the request is an error: deliver nothing.
			if (bytes > (size_t)frames * 4)
				bytes = 0;
			uint32_t got = (uint32_t)(bytes / 4);	// a trailing partial frame is dropped, as before

			// File is little-endian; deliver native-endian int16 (a no-op on little-endian hosts).
			for (uint32_t i = 0; i < got * 2; i++)
				dst[i] = (int16_t)GET_LE16(&dst[i]);

			return got;
		}

		bool seek(uint32_t frame) override
		{
			// Success is NOT "== 0": with -DZLIB, REVERT_STREAM is gzseek(), which returns the new offset
			// (>= 0) on success and -1 on error; fseek() and unzStream::revert() return 0 on success.
			return REVERT_STREAM(stream, PCM_HEADER_BYTES + (int32)(frame * 4), 0) >= 0;
		}

		uint32_t loopFrame() const override { return loop; }

	private:
		STREAM stream;
		uint32_t loop;
};

MSU1AudioDecoder * S9xMSU1OpenPcmDecoder(uint16_t track)
{
	char ext[_MAX_EXT];
	snprintf(ext, _MAX_EXT, "-%d.pcm", track);

	STREAM s = S9xMSU1OpenFile(ext);
	if (!s)
		return NULL;

	uint8 header[PCM_HEADER_BYTES];
	if (READ_STREAM((char *)header, PCM_HEADER_BYTES, s) != PCM_HEADER_BYTES || memcmp(header, "MSU1", 4) != 0)
	{
		CLOSE_STREAM(s);
		return NULL;
	}

	uint32_t loop;
	memcpy(&loop, header + 4, 4);
	loop = GET_LE32(&loop);

	return new MSU1PcmDecoder(s, loop);
}

/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2010
 *
 * patch.cpp
 *
 * IPS/UPS/PPF patch support
 ***************************************************************************/

#include <zlib.h>

#include "snes9xgx.h"
#include "menu.h"
#include "memfile.h"
#include "fileop.h"
#include "filebrowser.h"
#include "snes9x/snes9x.h"
#include "snes9x/memmap.h"

static int readInt2(MFILE *f) {
	int res = 0;
	int c = memfgetc(f);
	if (c == MEOF)
		return -1;
	res = c;
	c = memfgetc(f);
	if (c == MEOF)
		return -1;
	return c + (res << 8);
}

static int readInt3(MFILE *f) {
	int res = 0;
	int c = memfgetc(f);
	if (c == MEOF)
		return -1;
	res = c;
	c = memfgetc(f);
	if (c == MEOF)
		return -1;
	res = c + (res << 8);
	c = memfgetc(f);
	if (c == MEOF)
		return -1;
	return c + (res << 8);
}

static s64 readInt4(MFILE *f) {
	s64 tmp, res = 0;
	int c;

	for (int i = 0; i < 4; i++) {
		c = memfgetc(f);
		if (c == MEOF)
			return -1;
		tmp = c;
		res = res + (tmp << (i * 8));
	}

	return res;
}

static s64 readVarPtr(MFILE *f) {
	s64 offset = 0, shift = 1;
	for (;;) {
		int c = memfgetc(f);
		if (c == MEOF)
			return 0;
		offset += (c & 0x7F) * shift;
		if (c & 0x80)
			break;
		shift <<= 7;
		offset += shift;
	}
	return offset;
}

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

static uLong computePatchCRC(MFILE *f, unsigned int size) {
	Bytef buf[4096];
	long readed;

	uLong crc = crc32(0L, Z_NULL, 0);
	do {
		readed = memfread(buf, 1, MIN(size, sizeof(buf)), f);
		crc = crc32(crc, buf, readed);
		size -= readed;
	} while (readed > 0);

	return crc;
}

bool patchApplyIPS(MFILE * f, bool header, u8 **r, int *s) {
	// from the IPS spec at http://zerosoft.zophar.net/ips.htm

	bool result = false;

	u8 *rom = *r;
	int size = *s;
	if (memfgetc(f) == 'P' && memfgetc(f) == 'A' && memfgetc(f) == 'T'
			&& memfgetc(f) == 'C' && memfgetc(f) == 'H') {
		int b;
		int offset;
		int len;

		result = true;

		for (;;) {
			// read offset
			offset = readInt3(f);
			// if offset == MEOF, end of patch
			if (offset == 0x454f46 || offset == -1)
				break;

			if(header)
				offset -= 512;

			// read length
			len = readInt2(f);
			if (!len) {
				// len == 0, RLE block
				len = readInt2(f);
				// byte to fill
				int c = memfgetc(f);
				if (c == -1)
					break;
				b = (u8) c;
			} else
				b = -1;
			// check if we need to reallocate our ROM
			if ((offset + len) >= size) {
#ifdef GEKKO
				size = offset + len;
#else
				size *= 2;
				rom = (u8 *) realloc(rom, size);
#endif
				*r = rom;
				*s = size;
			}
			if (b == -1) {
				// normal block, just read the data
				if (memfread(&rom[offset], 1, len, f) != (size_t) len)
					break;
			} else {
				// fill the region with the given byte
				while (len--) {
					rom[offset++] = b;
				}
			}
		}
	}
	return result;
}

bool patchApplyUPS(MFILE * f, bool header, u8 **rom, int *size) {

	s64 srcCRC, dstCRC, patchCRC;

	memfseek(f, 0, MSEEK_END);
	long int patchSize = memftell(f);
	if (patchSize < 20) {
		return false;
	}

	memfseek(f, 0, MSEEK_SET);

	if (memfgetc(f) != 'U' || memfgetc(f) != 'P' || memfgetc(f) != 'S'
			|| memfgetc(f) != '1') {
		return false;
	}

	memfseek(f, -12, MSEEK_END);
	srcCRC = readInt4(f);
	dstCRC = readInt4(f);
	patchCRC = readInt4(f);
	if (srcCRC == -1 || dstCRC == -1 || patchCRC == -1) {
		return false;
	}

	memfseek(f, 0, MSEEK_SET);
	u32 crc = computePatchCRC(f, patchSize - 4);

	if (crc != patchCRC) {
		return false;
	}

	crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, *rom, *size);

	memfseek(f, 4, MSEEK_SET);
	s64 dataSize;
	s64 srcSize = readVarPtr(f);
	s64 dstSize = readVarPtr(f);

	if (crc == srcCRC) {
		dataSize = srcSize;
	} else if (crc == dstCRC) {
		dataSize = dstSize;
	} else {
		return false;
	}
	if (dataSize != *size) {
		return false;
	}

	s64 relative = 0;

	if(header)
		relative -= 512;

	u8 *mem;
	while (memftell(f) < patchSize - 12) {
		relative += readVarPtr(f);
		if (relative > dataSize)
			continue;
		mem = *rom + relative;
		for (s64 i = relative; i < dataSize; i++) {
			int x = memfgetc(f);
			relative++;
			if (!x)
				break;
			if (i < dataSize) {
				*mem++ ^= x;
			}
		}
	}
	return true;
}

void WiiLoadPatch(bool header)
{
	int patchsize = 0;
	int patchtype;
	char patchpath[2][512];

	AllocSaveBuffer ();

	memset(patchpath, 0, sizeof(patchpath));
	sprintf(patchpath[0], "%s%s.ips", browser.dir, Memory.ROMFilename);
	sprintf(patchpath[1], "%s%s.ups", browser.dir, Memory.ROMFilename);

	for(patchtype=0; patchtype<2; patchtype++)
	{
		patchsize = LoadFile(patchpath[patchtype], SILENT);

		if(patchsize)
			break;
	}

	if(patchsize > 0)
	{
		// create memory file
		MFILE * mf = memfopen((char *)savebuffer, patchsize);

		int tmpSize = SNESROMSize;

		if(patchtype == 0)
			patchApplyIPS(mf, header, &Memory.ROM, &tmpSize);
		else
			patchApplyUPS(mf, header, &Memory.ROM, &tmpSize);

		SNESROMSize = tmpSize;

		memfclose(mf); // close memory file
	}

	FreeSaveBuffer ();
}

/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric October 2008
 *
 * patch.cpp
 *
 * IPS/UPS/PPF patch support
 ***************************************************************************/

#include <zlib.h>

#include "memmap.h"

#include "snes9xGX.h"
#include "menu.h"
#include "memfile.h"
#include "fileop.h"
#include "filebrowser.h"

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

static s64 readInt8(MFILE *f) {
	s64 tmp, res = 0;
	int c;

	for (int i = 0; i < 8; i++) {
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

bool patchApplyIPS(MFILE * f, u8 **r, int *s) {
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
#ifdef NGC
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

bool patchApplyUPS(MFILE * f, u8 **rom, int *size) {

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

static int ppfVersion(MFILE *f) {
	memfseek(f, 0, MSEEK_SET);
	if (memfgetc(f) != 'P' || memfgetc(f) != 'P' || memfgetc(f) != 'F')
		return 0;
	switch (memfgetc(f)) {
	case '1':
		return 1;
	case '2':
		return 2;
	case '3':
		return 3;
	default:
		return 0;
	}
}

int ppfFileIdLen(MFILE *f, int version) {
	if (version == 2) {
		memfseek(f, -8, MSEEK_END);
	} else {
		memfseek(f, -6, MSEEK_END);
	}

	if (memfgetc(f) != '.' || memfgetc(f) != 'D' || memfgetc(f) != 'I'
			|| memfgetc(f) != 'Z')
		return 0;

	return (version == 2) ? readInt4(f) : readInt2(f);
}

static bool patchApplyPPF1(MFILE *f, u8 **rom, int *size) {
	memfseek(f, 0, MSEEK_END);
	int count = memftell(f);
	if (count < 56)
		return false;
	count -= 56;

	memfseek(f, 56, MSEEK_SET);

	u8 *mem = *rom;

	while (count > 0) {
		int offset = readInt4(f);
		if (offset == -1)
			break;
		int len = memfgetc(f);
		if (len == MEOF)
			break;
		if (offset + len > *size)
			break;
		if (memfread(&mem[offset], 1, len, f) != (size_t) len)
			break;
		count -= 4 + 1 + len;
	}

	return (count == 0);
}

static bool patchApplyPPF2(MFILE *f, u8 **rom, int *size) {
	memfseek(f, 0, MSEEK_END);
	int count = memftell(f);
	if (count < 56 + 4 + 1024)
		return false;
	count -= 56 + 4 + 1024;

	memfseek(f, 56, MSEEK_SET);

	int datalen = readInt4(f);
	if (datalen != *size)
		return false;

	u8 *mem = *rom;

	u8 block[1024];
	memfread(&block, 1, 1024, f);
	if (memcmp(&mem[0x9320], &block, 1024) != 0)
		return false;

	int idlen = ppfFileIdLen(f, 2);
	if (idlen > 0)
		count -= 16 + 16 + idlen;

	memfseek(f, 56 + 4 + 1024, MSEEK_SET);

	while (count > 0) {
		int offset = readInt4(f);
		if (offset == -1)
			break;
		int len = memfgetc(f);
		if (len == MEOF)
			break;
		if (offset + len > *size)
			break;
		if (memfread(&mem[offset], 1, len, f) != (size_t) len)
			break;
		count -= 4 + 1 + len;
	}

	return (count == 0);
}

static bool patchApplyPPF3(MFILE *f, u8 **rom, int *size) {
	memfseek(f, 0, MSEEK_END);
	int count = memftell(f);
	if (count < 56 + 4 + 1024)
		return false;
	count -= 56 + 4;

	memfseek(f, 56, MSEEK_SET);

	int imagetype = memfgetc(f);
	int blockcheck = memfgetc(f);
	int undo = memfgetc(f);
	memfgetc(f);

	u8 *mem = *rom;

	if (blockcheck) {
		u8 block[1024];
		memfread(&block, 1, 1024, f);
		if (memcmp(&mem[(imagetype == 0) ? 0x9320 : 0x80A0], &block, 1024) != 0)
			return false;
		count -= 1024;
	}

	int idlen = ppfFileIdLen(f, 2);
	if (idlen > 0)
		count -= 16 + 16 + idlen;

	memfseek(f, 56 + 4 + (blockcheck ? 1024 : 0), MSEEK_SET);

	while (count > 0) {
		s64 offset = readInt8(f);
		if (offset == -1)
			break;
		int len = memfgetc(f);
		if (len == MEOF)
			break;
		if (offset + len > *size)
			break;
		if (memfread(&mem[offset], 1, len, f) != (size_t) len)
			break;
		if (undo)
			memfseek(f, len, MSEEK_CUR);
		count -= 8 + 1 + len;
		if (undo)
			count -= len;
	}

	return (count == 0);
}

bool patchApplyPPF(MFILE *f, u8 **rom, int *size)
{
	bool res = false;

	int version = ppfVersion(f);
	switch (version)
	{
		case 1: res = patchApplyPPF1(f, rom, size); break;
		case 2: res = patchApplyPPF2(f, rom, size); break;
		case 3: res = patchApplyPPF3(f, rom, size); break;
	}

	return res;
}

void LoadPatch()
{
	int patchsize = 0;
	int patchtype;
	char patchpath[3][512];

	AllocSaveBuffer ();

	memset(patchpath, 0, sizeof(patchpath));
	sprintf(patchpath[0], "%s%s.ips", browser.dir, Memory.ROMFilename);
	sprintf(patchpath[1], "%s%s.ups", browser.dir, Memory.ROMFilename);
	sprintf(patchpath[2], "%s%s.ppf", browser.dir, Memory.ROMFilename);

	for(patchtype=0; patchtype<3; patchtype++)
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
			patchApplyIPS(mf, &Memory.ROM, &tmpSize);
		else if(patchtype == 1)
			patchApplyUPS(mf, &Memory.ROM, &tmpSize);
		else
			patchApplyPPF(mf, &Memory.ROM, &tmpSize);

		SNESROMSize = tmpSize;

		memfclose(mf); // close memory file
	}

	FreeSaveBuffer ();
}

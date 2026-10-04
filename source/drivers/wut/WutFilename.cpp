/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutFilename.cpp
 *
 * The Wii U's FSA returns non-ASCII FAT long file names as CP932 bytes
 * ("Shift-JIS") rather than UTF-8
 * This converts names to UTF-8 for display only - the raw name must still
 * be used for all file I/O.
 ***************************************************************************/

#include <errno.h>
#include <iconv.h>
#include <stdint.h>
#include <string.h>

#include "WutFilename.h"

// Strict UTF-8 validation (no overlongs, surrogates, or > U+10FFFF)
bool WutIsValidUtf8(const char * str)
{
	const unsigned char * s = (const unsigned char *)str;

	while(*s)
	{
		if(*s < 0x80) { ++s; continue; }

		int extra;
		uint32_t cp, minCp;

		if(*s >= 0xC2 && *s <= 0xDF)      { extra = 1; cp = *s & 0x1F; minCp = 0x80; }
		else if(*s >= 0xE0 && *s <= 0xEF) { extra = 2; cp = *s & 0x0F; minCp = 0x800; }
		else if(*s >= 0xF0 && *s <= 0xF4) { extra = 3; cp = *s & 0x07; minCp = 0x10000; }
		else return false;

		for(int i = 1; i <= extra; i++)
		{
			if((s[i] & 0xC0) != 0x80)
				return false;
			cp = (cp << 6) | (s[i] & 0x3F);
		}

		if(cp < minCp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
			return false;

		s += 1 + extra;
	}
	return true;
}

// Opens a CP932 -> UTF-8 converter. Not every libc names it the same way,
// so try the Windows variant first, then plain Shift-JIS.
static iconv_t openLegacyConverter()
{
	static const char * const names[] = { "CP932", "WINDOWS-31J", "SHIFT_JIS" };

	for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
	{
		iconv_t cd = iconv_open("UTF-8", names[i]);
		if(cd != (iconv_t)-1)
			return cd;
	}
	return (iconv_t)-1;
}

// Appends one undecodable byte as its Latin-1 code point. False if it won't fit.
static bool putLatin1(char * out, size_t outSize, size_t & pos, unsigned char b)
{
	size_t need = b < 0x80 ? 1 : 2;

	if(pos + need + 1 > outSize) // keep room for the terminator
		return false;

	if(need == 1)
		out[pos++] = (char)b;
	else
	{
		out[pos++] = (char)(0xC0 | (b >> 6));
		out[pos++] = (char)(0x80 | (b & 0x3F));
	}
	return true;
}

// newlib's CP932 decodes a few characters the JIS X 0208 way where Windows
// (which is what writes these names) uses a different code point. The Windows
// ones are in the bundled CJK fonts; the JIS ones are missing from some of
// them (e.g. the wave dash U+301C is not in ko.ttf). Each pair below is the
// same length in UTF-8, so this is done in place.
static void remapJisToWindows(char * s)
{
	static const struct { unsigned char from[3]; unsigned char to[3]; } map[] = {
		{ { 0xE3, 0x80, 0x9C }, { 0xEF, 0xBD, 0x9E } }, // U+301C WAVE DASH   -> U+FF5E FULLWIDTH TILDE (0x8160)
		{ { 0xE2, 0x80, 0x96 }, { 0xE2, 0x88, 0xA5 } }, // U+2016 DOUBLE VERT -> U+2225 PARALLEL TO     (0x8161)
		{ { 0xE2, 0x88, 0x92 }, { 0xEF, 0xBC, 0x8D } }, // U+2212 MINUS SIGN  -> U+FF0D FULLWIDTH MINUS (0x817C)
	};

	unsigned char * p = (unsigned char *)s;

	for(; p[0]; p++)
	{
		if(p[0] != 0xE2 && p[0] != 0xE3) // only lead bytes can start a match
			continue;

		for(size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
		{
			if(p[0] == map[i].from[0] && p[1] == map[i].from[1] && p[2] == map[i].from[2])
			{
				p[0] = map[i].to[0];
				p[1] = map[i].to[1];
				p[2] = map[i].to[2];
				p += 2;
				break;
			}
		}
	}
}

static void legacyToUtf8(const char * in, char * out, size_t outSize)
{
	// A descriptor per call keeps this thread-safe (iconv_t carries state), and
	// this only runs for the rare names that are not valid UTF-8.
	iconv_t cd = openLegacyConverter();

	const char * src = in;
	size_t srcLeft = strlen(in);
	size_t pos = 0;

	while(srcLeft > 0)
	{
		if(cd != (iconv_t)-1)
		{
			char * inp = (char *)src;
			char * outp = out + pos;
			size_t outLeft = outSize - 1 - pos;

			errno = 0;
			size_t rc = iconv(cd, &inp, &srcLeft, &outp, &outLeft);
			int err = errno;

			// iconv never writes a partial character, so progress is always safe
			pos = (size_t)(outp - out);
			src = inp;

			if(rc != (size_t)-1 || srcLeft == 0)
				break;

			if(err == E2BIG)
				break; // output full
			// EILSEQ / EINVAL: fall through and pass the offending byte as Latin-1

			iconv(cd, NULL, NULL, NULL, NULL); // reset conversion state
		}

		if(!putLatin1(out, outSize, pos, (unsigned char)*src))
			break;
		src++;
		srcLeft--;
	}

	if(cd != (iconv_t)-1)
		iconv_close(cd);

	out[pos] = '\0';
	remapJisToWindows(out);
}

void WutNameToUtf8(const char * rawName, char * out, size_t outSize)
{
	if(outSize == 0)
		return;

	if(!rawName)
	{
		out[0] = '\0';
		return;
	}

	if(!WutIsValidUtf8(rawName))
	{
		legacyToUtf8(rawName, out, outSize);
		return;
	}

	// Already UTF-8 (includes plain ASCII, and USB volumes mounted via libdvm)
	size_t pos = 0;
	while(rawName[pos] && pos + 1 < outSize)
	{
		out[pos] = rawName[pos];
		pos++;
	}

	// Don't leave a truncated multi-byte sequence at the end
	if(rawName[pos])
	{
		size_t i = pos;
		while(i > 0 && ((unsigned char)out[i - 1] & 0xC0) == 0x80)
			i--;

		if(i > 0)
		{
			unsigned char lead = (unsigned char)out[i - 1];
			size_t len = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
			if(pos - (i - 1) < len)
				pos = i - 1; // incomplete final character - drop it
		}
	}
	out[pos] = '\0';
}

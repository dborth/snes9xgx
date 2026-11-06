/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * safestr.h
 *
 * Bounded string helpers that report truncation instead of hiding it.
 *
 * A path or file name that was silently cut short is worse than one that
 * could not be built: it still names *something*, so a load or a save can
 * end up touching a different file than the one that was asked for. These
 * helpers always terminate the destination and tell the caller whether
 * everything fit, so the caller decides what to do about it.
 ***************************************************************************/
#ifndef _SAFESTR_H_
#define _SAFESTR_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

//! snprintf() that says whether the result fit. When it did not, dst is emptied
//! (never left holding a cut-off name) and false is returned. dstSize includes the terminator.
inline bool FormatString(char * dst, size_t dstSize, const char * fmt, ...) __attribute__((format(printf, 3, 4)));

inline bool FormatString(char * dst, size_t dstSize, const char * fmt, ...)
{
	if(dstSize == 0)
		return false;

	va_list args;
	va_start(args, fmt);
	int written = vsnprintf(dst, dstSize, fmt, args);
	va_end(args);

	if(written < 0 || (size_t)written >= dstSize)
	{
		dst[0] = '\0';
		return false;
	}
	return true;
}

//! Copies src into dst (dstSize includes the terminator), always terminating it. A src that does not
//! fit is cut on a UTF-8 character boundary, never inside a multi-byte sequence, so it stays valid
//! text; false is returned in that case. For text that is only displayed - never for a path.
inline bool CopyString(char * dst, size_t dstSize, const char * src)
{
	if(dstSize == 0)
		return false;

	if(!src)
		src = "";

	size_t len = strlen(src);
	const bool fits = len < dstSize;

	if(!fits)
	{
		len = dstSize - 1;

		// src[len] is the first dropped byte. A continuation byte (10xxxxxx) means the
		// character straddles the cut, so drop all of it.
		while(len > 0 && ((unsigned char)src[len] & 0xC0) == 0x80)
			len--;
	}

	memcpy(dst, src, len);
	dst[len] = '\0';
	return fits;
}

#endif

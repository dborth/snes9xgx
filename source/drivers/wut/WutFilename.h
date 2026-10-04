/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutFilename.h
 *
 * Filename decoding helpers for Wii U
 ***************************************************************************/
#pragma once

#include <stddef.h>

//! True if str is well-formed UTF-8 (plain ASCII counts)
bool WutIsValidUtf8(const char * str);

//! Converts a raw FSA directory-entry name to UTF-8 for display: names that are
//! already valid UTF-8 are copied unchanged, anything else is decoded as
//! CP932 (Shift-JIS) with newlib's iconv. Always terminates out and never
//! writes a partial character. The raw name must still be used for file I/O.
void WutNameToUtf8(const char * rawName, char * out, size_t outSize);

/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiTextTranslator.cpp
 ***************************************************************************/
#include "GuiTextTranslator.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <type_traits>

/* The catalog format is GNU gettext's .mo (see "The Format of GNU MO Files" in the gettext manual):
 *
 *   header, 7 words:  magic, revision, N, O, T, S, H
 *   at O:             N x { length, offset } of the original strings (the message IDs), sorted by byte value
 *   at T:             N x { length, offset } of the translations, in the same order
 *   (S, H: an optional hash table, which is not used here)
 *   the strings themselves, each followed by a NUL that the length does not count
 *
 * Words are in the byte order of the machine that ran msgfmt, which the magic number tells, so they are read a byte at a
 * time: the same catalog works on the big-endian consoles and on a host of either order, at any alignment. */

static_assert(std::is_trivially_destructible<GuiTextTranslator>::value,
		"the translator holds no resources, so the global instance never has anything to destroy at exit");

// A constexpr object exists only if the constructor can run at compile time, which is what makes textTranslator
// constant-initialized (valid before any dynamic initializer, whatever the link order).
namespace { constexpr GuiTextTranslator constantInitializationCheck{}; }

static GuiTextTranslator translatorInstance;
GuiTextTranslator* textTranslator = &translatorInstance;

static const uint32_t MO_HEADER_SIZE = 28;
static const uint32_t MO_MAGIC = 0x950412DEu;

static inline uint32_t readWord(const uint8_t* p, bool bigEndian)
{
	if(bigEndian)
		return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
	return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[0];
}

//! A string of the catalog: it must lie inside it, end in a NUL that its length does not count, and hold no other NUL
static bool stringIsSound(const uint8_t* base, size_t size, uint32_t length, uint32_t offset)
{
	if(length > GuiTextTranslator::MAX_STRING_LENGTH)
		return false;
	if((uint64_t)offset + length + 1 > size)
		return false;
	if(base[offset + length] != 0)
		return false;
	return length == 0 || memchr(base + offset, 0, length) == nullptr;
}

//! Compares a C string with a string of a given length: <0, 0 or >0 as strcmp would, comparing bytes as unsigned
static int compareString(const char* key, const uint8_t* s, uint32_t length)
{
	for(uint32_t i = 0; i < length; i++)
	{
		const uint8_t k = (uint8_t)key[i];
		if(k == 0)
			return -1;
		if(k != s[i])
			return k < s[i] ? -1 : 1;
	}
	return key[length] == 0 ? 0 : 1;
}

/* Checks everything that a lookup relies on, so that a lookup has nothing left to trust:
 *  - the magic number (either byte order), and a revision this reader understands (0 and 1 differ only in system-
 *    dependent strings, which are not used);
 *  - a sane message count, and both tables inside the data and clear of the header;
 *  - every string inside the data, ending in a NUL, with no NUL in it, and not longer than MAX_STRING_LENGTH;
 *  - the message IDs in strictly ascending byte order, which is what makes a binary search correct and means there
 *    are no duplicates.
 * Whether a translation is UTF-8 is checked when it is looked up, so that one bad translation does not cost the rest. */
bool GuiTextTranslator::check(const uint8_t* catalog, size_t size, Table& out)
{
	if(!catalog || size < MO_HEADER_SIZE || size > 0x7FFFFFFFu)
		return false;

	bool bigEndian;
	if(readWord(catalog, false) == MO_MAGIC)
		bigEndian = false;
	else if(readWord(catalog, true) == MO_MAGIC)
		bigEndian = true;
	else
		return false;

	if((readWord(catalog + 4, bigEndian) >> 16) > 1)
		return false;

	const uint32_t count = readWord(catalog + 8, bigEndian);
	const uint32_t origOffset = readWord(catalog + 12, bigEndian);
	const uint32_t transOffset = readWord(catalog + 16, bigEndian);

	if(count == 0 || count > MAX_MESSAGES)
		return false;

	const uint64_t tableBytes = (uint64_t)count * 8;
	if(origOffset < MO_HEADER_SIZE || (uint64_t)origOffset + tableBytes > size)
		return false;
	if(transOffset < MO_HEADER_SIZE || (uint64_t)transOffset + tableBytes > size)
		return false;

	const uint8_t* prev = nullptr;
	uint32_t prevLength = 0;

	for(uint32_t i = 0; i < count; i++)
	{
		const uint8_t* o = catalog + origOffset + (size_t)i * 8;
		const uint8_t* t = catalog + transOffset + (size_t)i * 8;
		const uint32_t origLength = readWord(o, bigEndian), origPos = readWord(o + 4, bigEndian);
		const uint32_t transLength = readWord(t, bigEndian), transPos = readWord(t + 4, bigEndian);

		if(!stringIsSound(catalog, size, origLength, origPos) || !stringIsSound(catalog, size, transLength, transPos))
			return false;

		const uint8_t* id = catalog + origPos;
		if(prev)
		{
			const uint32_t common = prevLength < origLength ? prevLength : origLength;
			const int c = memcmp(prev, id, common);
			if(c > 0 || (c == 0 && prevLength >= origLength))
				return false;
		}
		prev = id;
		prevLength = origLength;
	}

	out.base = catalog;
	out.size = size;
	out.count = count;
	out.origOffset = origOffset;
	out.transOffset = transOffset;
	out.bigEndian = bigEndian;
	return true;
}

//! Makes a table the one in use (or, with an empty table, makes there be none). One thread at a time.
void GuiTextTranslator::publish(const Table& table)
{
	const uint32_t s = sequence.load(std::memory_order_relaxed);
	sequence.store(s + 1, std::memory_order_relaxed);			// odd: being changed
	std::atomic_thread_fence(std::memory_order_release);
	base.store(table.base, std::memory_order_relaxed);
	size.store(table.size, std::memory_order_relaxed);
	count.store(table.count, std::memory_order_relaxed);
	origOffset.store(table.origOffset, std::memory_order_relaxed);
	transOffset.store(table.transOffset, std::memory_order_relaxed);
	bigEndian.store(table.bigEndian ? 1 : 0, std::memory_order_relaxed);
	sequence.store(s + 2, std::memory_order_release);			// even: complete
}

//! Copies the table in use. False if there is none, or if it was being changed while it was read (the copy is then not
//! to be trusted, and nothing waits for the change to finish: the caller simply has no translation this time).
bool GuiTextTranslator::snapshot(Table& out) const
{
	const uint32_t before = sequence.load(std::memory_order_acquire);
	if(before & 1)
		return false;

	out.base = base.load(std::memory_order_relaxed);
	out.size = size.load(std::memory_order_relaxed);
	out.count = count.load(std::memory_order_relaxed);
	out.origOffset = origOffset.load(std::memory_order_relaxed);
	out.transOffset = transOffset.load(std::memory_order_relaxed);
	out.bigEndian = bigEndian.load(std::memory_order_relaxed) != 0;

	std::atomic_thread_fence(std::memory_order_acquire);
	if(sequence.load(std::memory_order_relaxed) != before)
		return false;
	return out.base != nullptr && out.count != 0;
}

bool GuiTextTranslator::loadLanguage(const uint8_t* catalog, size_t length)
{
	Table checked;
	if(!check(catalog, length, checked))
	{
		clear();
		return false;
	}

	publish(checked);
	return true;
}

void GuiTextTranslator::clear()
{
	publish(Table());
}

size_t GuiTextTranslator::getCount() const
{
	Table t;
	return snapshot(t) ? t.count : 0;
}

//! Whether s is well-formed UTF-8: no overlong forms, no surrogates, nothing above U+10FFFF, no truncated sequence
bool GuiTextTranslator::isValidUtf8(const char* s, size_t length)
{
	const uint8_t* p = (const uint8_t*)s;
	const uint8_t* end = p + length;

	while(p < end)
	{
		const uint8_t c = *p++;
		if(c < 0x80)
			continue;

		size_t extra;
		uint8_t lo = 0x80, hi = 0xBF; // the allowed range of the first continuation byte
		if(c >= 0xC2 && c <= 0xDF)
			extra = 1;
		else if(c >= 0xE0 && c <= 0xEF)
		{
			extra = 2;
			if(c == 0xE0) lo = 0xA0;		// not an overlong form
			else if(c == 0xED) hi = 0x9F;	// not a surrogate
		}
		else if(c >= 0xF0 && c <= 0xF4)
		{
			extra = 3;
			if(c == 0xF0) lo = 0x90;		// not an overlong form
			else if(c == 0xF4) hi = 0x8F;	// not above U+10FFFF
		}
		else
			return false;

		if((size_t)(end - p) < extra)
			return false;
		if(*p < lo || *p > hi)
			return false;
		p++;
		for(size_t i = 1; i < extra; i++)
			if((*p++ & 0xC0) != 0x80)
				return false;
	}
	return true;
}

const char* GuiTextTranslator::find(const char* msgid) const
{
	if(!msgid || !*msgid)
		return nullptr;

	Table table;
	if(!snapshot(table))
		return nullptr;
	const Table* t = &table;

	size_t lo = 0, hi = t->count;
	while(lo < hi)
	{
		const size_t mid = lo + (hi - lo) / 2;
		const uint8_t* o = t->base + t->origOffset + mid * 8;
		const uint32_t length = readWord(o, t->bigEndian), offset = readWord(o + 4, t->bigEndian);

		// checked when the catalog was loaded; checked again so a lookup can never leave the catalog
		if((uint64_t)offset + length > t->size)
			return nullptr;

		const int c = compareString(msgid, t->base + offset, length);
		if(c == 0)
		{
			const uint8_t* tr = t->base + t->transOffset + mid * 8;
			const uint32_t transLength = readWord(tr, t->bigEndian), transOffset = readWord(tr + 4, t->bigEndian);

			if(transLength == 0 || (uint64_t)transOffset + transLength + 1 > t->size)
				return nullptr;
			const char* text = (const char*)t->base + transOffset;
			if(text[transLength] != '\0' || !isValidUtf8(text, transLength))
				return nullptr;
			return text;
		}
		if(c < 0)
			hi = mid;
		else
			lo = mid + 1;
	}
	return nullptr;
}

const char* GuiTextTranslator::getText(const char* msgid) const
{
	if(!msgid)
		return nullptr;

	const char* text = find(msgid);
	return text ? text : msgid;
}

// Finds the next printf conversion of s (from its '%' to its conversion letter, so "%-5.2f" is one): its start and
// length go to spec and spec_length, and the return value is where it ends, or nullptr when there are no more.
// "%%" is not a conversion.
static const char* nextConversion(const char* s, const char*& spec, size_t& specLength)
{
	while(*s)
	{
		if(*s++ != '%')
			continue;
		if(*s == '%')
		{
			s++;
			continue;
		}
		spec = s;
		while(*s && !strchr("diouxXeEfFgGaAcspn", *s))
			s++;
		if(*s)
			s++;
		specLength = (size_t)(s - spec);
		return s;
	}
	return nullptr;
}

bool GuiTextTranslator::sameConversions(const char* a, const char* b)
{
	if(!a || !b)
		return false;

	const char *specA = nullptr, *specB = nullptr;
	size_t lengthA = 0, lengthB = 0;
	do
	{
		a = nextConversion(a, specA, lengthA);
		b = nextConversion(b, specB, lengthB);
	} while(a && b && lengthA == lengthB && memcmp(specA, specB, lengthA) == 0);

	return !a && !b; // equal only if both ran out together; a mismatched conversion leaves a and b non-null
}

std::string Tr(const char* msgid)
{
	return msgid ? std::string(textTranslator->getText(msgid)) : std::string();
}

std::string TrFormat(const char* msgid, ...)
{
	if(!msgid)
		return std::string();

	const char* fmt = textTranslator->find(msgid);
	if(!fmt || !GuiTextTranslator::sameConversions(fmt, msgid))
		fmt = msgid;

	va_list args;
	va_start(args, msgid);
	va_list copy;
	va_copy(copy, args);
	const int n = vsnprintf(nullptr, 0, fmt, copy);
	va_end(copy);

	std::string out;
	if(n > 0)
	{
		out.resize((size_t)n + 1);
		vsnprintf(&out[0], out.size(), fmt, args);
		out.resize((size_t)n);
	}
	va_end(args);
	return out;
}

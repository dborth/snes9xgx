/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiTextTranslator.h
 ***************************************************************************/
#pragma once

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstddef>
#include <string>

//!Looks up translated UTF-8 text by its English message ID.
//!
//!The translations are GNU gettext message catalogs (.mo), compiled from the .lang source files by the build
//!(msgfmt) and embedded in the program. loadLanguage() checks a catalog once and then reads it where it lies: the
//!translator never allocates, never copies a string and never parses text, and a lookup is a binary search of the
//!catalog's sorted message IDs, compared exactly (there is no hashing, so two IDs can never be mistaken for each other).
//!
//!A catalog that is not entirely sound is not used at all, and a single translation that is not sound (empty, or not
//!valid UTF-8) is not returned: in both cases the caller gets the English text back. There is no state to corrupt:
//!all the translator holds is a pointer to the catalog and a few numbers read from its header.
//!
//!The catalog must stay where it is, unchanged, for the life of the program (embedded data always does): a lookup that
//!began before a catalog was replaced may still be reading the old one.
//!
//!Any thread may look text up while a language is loaded or cleared. A lookup that overlaps the switch is not held up
//!and does not guess: it returns the English text, once. Only one thread may load or clear at a time.
//!\ingroup grp_core
class GuiTextTranslator
{
	public:
		//! constexpr, with nothing to destroy, on purpose: the instance (textTranslator) is then constant-initialized (it
		//! is laid out in the binary and no constructor runs at startup), so it is already valid for any static initializer
		//! in any translation unit, whatever the link order.
		constexpr GuiTextTranslator() : sequence(0), base(nullptr), size(0), count(0), origOffset(0), transOffset(0), bigEndian(0) {}

		//! Checks a compiled message catalog and starts using it, replacing the one in use. A catalog that fails any
		//! check (see the .cpp for the list) is not used at all: the translator is left with no catalog, so every
		//! lookup returns the English text.
		//!\param catalog The .mo file's data. It is not copied, so it must outlive its use here.
		//!\param length Length of the data in bytes
		//!\return true if the catalog was sound and is now in use, false otherwise
		bool loadLanguage(const uint8_t* catalog, size_t length);

		//! Stops using the catalog. The English text is returned from then on, which is how English is selected.
		void clear();

		//! Whether a catalog is in use
		bool isLoaded() const { return getCount() != 0; }

		//! The number of messages in the catalog in use (0 if none)
		size_t getCount() const;

		//! Finds the translation of a message ID
		//!\param msgid The English text/ID
		//!\return The translation, or nullptr if there is none or it is not sound. Points into the catalog.
		const char* find(const char* msgid) const;

		//! Retrieves the translated UTF-8 string for a message ID
		//!\param msgid The English text/ID to translate
		//!\return The translated string, or msgid itself if there is no translation (nullptr if msgid is nullptr)
		const char* getText(const char* msgid) const;

		//! Whether two printf formats take the same arguments: the same conversions, in the same order
		static bool sameConversions(const char* a, const char* b);

		//! Where to cut UTF-8 text that was cut short (by a buffer that was too small) so that it ends on a whole character
		//!\param s UTF-8 text, of which only s[0..n) is read; need not be NUL-terminated
		//!\param n Length of what was kept, in bytes
		//!\return n, or less if the last character in s[0..n) is incomplete (so that it can be dropped)
		static size_t utf8Trim(const char* s, size_t n)
		{
			size_t i = n, continuation = 0;
			while(i > 0 && continuation < 3 && ((unsigned char)s[i - 1] & 0xC0) == 0x80)
			{
				i--;
				continuation++;
			}
			if(i == 0)
				return n; // nothing but continuation bytes: no character was cut, the text is just not valid
			const unsigned char lead = (unsigned char)s[i - 1];
			const size_t need = lead >= 0xF0 ? 4 : (lead >= 0xE0 ? 3 : (lead >= 0xC0 ? 2 : 1));
			return need > continuation + 1 ? i - 1 : n;
		}

		//! The longest message ID or translation (in bytes) a catalog may hold
		static constexpr uint32_t MAX_STRING_LENGTH = 1024;
		//! The most messages a catalog may hold
		static constexpr uint32_t MAX_MESSAGES = 16384;

	private:
		//! What a lookup needs to know about the catalog in use, read from its header when it was checked
		struct Table
		{
			const uint8_t* base;	//!< The catalog (nullptr when there is none)
			size_t size;			//!< Its length
			uint32_t count;			//!< Number of messages
			uint32_t origOffset;	//!< Offset of the table of message IDs (count entries of {length, offset})
			uint32_t transOffset;	//!< Offset of the table of translations, in the same order
			bool bigEndian;			//!< The catalog's words are big-endian

			constexpr Table() : base(nullptr), size(0), count(0), origOffset(0), transOffset(0), bigEndian(false) {}
		};

		//! The table in use, as a seqlock: the loader makes `sequence` odd, changes the fields, and makes it even again; a
		//! lookup copies the fields and keeps the copy only if `sequence` was even and unchanged. Each field is atomic (and
		//! only ever accessed relaxed) so that a copy that is thrown away is not a data race.
		std::atomic<uint32_t> sequence;
		std::atomic<const uint8_t*> base;
		std::atomic<size_t> size;
		std::atomic<uint32_t> count, origOffset, transOffset, bigEndian;

		void publish(const Table& table);
		bool snapshot(Table& out) const;
		static bool check(const uint8_t* catalog, size_t size, Table& out);
		static bool isValidUtf8(const char* s, size_t length);
};

//! The one translator. Defined in GuiTextTranslator.cpp.
extern GuiTextTranslator* textTranslator;

//! Translates a message ID, for text that is put together at runtime. GuiText only translates the exact string it
//! is given, so a message built with printf or operator+ has to have its parts translated first. Use TrFormat() for a
//! message with values in it.
//!
//! Nothing is copied or allocated: this is the translation where it lies in the catalog, which stays valid for the
//! life of the program. When there is no translation it is msgid itself, which is only as long-lived as msgid is.
//!\param msgid The English text/ID to translate
//!\return The translated string, or msgid itself when there is no translation (an empty string if msgid is nullptr)
//!\ingroup grp_core
const char* Tr(const char* msgid);

//! printf for translated messages. msgid is both the lookup key and the English format string,
//! so its conversions are checked against the arguments at compile time. If the translation does
//! not have the same conversions in the same order (a typo in a .lang file) the English text is
//! used instead, so a bad translation can never be run with the wrong arguments.
//!
//! This allocates a std::string (as does anything that holds the result as one). To show a message once, use
//! TrMessage, or GuiText::setFormat() for a label; to keep it as a string, this is the one to use.
//!\param msgid The English format string, e.g. "Player %d"
//!\return The formatted, translated string
//!\ingroup grp_core
#if defined(__GNUC__)
std::string TrFormat(const char* msgid, ...) __attribute__((format(printf, 1, 2)));
#else
std::string TrFormat(const char* msgid, ...);
#endif

//! TrFormat() into a buffer the caller owns, with no allocation. The result is always NUL-terminated and, if the
//! buffer is too small, cut on a whole character.
//!\param out Where to write
//!\param size Size of out in bytes (nothing is written if it is 0)
//!\param msgid The English format string
//!\param args The arguments of the format
//!\return The length of what was written, not counting the NUL
//!\ingroup grp_core
int TrFormatV(char* out, size_t size, const char* msgid, va_list args);

//! TrFormatV() with the arguments written out
//!\ingroup grp_core
#if defined(__GNUC__)
int TrFormatTo(char* out, size_t size, const char* msgid, ...) __attribute__((format(printf, 3, 4)));
#else
int TrFormatTo(char* out, size_t size, const char* msgid, ...);
#endif

//! A translated, formatted message held in the object itself (256 bytes unless N says otherwise, cut on a whole
//! character if it does not fit), for a message that is used once and thrown away - as the argument of a call, say:
//! `ErrorPrompt(TrMessage<>("Backup %d", slot))`. It converts to const char*, valid as long as the object is.
//! No heap is used; use TrFormat() for a message that has to be kept as a std::string.
//!\ingroup grp_core
template <size_t N = 256>
class TrMessage
{
	static_assert(N >= 2, "a message needs room for at least one character and the NUL");

	public:
#if defined(__GNUC__)
		explicit TrMessage(const char* msgid, ...) __attribute__((format(printf, 2, 3)))
#else
		explicit TrMessage(const char* msgid, ...)
#endif
		{
			va_list args;
			va_start(args, msgid);
			TrFormatV(text, N, msgid, args);
			va_end(args);
		}

		const char* c_str() const { return text; }
		operator const char*() const { return text; }

	private:
		char text[N];
};

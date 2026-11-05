/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiTextTranslator.h
 ***************************************************************************/
#pragma once

#include <cstdint>
#include <cstddef>

//!Hashed msgid -> UTF-8 string table loaded from a single binary .lang
//!blob (see loadLanguage()). GuiText and other classes call getText() to
//!resolve a message ID to translated text.
//!\ingroup grp_core
class GuiTextTranslator
{
	public:
		//! constexpr on purpose: an instance defined at namespace scope is then constant-initialized
		//! (it is laid out in the binary and no constructor runs at startup), so it is already valid
		//! for any static initializer in any translation unit, whatever the link order.
		constexpr GuiTextTranslator() : magic(MAGIC), head(nullptr) {}
		~GuiTextTranslator();

		//! Loads and parses a translation file from a memory buffer
		//!\param buffer Pointer to the loaded file data
		//!\param size Length of the buffer in bytes
		//!\return true if successful, false otherwise
		bool loadLanguage(const uint8_t* buffer, size_t size);

		//! Retrieves the translated UTF-8 string for a given ASCII message ID
		//!\param msgid The English text/ID to translate
		//!\return The translated string, or the original msgid if not found
		const char* getText(const char* msgid) const;

		//! Clears the current translation dictionary and frees memory
		void clear();

	private:
		static constexpr uint32_t MAGIC = 0x47545254; // "GTRT": a live, intact translator

		struct MSG
		{
			uint32_t id = 0;
			char* msgstr = nullptr;
			MSG* next = nullptr;
		};

		uint32_t magic; // MAGIC while alive; checked on lookup so a stomped object is caught early
		MSG* head;

		//! Internal helper methods
		static uint32_t hashString(const char* str);
		static char* expandEscape(const char* str);

		//! Safely reads a line from a non-null-terminated memory buffer
		const char* memFGets(char* dst, int maxlen, const char* src, const char* eof) const;

		//! Stops the program at the first sign that the object or its message list has been overwritten
		//! (see the definition for what the crash screen then shows). Never returns.
		[[noreturn]] static void fatalCorruption(const GuiTextTranslator* self, const void* bad,
				const void* from, uint32_t magicSeen);
		//! Aborts through fatalCorruption() unless the object and the given list node look sane
		void checkIntegrity(const MSG* node, const MSG* prev) const;

		MSG* findMessage(uint32_t id) const;
		MSG* setMessage(const char* msgid, const char* msgstr);
};

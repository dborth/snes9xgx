/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiTextTranslator.h
 ***************************************************************************/
#pragma once

#include <cstdint>
#include <cstddef>

class GuiTextTranslator
{
	public:
		GuiTextTranslator();
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
		struct MSG
		{
			uint32_t id;
			char* msgstr;
			MSG* next;
		};

		MSG* head;

		//! Internal helper methods
		static uint32_t hashString(const char* str);
		static char* expandEscape(const char* str);

		//! Safely reads a line from a non-null-terminated memory buffer
		const char* memFGets(char* dst, int maxlen, const char* src, const char* eof) const;

		MSG* findMessage(uint32_t id) const;
		MSG* setMessage(const char* msgid, const char* msgstr);
};

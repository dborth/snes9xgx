/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiTextTranslator.cpp
 ***************************************************************************/
#include "GuiTextTranslator.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

#define HASHWORDBITS 32

GuiTextTranslator::GuiTextTranslator() :
		head(nullptr) {
}

GuiTextTranslator::~GuiTextTranslator() {
	clear();
}

/* Defines the so-called `hashpjw' function by P.J. Weinberger */
uint32_t GuiTextTranslator::hashString(const char *str_param) {
	uint32_t hval = 0, g;
	const char *str = str_param;

	while (*str != '\0') {
		hval <<= 4;
		hval += (uint8_t) * str++;
		g = hval & ((uint32_t) 0xf << (HASHWORDBITS - 4));
		if (g != 0) {
			hval ^= g >> (HASHWORDBITS - 8);
			hval ^= g;
		}
	}
	return hval;
}

/* Expand some escape sequences found in the argument string. */
char* GuiTextTranslator::expandEscape(const char *str) {
	char *retval, *rp;
	const char *cp = str;

	retval = (char*) malloc(strlen(str) + 1);
	if (retval == nullptr)
		return nullptr;

	rp = retval;

	while (cp[0] != '\0' && cp[0] != '\\')
		*rp++ = *cp++;

	if (cp[0] == '\0')
		goto terminate;

	do {
		/* Here cp[0] == '\\'. */
		switch (*++cp) {
			case '\"':
				*rp++ = '\"';
				++cp;
				break;
			case 'a':
				*rp++ = '\a';
				++cp;
				break;
			case 'b':
				*rp++ = '\b';
				++cp;
				break;
			case 'f':
				*rp++ = '\f';
				++cp;
				break;
			case 'n':
				*rp++ = '\n';
				++cp;
				break;
			case 'r':
				*rp++ = '\r';
				++cp;
				break;
			case 't':
				*rp++ = '\t';
				++cp;
				break;
			case 'v':
				*rp++ = '\v';
				++cp;
				break;
			case '\\':
				*rp = '\\';
				++cp;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7': {
				int ch = *cp++ - '0';
				if (*cp >= '0' && *cp <= '7') {
					ch *= 8;
					ch += *cp++ - '0';

					if (*cp >= '0' && *cp <= '7') {
						ch *= 8;
						ch += *cp++ - '0';
					}
				}
				*rp = ch;
			}
				break;
			default:
				*rp = '\\';
				break;
		}

		while (cp[0] != '\0' && cp[0] != '\\')
			*rp++ = *cp++;

	} while (cp[0] != '\0');

	terminate: *rp = '\0';
	return retval;
}

GuiTextTranslator::MSG* GuiTextTranslator::findMessage(uint32_t id) const {
	for (MSG *msg = head; msg; msg = msg->next) {
		if (msg->id == id)
			return msg;
	}
	return nullptr;
}

GuiTextTranslator::MSG* GuiTextTranslator::setMessage(const char *msgid, const char *msgstr) {
	uint32_t id = hashString(msgid);
	MSG *msg = findMessage(id);

	if (!msg) {
		msg = new MSG;
		msg->id = id;
		msg->msgstr = nullptr;
		msg->next = head;
		head = msg;
	}

	if (msg) {
		if (msg->msgstr)
			free(msg->msgstr);

		if (msgstr)
			msg->msgstr = expandEscape(msgstr);
	}

	return msg;
}

void GuiTextTranslator::clear() {
	while (head) {
		MSG *nextMsg = head->next;
		if (head->msgstr)
			free(head->msgstr);
		delete head;
		head = nextMsg;
	}
}

const char* GuiTextTranslator::memFGets(char *dst, int maxlen, const char *src, const char *eof) const {
	if (!src || !dst || maxlen <= 0 || src >= eof)
		return nullptr;

	int len = 0;
	while (src + len < eof && src[len] != '\n' && len < maxlen - 1) {
		dst[len] = src[len];
		len++;
	}
	dst[len] = '\0';

	// Advance pointer past the newline if that's what stopped us
	if (src + len < eof && src[len] == '\n') {
		return src + len + 1;
	}

	return src + len;
}

bool GuiTextTranslator::loadLanguage(const uint8_t *buffer, size_t size) {
	clear();

	if (!buffer || size == 0)
		return false;

	char line[1024];
	char *lastID = nullptr;
	const char *ptr = reinterpret_cast<const char*>(buffer);
	const char *eof = ptr + size;

	while (ptr && ptr < eof) {
		const char *nextPtr = memFGets(line, sizeof(line), ptr, eof);

		if (!nextPtr && ptr == nextPtr)
			break;

		ptr = nextPtr;

		// Lines starting with # are comments
		if (line[0] == '#' || line[0] == '\0')
			continue;

		if (strncmp(line, "msgid \"", 7) == 0) {
			if (lastID) {
				free(lastID);
				lastID = nullptr;
			}

			char *msgid = &line[7];
			char *end = strrchr(msgid, '"');
			if (end && (end - msgid) >= 0) {
				*end = '\0';
				lastID = strdup(msgid);
			}
		} else if (strncmp(line, "msgstr \"", 8) == 0) {
			if (!lastID)
				continue;

			char *msgstr = &line[8];
			char *end = strrchr(msgstr, '"');
			if (end && (end - msgstr) >= 0) {
				*end = '\0';
				setMessage(lastID, msgstr);
			}

			free(lastID);
			lastID = nullptr;
		}
	}

	// Clean up if the file ended cleanly but we had an orphaned ID buffered
	if (lastID)
		free(lastID);

	return true;
}

const char* GuiTextTranslator::getText(const char *msgid) const {
	if (!msgid)
		return nullptr;

	MSG *msg = findMessage(hashString(msgid));

	if (msg && msg->msgstr) {
		return msg->msgstr;
	}

	return msgid;
}

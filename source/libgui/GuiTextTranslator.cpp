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

GuiTextTranslator::~GuiTextTranslator() {
	clear();
	magic = 0; // a lookup after this point is a lifetime bug: it will stop in checkIntegrity()
}

/* Never returns. __builtin_trap() raises a Program exception, and its crash screen shows the
 * registers as they are when it fires, because this function does nothing before trapping:
 *   r3 = the translator          r4 = the bad pointer (the list head, or some node's next)
 *   r5 = the node that held r4   (0 when r4 is the head itself)
 *   r6 = the translator's magic as read (0x47545254 when the object itself is intact)
 * With an intact magic the object survived and only the list was damaged; a wrong magic means
 * the translator itself was overwritten. Pair this with the build's ELF (crash-lookup.sh). */
#if defined(__GNUC__) && (__GNUC__ >= 8)
#define GUI_OPAQUE_CALL __attribute__((noipa, cold))	/* no inlining, cloning or argument pruning: the registers stay as passed */
#else
#define GUI_OPAQUE_CALL __attribute__((noinline, noclone, cold))
#endif

[[noreturn]] GUI_OPAQUE_CALL
void GuiTextTranslator::fatalCorruption(const GuiTextTranslator*, const void*, const void*, uint32_t) {
	__builtin_trap();
}

void GuiTextTranslator::checkIntegrity(const MSG* node, const MSG* prev) const {
	if (magic != MAGIC)
		fatalCorruption(this, node, prev, magic);

	// every MSG comes from operator new, so a node pointer that is not aligned for MSG is not one
	if (reinterpret_cast<uintptr_t>(node) & (alignof(MSG) - 1))
		fatalCorruption(this, node, prev, magic);
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
	checkIntegrity(head, nullptr);

	const MSG *prev = nullptr;
	for (MSG *msg = head; msg; prev = msg, msg = msg->next) {
		checkIntegrity(msg, prev);
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
	checkIntegrity(head, nullptr);

	while (head) {
		checkIntegrity(head, nullptr);
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

	if (msg && msg->msgstr && msg->msgstr[0] != '\0') {
		return msg->msgstr;
	}

	return msgid;
}

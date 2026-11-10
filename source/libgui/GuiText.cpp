/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiText.cpp
 ***************************************************************************/

#include "Gui.h"

#include <cstdarg>
#include <cstdio>

// The one translator. Its constructor is constexpr, so this is constant-initialized: it is complete
// before any dynamic initializer runs, and a static GuiText/GuiButton constructed in any translation
// unit can resolve text through it without depending on link order. The union keeps it from ever being
// destroyed: it lives until the process ends, so nothing can look up text in a list freed at exit.
static union TranslatorStorage
{
	GuiTextTranslator translator;
	constexpr TranslatorStorage() : translator() {}
	~TranslatorStorage() {}
} translatorStorage;
GuiTextTranslator* textTranslator = &translatorStorage.translator;

std::string Tr(const char* msgid)
{
	const char* text = msgid ? textTranslator->getText(msgid) : nullptr;
	return text ? std::string(text) : std::string();
}

// Reads the next printf conversion of s (from its '%' to its conversion letter, so "%-5.2f" is one) into
// spec and returns where it ends, or nullptr when there are no more. "%%" is not a conversion.
static const char* NextConversion(const char* s, std::string& spec)
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
		spec.clear();
		while(*s && !strchr("diouxXeEfFgGaAcspn", *s))
			spec += *s++;
		if(*s)
			spec += *s++;
		return s;
	}
	return nullptr;
}

// true when a and b take the same arguments: the same conversions in the same order
static bool SameConversions(const char* a, const char* b)
{
	std::string specA, specB;
	do
	{
		a = NextConversion(a, specA);
		b = NextConversion(b, specB);
	} while(a && b && specA == specB);

	return !a && !b; // Only equal if both ran out together; a mismatched spec leaves a and b non-null
}

std::string TrFormat(const char* msgid, ...)
{
	const char* fmt = textTranslator->getText(msgid);
	if(!fmt || !SameConversions(fmt, msgid))
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

static PixelColor presetColor = (PixelColor){255, 255, 255, 255};
static int presetSize = 0;
static int presetMaxWidth = 0;
static ALIGN_H presetAlignmentHor = ALIGN_H::LEFT;
static ALIGN_V presetAlignmentVert = ALIGN_V::TOP;
static uint16_t presetStyle = 0;

#define TEXT_SCROLL_DELAY			8
#define	TEXT_SCROLL_INITIAL_DELAY	6

static const wchar_t ellipsis[] = L"...";
static const wchar_t scrollGap[] = L"  ";

/**
 * Constructor for the GuiText class.
 */
GuiText::GuiText(const char * t, int s, PixelColor c)
{
	setup(t, s, c, GUI_TEXT_JUSTIFY_CENTER | GUI_TEXT_ALIGN_MIDDLE, 0, ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
}

/**
 * Constructor for the GuiText class, uses presets
 */
GuiText::GuiText(const char * t)
{
	setup(t, presetSize, presetColor, presetStyle, presetMaxWidth, presetAlignmentHor, presetAlignmentVert);
}

void GuiText::setup(const char * t, int s, PixelColor c, uint16_t st, int mw, ALIGN_H h, ALIGN_V v)
{
	size = s;
	color = c;
	alpha = c.a;
	style = st;
	maxWidth = mw;
	wrap = false;
	textScroll = SCROLL::NONE;
	textScrollPos = 0;
	textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
	textScrollDelay = TEXT_SCROLL_DELAY;
	measuredW = measuredH = 0;
	measuredSize = fullWidthSize = -1;
	measuredGen = fullWidthGen = 0;
	fullWidth = 0;

	alignmentHor = h;
	alignmentVert = v;

	assign(t);
}

/**
 * Destructor for the GuiText class.
 */
GuiText::~GuiText()
{
	free(text); // the one allocation
}

void GuiText::release()
{
	free(text);
	text = nullptr;
	msgid = nullptr;
	textLen = textCap = msgCap = 0;
}

/**
 * Takes a message ID: translates it, and keeps the translation and the ID itself (resetText() translates
 * it again) in one allocation. When the text already held is in an allocation big enough, that is reused.
 * `id` may point into the allocation that is held (resetText() passes the ID it keeps).
 */
void GuiText::assign(const char * id)
{
	layoutValid = false;
	textScrollPos = 0;
	textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
	this->forgetMeasurements();

	if(!id)
	{
		release();
		return;
	}

	const char * translated = textTranslator->getText(id);
	if(!translated)
		translated = id;

	// two passes over the translation: how many characters it comes to, then the characters themselves
	size_t n = GuiTextRenderer::utf8ToWide(translated, nullptr, 0);
	if(n > MAX_TEXT_CHARS)
		n = MAX_TEXT_CHARS;

	size_t m = strlen(id);
	if(m > MAX_MSGID_BYTES)
	{
		m = MAX_MSGID_BYTES;
		while(m > 0 && ((unsigned char)id[m] & 0xC0) == 0x80) // do not end partway through a character
			--m;
	}

	if(text && n <= textCap && m <= msgCap)
	{
		// fits where it is
		textLen = (uint16_t)GuiTextRenderer::utf8ToWide(translated, text, n + 1);
		if(id != msgid)
			memmove(msgid, id, m);
		msgid[m] = 0;
		return;
	}

	// The new allocation is filled before the old one is freed, because `id` and `translated` may point into it
	const size_t wideBytes = (n + 1) * sizeof(wchar_t);
	void * block = malloc(wideBytes + m + 1);
	if(!block)
	{
		release(); // nothing is drawn, and nothing can go wrong with it
		return;
	}

	wchar_t * newText = static_cast<wchar_t *>(block);
	char * newMsgid = static_cast<char *>(block) + wideBytes;
	const size_t len = GuiTextRenderer::utf8ToWide(translated, newText, n + 1);
	memcpy(newMsgid, id, m);
	newMsgid[m] = 0;

	free(text);
	text = newText;
	msgid = newMsgid;
	textLen = (uint16_t)len;
	textCap = (uint16_t)n;
	msgCap = (uint16_t)m;
}

void GuiText::setText(const char * t)
{
	assign(t);
}

int GuiText::getLongestWordWidth()
{
	if(!text)
		return 0;

	int longest = 0;
	uint32_t p = 0;

	fontSystem->setPixelSize(size);
	while(p < textLen)
	{
		while(p < textLen && text[p] == L' ')
			p++;

		uint32_t q = p;
		while(q < textLen && text[q] != L' ')
			q++;

		if(q > p)
		{
			const GuiTextRun word = { text + p, q - p };
			const int w = fontSystem->getWidth(&word, 1);
			if(w > longest)
				longest = w;
		}
		p = q;
	}
	return longest;
}

void GuiText::setPresets(int sz, PixelColor c, int w, uint16_t s, ALIGN_H h, ALIGN_V v)
{
	presetSize = sz;
	presetColor = c;
	presetStyle = s;
	presetMaxWidth = w;
	presetAlignmentHor = h;
	presetAlignmentVert = v;
}

void GuiText::setFontSize(int s)
{
	if(s == size)
		return;

	size = s;
	layoutValid = false;
}

//! Measures the text at `size` unless that has already been done for this text, size and font
void GuiText::measure()
{
	if(measuredSize == size && measuredGen == GuiTextRenderer::getMetricsGeneration())
		return;

	const GuiTextRun whole = { text, textLen };
	fontSystem->setPixelSize(size);
	measuredW = fontSystem->getWidth(&whole, 1);
	measuredH = fontSystem->getLineHeight(); // a line's height, not the height of these glyphs
	measuredSize = size;
	measuredGen = GuiTextRenderer::getMetricsGeneration();
}

void GuiText::forgetMeasurements()
{
	measuredSize = -1;
	fullWidthSize = -1;
}

//! The width of the whole text at a font size, measured on the first ask. The font must be at that size.
int GuiText::getFullWidth(int atSize)
{
	if(fullWidthSize != atSize || fullWidthGen != GuiTextRenderer::getMetricsGeneration())
	{
		const GuiTextRun whole = { text, textLen };
		fullWidth = fontSystem->getWidth(&whole, 1);
		fullWidthSize = atSize;
		fullWidthGen = GuiTextRenderer::getMetricsGeneration();
	}
	return fullWidth;
}

void GuiText::setMaxWidth(int width)
{
	if(width == maxWidth)
		return;

	maxWidth = width;
	layoutValid = false;
}

int GuiText::getTextWidth()
{
	if(!text)
		return 0;

	this->measure();
	return measuredW;
}

void GuiText::setWrap(bool w, int width)
{
	if(w == wrap && width == maxWidth)
		return;

	wrap = w;
	maxWidth = width;
	layoutValid = false;
}

void GuiText::setScroll(SCROLL s)
{
	if(textScroll == s)
		return;

	layoutValid = false;

	textScroll = s;
	textScrollPos = 0;
	textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
	textScrollDelay = TEXT_SCROLL_DELAY;
}

void GuiText::setColor(PixelColor c)
{
	color = c;
	alpha = c.a;
}

void GuiText::setStyle(uint16_t s)
{
	style = s;
}

void GuiText::setAlignment(ALIGN_H hor, ALIGN_V vert)
{
	style = 0;

	switch(hor)
	{
		case ALIGN_H::LEFT:
			style |= GUI_TEXT_JUSTIFY_LEFT;
			break;
		case ALIGN_H::RIGHT:
			style |= GUI_TEXT_JUSTIFY_RIGHT;
			break;
		default:
			style |= GUI_TEXT_JUSTIFY_CENTER;
			break;
	}
	switch(vert)
	{
		case ALIGN_V::TOP:
			style |= GUI_TEXT_ALIGN_TOP;
			break;
		case ALIGN_V::BOTTOM:
			style |= GUI_TEXT_ALIGN_BOTTOM;
			break;
		default:
			style |= GUI_TEXT_ALIGN_MIDDLE;
			break;
	}

	alignmentHor = hor;
	alignmentVert = vert;
}

void GuiText::resetText()
{
	if(!msgid)
		return;

	assign(msgid);
}

/**
 * Returns true once every textScrollDelay frame timer ticks.
 */
bool GuiText::scrollStepDue()
{
	const uint32_t now = platform->getVideo()->getFrameTimer();

	// Timer was reset: resync instead of stalling
	if(now < textScrollLastTick)
		textScrollLastTick = now;

	if(now - textScrollLastTick < (uint32_t)textScrollDelay)
		return false;

	textScrollLastTick = now;
	return true;
}

/**
 * Breaks the text into lines no wider than maxWidth, at spaces. A line is measured without the spaces
 * that end it, and a word too wide for a line of its own gets one anyway. Fills lineStart/lineLen. If more
 * lines are needed than MAX_WRAP_LINES, the last line is cut short to leave room for "...". The font must
 * already be at the size the text is drawn at.
 */
void GuiText::breakLines()
{
	const uint32_t len = textLen;
	uint32_t pos = 0;

	while(pos < len && lineCount < MAX_WRAP_LINES)
	{
		const uint32_t start = pos;
		uint32_t end = start; // where the last word that fits ends
		uint32_t p = start;
		bool anyWord = false;

		for(;;)
		{
			uint32_t q = p;
			while(q < len && text[q] != L' ')
				q++;

			const bool isWord = q > p;
			if(isWord && anyWord)
			{
				const GuiTextRun line = { text + start, q - start };
				if(fontSystem->getWidth(&line, 1) > maxWidth)
					break;
			}
			end = q;
			anyWord = anyWord || isWord;

			p = q;
			while(p < len && text[p] == L' ')
				p++;
			if(p >= len)
				break;
		}

		uint32_t next = end;
		while(next < len && text[next] == L' ')
			next++;
		if(next <= start) // cannot happen; but a line that makes no progress must not loop forever
			next = len;

		uint32_t lineEnd = end;
		if(lineCount == MAX_WRAP_LINES - 1 && next < len)
		{
			// the last line there is room for, and there is more text: keep what fits with "..." after it
			const GuiTextRun dots = { ellipsis, 3 };
			const int room = maxWidth - fontSystem->getWidth(&dots, 1);
			const GuiTextRun last = { text + start, end - start };
			uint32_t keep = room > 0 ? (uint32_t)fontSystem->fitCount(&last, 1, room) : 0;
			while(keep > 0 && text[start + keep - 1] == L' ')
				keep--;
			lineEnd = start + keep;
			lastLineCut = true;
		}

		lineStart[lineCount] = (uint16_t)start;
		lineLen[lineCount] = (uint16_t)(lineEnd - start);
		lineCount++;
		pos = next;
	}
}

/**
 * The runs the text is shown as when it is not wrapped: from textScrollPos on, then, once the start has
 * scrolled off, two spaces and the text again from its start. Returns how many runs there are (1 to 3).
 */
int GuiText::makeWindow(GuiTextRun * runs) const
{
	int n = 0;
	runs[n++] = { text + textScrollPos, textLen - (uint32_t)textScrollPos };
	if(textScrollPos > 0)
	{
		runs[n++] = { scrollGap, 2 };
		runs[n++] = { text, textLen };
	}
	return n;
}

//! Works out how much of the window (see makeWindow()) fits in maxWidth
void GuiText::updateWindow()
{
	GuiTextRun runs[3];
	const int n = makeWindow(runs);
	shownLen = (uint16_t)fontSystem->fitCount(runs, n, maxWidth);
}

//! Cuts a list of runs down to its first `total` characters
static int CutRuns(GuiTextRun * runs, int n, uint32_t total)
{
	for(int i = 0; i < n; i++)
	{
		if(runs[i].len >= total)
		{
			runs[i].len = total;
			return i + 1;
		}
		total -= runs[i].len;
	}
	return n;
}

//! Works out everything draw() needs to show the text within maxWidth. The font must be at the drawn size.
void GuiText::buildLayout()
{
	layoutValid = true;
	layoutGen = GuiTextRenderer::getMetricsGeneration();
	lineCount = 0;
	lastLineCut = false;

	if(wrap)
	{
		breakLines();
		return;
	}

	if((uint32_t)textScrollPos >= textLen)
		textScrollPos = 0;
	updateWindow();
}

/**
 * Draw the text on screen
 */
void GuiText::draw()
{
	if(!text)
		return;

	if(!this->isVisible())
		return;

	PixelColor c = color;
	c.a = this->getAlpha();

	int newSize = size*this->getScale();

	fontSystem->setPixelSize(newSize);

	if(maxWidth == 0)
	{
		const GuiTextRun whole = { text, textLen };
		fontSystem->drawText(this->getLeft(), this->getTop() + nudgeY, &whole, 1, c, style);
		this->updateEffects();
		return;
	}

	if(!layoutValid || layoutGen != GuiTextRenderer::getMetricsGeneration())
		buildLayout();

	if(wrap)
	{
		int lineheight = newSize + lineSpacing;
		int voffset = 0;

		if(alignmentVert == ALIGN_V::MIDDLE)
			voffset = (lineheight >> 1) * (1-(int)lineCount);

		int left = this->getLeft();
		int top  = this->getTop() + voffset + nudgeY;

		for(int i=0; i < lineCount; ++i)
		{
			GuiTextRun runs[2] = { { text + lineStart[i], lineLen[i] }, { ellipsis, 3 } };
			const int n = (lastLineCut && i == lineCount - 1) ? 2 : 1;
			fontSystem->drawText(left, top+i*lineheight, runs, n, c, style);
		}
	}
	else
	{
		if(textScroll == SCROLL::HORIZONTAL && textLen > 0)
		{
			if(getFullWidth(newSize) > maxWidth && scrollStepDue())
			{
				if(textScrollInitialDelay)
				{
					--textScrollInitialDelay;
				}
				else
				{
					++textScrollPos;
					if((uint32_t)textScrollPos >= textLen)
					{
						textScrollPos = 0;
						textScrollInitialDelay = TEXT_SCROLL_INITIAL_DELAY;
					}

					updateWindow();
				}
			}
		}

		GuiTextRun runs[3];
		const int n = CutRuns(runs, makeWindow(runs), shownLen);
		fontSystem->drawText(this->getLeft(), this->getTop() + nudgeY, runs, n, c, style);
	}
	this->updateEffects();
}

void GuiText::getNaturalSize(int& w, int& h)
{
	if(!text)
	{
		w = 0;
		h = 0;
		return;
	}

	this->measure();
	w = measuredW;
	h = measuredH;
	if(maxWidth > 0 && w > maxWidth)
		w = maxWidth;
}

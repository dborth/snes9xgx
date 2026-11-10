/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiText.h
 ***************************************************************************/
#pragma once

//!Display, manage, and manipulate text in the GUI
class GuiText : public GuiElement
{
	public:
		//!Constructor
		//!\param t Text
		//!\param s Font size
		//!\param c Font color
		GuiText(const char * t, int s, PixelColor c);
		//!\overload
		//!Assumes SetPresets() has been called to setup preferred text attributes
		//!\param t Text
		GuiText(const char * t);
		//!Destructor
		~GuiText();
		//!Sets the text of the GuiText element. The text is looked up for translation (see GuiTextTranslator),
		//!and the result is kept in one allocation that later calls reuse whenever the new text fits in it.
		//!\param t Text (UTF-8)
		void setText(const char * t);
		//!Sets up preset values to be used by GuiText(t)
		//!Useful when printing multiple text elements, all with the same attributes set
		//!\param sz Font size
		//!\param c Font color
		//!\param w Maximum width of texture image (for text wrapping)
		//!\param s Font size
		//!\param h Text alignment (horizontal)
		//!\param v Text alignment (vertical)
		static void setPresets(int sz, PixelColor c, int w, uint16_t s, ALIGN_H h, ALIGN_V v);
		//!Sets the font size
		//!\param s Font size
		void setFontSize(int s);
		//!Sets the maximum width of the drawn texture image
		//!\param w Maximum width
		void setMaxWidth(int w);
		//!Gets the width of the text when rendered
		int getTextWidth();
		//!Gets the number of characters in the (translated) text
		int getLength() { return textLen; }
		//!Gets the width of the text's widest word (words are split at spaces): the least width
		//!that wrapping can fit the text in
		int getLongestWordWidth();
		//!The font size
		int getFontSize() const { return size; }
		//!Whether the text wraps onto more lines (see setWrap())
		bool isWrapped() const { return wrap; }
		//!Gets the text's size for layout: its rendered width and height on one line
		//!(at most the maximum width, if one is set). A layout container places text by its
		//!top-left corner: it calls setAlignment(LEFT, TOP), which anchors the text there.
		void getNaturalSize(int& w, int& h) override;
		//!Enables/disables text scrolling
		//!\param s Scrolling on/off
		void setScroll(SCROLL s);
		//!Enables/disables text wrapping. Lines break at spaces; a word wider than the width gets a line to itself.
		//!Text that needs more than MAX_WRAP_LINES lines is cut off with "..." at the end of the last one.
		//!\param w Wrapping on/off
		//!\param width Maximum width (0 to disable)
		void setWrap(bool w, int width = 0);
		//!Sets the font color
		//!\param c Font color
		void setColor(PixelColor c);
		//!Sets the GuiTextRenderer style attributes
		//!\param s Style attributes
		void setStyle(uint16_t s);
		//!Sets the text alignment
	//!\param hor Horizontal alignment (LEFT, RIGHT, CENTRE)
	//!\param vert Vertical alignment (TOP, BOTTOM, MIDDLE)
		void setAlignment(ALIGN_H hor, ALIGN_V vert);
		//!Moves the drawn text down (or up, if negative) by this many design pixels, after it is
		//!placed. The font's line box has more room below the baseline than above the capitals,
		//!so capitals centered in a button read as sitting high; this evens the padding.
		void setNudgeY(int n) { nudgeY = n; }
		//!Sets the pixels added to the font size to get the distance between wrapped lines (default 6; less
		//!for a tighter block, negative for tighter than the font size)
		void setLineSpacing(int px) { lineSpacing = px; }
		//!Updates the text to the selected language
		void resetText();
		//!Constantly called to draw the text
	void draw() override;
	private:
		//! Limits that keep the text, and the work done on it, bounded
		enum {
			MAX_WRAP_LINES = 8,		//!< Lines a wrapped text is kept in; what does not fit is replaced by "..."
			MAX_TEXT_CHARS = 1023,	//!< Characters kept of the translated text
			MAX_MSGID_BYTES = 1023	//!< Bytes kept of the original text (what resetText() translates again)
		};

		PixelColor color; //!< Font color

		// The text lives in one allocation: textCap + 1 wide characters, then msgCap + 1 bytes. `text` is its
		// start (and what is freed); null when there is no text, or when the allocation failed.
		wchar_t* text = nullptr; //!< Translated Unicode text, NUL-terminated
		char* msgid = nullptr; //!< Original text as given (UTF-8, English), kept for resetText()
		uint16_t textLen = 0; //!< Characters in text
		uint16_t textCap = 0; //!< Most characters the allocation holds in text
		uint16_t msgCap = 0; //!< Most bytes the allocation holds in msgid

		int size; //!< Font size
		int maxWidth; //!< Maximum width of the generated text object (for text wrapping)
		SCROLL textScroll; //!< Scrolling toggle
		int textScrollPos; //!< Index of the character at the left edge when scrolling
		int textScrollInitialDelay; //!< Delay to wait before starting to scroll
		int textScrollDelay; //!< Scrolling speed (frame timer ticks per scroll step)
		uint32_t textScrollLastTick = 0; //!< Frame timer value at the last scroll step
		uint16_t style; //!< GuiTextRenderer style attributes
		bool wrap; //!< Wrapping toggle
		int nudgeY = 0; //!< Extra vertical offset when drawn, in design pixels
		int lineSpacing = 6; //!< Pixels added to the font size for the distance between wrapped lines

		// How the text is cut up when it has a maximum width. These are ranges of `text`, worked out when the
		// text is first drawn after anything that changes them, and kept until then. Nothing is copied.
		bool layoutValid = false; //!< false after anything that changes where lines break
		uint32_t layoutGen = 0; //!< GuiTextRenderer::getMetricsGeneration() when laid out
		uint8_t lineCount = 0; //!< Wrapped lines in use
		bool lastLineCut = false; //!< The last wrapped line is followed by "..." because the text did not all fit
		uint16_t lineStart[MAX_WRAP_LINES]; //!< Index in text of each wrapped line's first character
		uint16_t lineLen[MAX_WRAP_LINES]; //!< Characters in each wrapped line (spaces at its end are not counted)
		uint16_t shownLen = 0; //!< Characters shown when not wrapped: those that fit (see makeWindow())

		// What measuring the text cost, kept so a layout pass that asks for the size every frame costs
		// nothing. Measured at `size`, and valid while the text, the size and the font are unchanged.
		int measuredW;				//!< Rendered width at measuredSize
		int measuredH;				//!< Rendered height at measuredSize
		int measuredSize;			//!< Font size measuredW/H are for; -1 when stale
		uint32_t measuredGen;		//!< GuiTextRenderer::getMetricsGeneration() when measured
		// The same for the full width of the text at the size it is drawn at (which follows the
		// element's scale), used by the scroll check every frame
		int fullWidth;
		int fullWidthSize;			//!< -1 when stale
		uint32_t fullWidthGen;

		void setup(const char * t, int s, PixelColor c, uint16_t st, int mw, ALIGN_H h, ALIGN_V v);
		void assign(const char * id);
		void release();
		void buildLayout();
		void breakLines();
		int makeWindow(GuiTextRun * runs) const;
		void updateWindow();
		bool scrollStepDue();
		void measure();
		void forgetMeasurements();
		int getFullWidth(int atSize);
};

extern GuiTextTranslator* textTranslator;

//! Translates a message ID to a std::string, for text that is put together at runtime.
//! GuiText only translates the exact string it is given, so a message built with printf or
//! operator+ has to have its parts translated first. Use TrFormat() for a message with values in it.
//!\param msgid The English text/ID to translate
//!\return The translated string, or msgid itself when there is no translation
//!\ingroup grp_core
std::string Tr(const char* msgid);

//! printf for translated messages. msgid is both the lookup key and the English format string,
//! so its conversions are checked against the arguments at compile time. If the translation does
//! not have the same conversions in the same order (a typo in a .lang file) the English text is
//! used instead, so a bad translation can never be run with the wrong arguments.
//!\param msgid The English format string, e.g. "Player %d"
//!\return The formatted, translated string
//!\ingroup grp_core
#if defined(__GNUC__)
std::string TrFormat(const char* msgid, ...) __attribute__((format(printf, 1, 2)));
#else
std::string TrFormat(const char* msgid, ...);
#endif

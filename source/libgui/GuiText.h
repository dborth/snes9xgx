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
		//!Sets the text of the GuiText element
		//!\param t Text
		void setText(const char * t);
		//!Sets the text of the GuiText element
		//!\param t UTF-8 Text
		void setWText(wchar_t * t);
		//!Gets the translated text length of the GuiText element
		int getLength();
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
		//!Enables/disables text scrolling
		//!\param s Scrolling on/off
		void setScroll(SCROLL s);
		//!Enables/disables text wrapping
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
		//!Updates the text to the selected language
		void resetText();
		//!Constantly called to draw the text
	void draw() override;
	private:
		PixelColor color; //!< Font color
		wchar_t* text; //!< Translated Unicode text value
		wchar_t *textDyn[20]; //!< Text value, if max width, scrolling, or wrapping enabled
		int textDynNum; //!< Number of text lines
		char * origText; //!< Original text data (English)
		int size; //!< Font size
		int maxWidth; //!< Maximum width of the generated text object (for text wrapping)
		SCROLL textScroll; //!< Scrolling toggle
		int textScrollPos; //!< Current starting index of text string for scrolling
		int textScrollInitialDelay; //!< Delay to wait before starting to scroll
		int textScrollDelay; //!< Scrolling speed
		uint16_t style; //!< GuiTextRenderer style attributes
		bool wrap; //!< Wrapping toggle

		wchar_t* getText(const char *text) const;
};

extern GuiTextTranslator* textTranslator;

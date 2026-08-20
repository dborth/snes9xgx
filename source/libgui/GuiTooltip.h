/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiTooltip.h
 ***************************************************************************/
#pragma once

//!Display, manage, and manipulate tooltips in the GUI
class GuiTooltip : public GuiElement
{
	public:
		//!Constructor
		//!\param t Text
		GuiTooltip(const char *t);
		//!Destructor
		~GuiTooltip();
		//!Gets the element's current scale
		float getScale();
		//!Sets the text of the GuiTooltip element
		//!\param t Text
		void setText(const char * t);
		//!Constantly called to draw the GuiTooltip
		void drawTooltip();
	
		time_t time1, time2; //!< Tooltip times

	protected:
		GuiImage leftImage; //!< Tooltip left image
		GuiImage tileImage; //!< Tooltip tile image
		GuiImage rightImage; //!< Tooltip right image
		GuiText *text; //!< Tooltip text
};

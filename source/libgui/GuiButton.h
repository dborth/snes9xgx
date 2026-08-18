#ifndef GUIBUTTON_H
#define GUIBUTTON_H

#include "../libgui/Gui.h"

//!Display, manage, and manipulate buttons in the GUI. Buttons can have images, icons, text, and sound set (all of which are optional)
class GuiButton : public GuiElement
{
	public:
		//!Constructor
		//!\param w Width
		//!\param h Height
		GuiButton(int w = 0, int h = 0);
		//!Destructor
		~GuiButton();
		//!Sets the button's image
		//!\param i Pointer to GuiImage object
		void setImage(GuiImage* i);
		//!Sets the button's image on over
		//!\param i Pointer to GuiImage object
		void setImageOver(GuiImage* i);
		//!Sets the button's image on hold
		//!\param i Pointer to GuiImage object
		void setImageHold(GuiImage* i);
		//!Sets the button's image on click
		//!\param i Pointer to GuiImage object
		void setImageClick(GuiImage* i);
		//!Sets the button's icon
		//!\param i Pointer to GuiImage object
		void setIcon(GuiImage* i);
		//!Sets the button's icon on over
		//!\param i Pointer to GuiImage object
		void setIconOver(GuiImage* i);
		//!Sets the button's icon on hold
		//!\param i Pointer to GuiImage object
		void setIconHold(GuiImage* i);
		//!Sets the button's icon on click
		//!\param i Pointer to GuiImage object
		void setIconClick(GuiImage* i);
		//!Sets the button's label
		//!\param t Pointer to GuiText object
		//!\param n Index of label to set (optional, default is 0)
		void setLabel(GuiText* t, int n = 0);
		//!Sets the button's label on over (eg: different colored text)
		//!\param t Pointer to GuiText object
		//!\param n Index of label to set (optional, default is 0)
		void setLabelOver(GuiText* t, int n = 0);
		//!Sets the button's label on hold
		//!\param t Pointer to GuiText object
		//!\param n Index of label to set (optional, default is 0)
		void setLabelHold(GuiText* t, int n = 0);
		//!Sets the button's label on click
		//!\param t Pointer to GuiText object
		//!\param n Index of label to set (optional, default is 0)
		void setLabelClick(GuiText* t, int n = 0);
		//!Sets the sound to play on over
		//!\param s Pointer to GuiSound object
		void setSoundOver(GuiSound * s);
		//!Sets the sound to play on hold
		//!\param s Pointer to GuiSound object
		void setSoundHold(GuiSound * s);
		//!Sets the sound to play on click
		//!\param s Pointer to GuiSound object
		void setSoundClick(GuiSound * s);
		//!Sets the tooltip for the button
		//!\param t Tooltip
		void setTooltip(GuiTooltip * t);
		//!Constantly called to draw the GuiButton
		void draw();
		//!Constantly called to draw the GuiButton's tooltip
		void drawTooltip();
		//!Resets the text for all contained elements
		void resetText();
		//!Constantly called to allow the GuiButton to respond to updated input data
		//!\param t Pointer to a GuiTrigger, containing the current input data from PAD/WPAD
		void update(GuiTrigger * t);
	protected:
		GuiImage * image; //!< Button image (default)
		GuiImage * imageOver; //!< Button image for STATE_SELECTED
		GuiImage * imageHold; //!< Button image for STATE_HELD
		GuiImage * imageClick; //!< Button image for STATE_CLICKED
		GuiImage * icon; //!< Button icon (drawn after button image)
		GuiImage * iconOver; //!< Button icon for STATE_SELECTED
		GuiImage * iconHold; //!< Button icon for STATE_HELD
		GuiImage * iconClick; //!< Button icon for STATE_CLICKED
		GuiText * label[3]; //!< Label(s) to display (default)
		GuiText * labelOver[3]; //!< Label(s) to display for STATE_SELECTED
		GuiText * labelHold[3]; //!< Label(s) to display for STATE_HELD
		GuiText * labelClick[3]; //!< Label(s) to display for STATE_CLICKED
		GuiSound * soundOver; //!< Sound to play for STATE_SELECTED
		GuiSound * soundHold; //!< Sound to play for STATE_HELD
		GuiSound * soundClick; //!< Sound to play for STATE_CLICKED
		GuiTooltip * tooltip; //!< Tooltip to display on over
};

#endif // GUIBUTTON_H

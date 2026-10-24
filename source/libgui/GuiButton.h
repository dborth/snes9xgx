/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiButton.h
 ***************************************************************************/
#pragma once

#define MAX_BTN_LABELS	3

//!Display, manage, and manipulate buttons in the GUI. Buttons can have images, icons, text, and sound set (all of which are optional)
//!\ingroup grp_core
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
		//!\param i Pointer to a GuiImage, GuiShape or other element
		void setImage(GuiElement* i);
		//!Sets the button's image on over
		//!\param i Pointer to a GuiImage, GuiShape or other element
		void setImageOver(GuiElement* i);
		//!Sets the button's image on hold
		//!\param i Pointer to a GuiImage, GuiShape or other element
		void setImageHold(GuiElement* i);
		//!Sets the button's image on click
		//!\param i Pointer to a GuiImage, GuiShape or other element
		void setImageClick(GuiElement* i);
		//!Sets the button's icon
		//!\param i Pointer to a GuiImage, GuiShape or other element
		void setIcon(GuiElement* i);
		//!Sets the button's icon on over
		//!\param i Pointer to a GuiImage, GuiShape or other element
		void setIconOver(GuiElement* i);
		//!Sets the button's icon on hold
		//!\param i Pointer to a GuiImage, GuiShape or other element
		void setIconHold(GuiElement* i);
		//!Sets the button's icon on click
		//!\param i Pointer to a GuiImage, GuiShape or other element
		void setIconClick(GuiElement* i);
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
		//!Constantly called to draw the GuiButton
		void draw() override;
		//!Resets the text for all contained elements
		void resetText();
		//!Sets the state and mirrors it into the button's images and icons, so a
		//!state-aware element such as GuiShape can restyle itself
		void setState(STATE s, int c = -1) override;
		//!Resets the state and mirrors it into the button's images and icons
		void resetState() override;
		//!Constantly called to allow the GuiButton to respond to updated input data
		//!\param c Pointer to a InputController, containing the current input data
		void update(InputController * c);
	protected:
		GuiElement * image; //!< Button image (default): a GuiImage, GuiShape or any element
		GuiElement * imageOver; //!< Button image for STATE::SELECTED
		GuiElement * imageHold; //!< Button image for STATE::HELD
		GuiElement * imageClick; //!< Button image for STATE::CLICKED
		GuiElement * icon; //!< Button icon (drawn after button image)
		GuiElement * iconOver; //!< Button icon for STATE::SELECTED
		GuiElement * iconHold; //!< Button icon for STATE::HELD
		GuiElement * iconClick; //!< Button icon for STATE::CLICKED
		GuiText * label[MAX_BTN_LABELS]; //!< Label(s) to display (default)
		GuiText * labelOver[MAX_BTN_LABELS]; //!< Label(s) to display for STATE::SELECTED
		GuiText * labelHold[MAX_BTN_LABELS]; //!< Label(s) to display for STATE::HELD
		GuiText * labelClick[MAX_BTN_LABELS]; //!< Label(s) to display for STATE::CLICKED
		GuiSound * soundOver; //!< Sound to play for STATE::SELECTED
		GuiSound * soundHold; //!< Sound to play for STATE::HELD
		GuiSound * soundClick; //!< Sound to play for STATE::CLICKED
};

/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * GuiKeyboard.h
 ***************************************************************************/
#pragma once

constexpr int KB_ROWS = 4;
constexpr int KB_COLUMNS = 11;

#define MAX_KEYBOARD_DISPLAY	32

typedef struct _keytype {
	char ch, chShift;
} Key;

//!On-screen keyboard
class GuiKeyboard : public GuiWindow
{
	public:
		GuiKeyboard(char * t, uint32_t m);
		~GuiKeyboard();
		void update(InputController * c);
		char kbtextstr[256];
	protected:
		uint32_t kbtextmaxlen;
		int shift;
		int caps;
		GuiText * kbText;
		GuiShape * keyTextboxShape;
		GuiText * keyCapsText;
		GuiShape * keyCapsImg;
		GuiButton * keyCaps;
		GuiText * keyShiftText;
		GuiShape * keyShiftImg;
		GuiButton * keyShift;
		GuiText * keyBackText;
		GuiShape * keyBackImg;
		GuiButton * keyBack;
		GuiShape * keySpaceImg;
		GuiButton * keySpace;
		GuiButton * keyBtn[KB_ROWS][KB_COLUMNS];
		GuiShape * keyImg[KB_ROWS][KB_COLUMNS];
		GuiText * keyTxt[KB_ROWS][KB_COLUMNS];
		GuiSound * keySoundOver;
		GuiSound * keySoundClick;
		GuiTrigger * trigA;
		Key keys[KB_ROWS][KB_COLUMNS]; // two chars = less space than one pointer
};

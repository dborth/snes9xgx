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
		void update(GuiInputController * c);
		char kbtextstr[256];
	protected:
		uint32_t kbtextmaxlen;
		int shift;
		int caps;
		GuiText * kbText;
		GuiImage * keyTextboxImg;
		GuiText * keyCapsText;
		GuiImage * keyCapsImg;
		GuiImage * keyCapsOverImg;
		GuiButton * keyCaps;
		GuiText * keyShiftText;
		GuiImage * keyShiftImg;
		GuiImage * keyShiftOverImg;
		GuiButton * keyShift;
		GuiText * keyBackText;
		GuiImage * keyBackImg;
		GuiImage * keyBackOverImg;
		GuiButton * keyBack;
		GuiImage * keySpaceImg;
		GuiImage * keySpaceOverImg;
		GuiButton * keySpace;
		GuiButton * keyBtn[KB_ROWS][KB_COLUMNS];
		GuiImage * keyImg[KB_ROWS][KB_COLUMNS];
		GuiImage * keyImgOver[KB_ROWS][KB_COLUMNS];
		GuiText * keyTxt[KB_ROWS][KB_COLUMNS];
		GuiImageData * keyTextbox;
		GuiImageData * key;
		GuiImageData * keyOver;
		GuiImageData * keyMedium;
		GuiImageData * keyMediumOver;
		GuiImageData * keyLarge;
		GuiImageData * keyLargeOver;
		GuiSound * keySoundOver;
		GuiSound * keySoundClick;
		GuiTrigger * trigA;
		Key keys[KB_ROWS][KB_COLUMNS]; // two chars = less space than one pointer
};

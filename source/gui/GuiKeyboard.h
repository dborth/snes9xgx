#ifndef GUIKEYBOARD_H
#define GUIKEYBOARD_H

#include "Gui.h"

#define MAX_KEYBOARD_DISPLAY	32

typedef struct _keytype {
	char ch, chShift;
} Key;

//!On-screen keyboard
class GuiKeyboard : public GuiWindow
{
	public:
		GuiKeyboard(char * t, u32 m);
		~GuiKeyboard();
		void update(GuiTrigger * t);
		char kbtextstr[256];
	protected:
		u32 kbtextmaxlen;
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
		GuiButton * keyBtn[4][11];
		GuiImage * keyImg[4][11];
		GuiImage * keyImgOver[4][11];
		GuiText * keyTxt[4][11];
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
		GuiTrigger * trig2;
		Key keys[4][11]; // two chars = less space than one pointer
};

#endif // GUIKEYBOARD_H

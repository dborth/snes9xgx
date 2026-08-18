#ifndef GUIOPTIONBROWSER_H
#define GUIOPTIONBROWSER_H

#include "Gui.h"

#define MAX_OPTIONS 			150
#define OPTION_PAGESIZE 		8

typedef struct _optionlist {
	int length;
	char name[MAX_OPTIONS][100];
	char value[MAX_OPTIONS][50];
} OptionList;

//!Display a list of menu options
class GuiOptionBrowser : public GuiElement
{
	public:
		GuiOptionBrowser(int w, int h, OptionList * l);
		~GuiOptionBrowser();
		void setCol1Position(int x);
		void setCol2Position(int x);
		int findMenuItem(int c, int d);
		int getClickedOption();
		void resetState();
		void setFocus(int f);
		void draw();
		void triggerUpdate();
		void resetText();
		void update(GuiTrigger * t);
		GuiText * optionVal[OPTION_PAGESIZE];
	protected:
		int optionIndex[OPTION_PAGESIZE];
		GuiButton * optionBtn[OPTION_PAGESIZE];
		GuiText * optionTxt[OPTION_PAGESIZE];
		GuiImage * optionBg[OPTION_PAGESIZE];

		int selectedItem;
		int listOffset;
		OptionList * options;

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;

		GuiImage * bgOptionsImg;
		GuiImage * scrollbarImg;
		GuiImage * arrowDownImg;
		GuiImage * arrowDownOverImg;
		GuiImage * arrowUpImg;
		GuiImage * arrowUpOverImg;

		GuiImageData * bgOptions;
		GuiImageData * bgOptionsEntry;
		GuiImageData * scrollbar;
		GuiImageData * arrowDown;
		GuiImageData * arrowDownOver;
		GuiImageData * arrowUp;
		GuiImageData * arrowUpOver;

		GuiSound * btnSoundOver;
		GuiSound * btnSoundClick;
		GuiTrigger * trigA;
		GuiTrigger * trig2;

		bool listChanged;
};

#endif // GUIOPTIONBROWSER_H

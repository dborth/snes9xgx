/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * GuiOptionBrowser.h
 ***************************************************************************/
#pragma once

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
		void draw() override;
		void triggerUpdate();
		void resetText();
		void update(InputController * c);
		GuiText * optionVal[OPTION_PAGESIZE];
	protected:
		int optionIndex[OPTION_PAGESIZE];
		GuiButton * optionBtn[OPTION_PAGESIZE];
		GuiText * optionTxt[OPTION_PAGESIZE];
		GuiShape * optionBg[OPTION_PAGESIZE];

		int selectedItem;
		int listOffset;
		OptionList * options;

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;

		GuiShape * cardShape;
		GuiShape * trackShape;
		GuiShape * thumbShape;		// shows where the visible rows sit in the whole list
		GuiButton * thumbBtn;		// the thumb's hit area: drag it to scroll
		GuiShape * arrowUpShape;
		GuiShape * arrowDownShape;
		GuiArrow * arrowUpGlyph;
		GuiArrow * arrowDownGlyph;

		int rowWidth;
		int trackHeight;
		int thumbHeight;			// current thumb size, in pixels
		int thumbTravel;			// how far the thumb can move, in pixels
		int dragPos;				// thumb offset while it is being dragged
		bool dragging;				// the thumb is held and following the pointer
		int col1X;
		int col2X;
		int visibleCount;			// options that are listed (those with a name)
		int visiblePos;				// listed options before the first row shown

		void updateColumns();
		void updateThumb();

		GuiSound * btnSoundOver;
		GuiSound * btnSoundClick;
		GuiTrigger * trigA;
		GuiTrigger * trigHeldA;

		bool listChanged;
};

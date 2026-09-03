#ifndef GUISAVEBROWSER_H
#define GUISAVEBROWSER_H

#include <memory>
#include "Gui.h"

#define MAX_SAVES 				100
#define SAVELISTSIZE 			6

typedef struct _savelist {
	int length;
	char filename[MAX_SAVES+1][256];
	std::unique_ptr<GuiImageData> previewImg[MAX_SAVES+1];
	char date[MAX_SAVES+1][20];
	char time[MAX_SAVES+1][10];
	int type[MAX_SAVES+1];
	int files[2][MAX_SAVES+1];
} SaveList;

//!Display a list of game save files, with screenshots and file information
class GuiSaveBrowser : public GuiElement
{
	public:
		GuiSaveBrowser(int w, int h, SaveList * l, int a);
		~GuiSaveBrowser();
		int getClickedSave();
		void resetState();
		void setFocus(int f);
		void draw();
		void update(InputController * c);
	protected:
		int selectedItem;
		int action;
		int listOffset;
		SaveList * saves;
		GuiButton * saveBtn[SAVELISTSIZE];
		GuiText * saveDate[SAVELISTSIZE];
		GuiText * saveTime[SAVELISTSIZE];
		GuiText * saveType[SAVELISTSIZE];

		GuiImage * saveBgImg[SAVELISTSIZE];
		GuiImage * saveBgOverImg[SAVELISTSIZE];
		GuiImage * savePreviewImg[SAVELISTSIZE];

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;

		GuiImage * scrollbarImg;
		GuiImage * arrowDownImg;
		GuiImage * arrowDownOverImg;
		GuiImage * arrowUpImg;
		GuiImage * arrowUpOverImg;

		GuiImageData * gameSave;
		GuiImageData * gameSaveOver;
		GuiImageData * gameSaveBlank;
		GuiImageData * scrollbar;
		GuiImageData * arrowDown;
		GuiImageData * arrowDownOver;
		GuiImageData * arrowUp;
		GuiImageData * arrowUpOver;

		GuiSound * btnSoundOver;
		GuiSound * btnSoundClick;
		GuiTrigger * trigA;

		bool saveBtnLastOver[SAVELISTSIZE];
};

#endif // GUISAVEBROWSER_H

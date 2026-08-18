#ifndef GUIFILEBROWSER_H
#define GUIFILEBROWSER_H

#include "gui.h"

#define FILE_PAGESIZE 			10

//!Display a list of files
class GuiFileBrowser : public GuiElement
{
	public:
		GuiFileBrowser(int w, int h);
		~GuiFileBrowser();
		void resetState();
		void setFocus(int f);
		void draw();
		void drawTooltip();
		void triggerUpdate();
		void update(GuiTrigger * t);
		GuiButton * fileList[FILE_PAGESIZE];
	protected:
		GuiText * fileListText[FILE_PAGESIZE];
		GuiImage * fileListBg[FILE_PAGESIZE];
		GuiImage * fileListIcon[FILE_PAGESIZE];

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;
		GuiButton * scrollbarBoxBtn;

		GuiImage * bgFileSelectionImg;
		GuiImage * scrollbarImg;
		GuiImage * arrowDownImg;
		GuiImage * arrowDownOverImg;
		GuiImage * arrowUpImg;
		GuiImage * arrowUpOverImg;
		GuiImage * scrollbarBoxImg;
		GuiImage * scrollbarBoxOverImg;

		GuiImageData * bgFileSelection;
		GuiImageData * bgFileSelectionEntry;
		GuiImageData * iconFolder;
		GuiImageData * iconSD;
		GuiImageData * iconUSB;
		GuiImageData * iconDVD;
		GuiImageData * iconSMB;
		GuiImageData * scrollbar;
		GuiImageData * arrowDown;
		GuiImageData * arrowDownOver;
		GuiImageData * arrowUp;
		GuiImageData * arrowUpOver;
		GuiImageData * scrollbarBox;
		GuiImageData * scrollbarBoxOver;

		GuiSound * btnSoundOver;
		GuiSound * btnSoundClick;
		GuiTrigger * trigA;
		GuiTrigger * trig2;
		GuiTrigger * trigHeldA;

		int selectedItem;
		int numEntries;
		bool listChanged;
};

#endif // GUIFILEBROWSER_H

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

//!The screenshot of a save slot, or a gray rounded placeholder while there is none.
//!A screenshot of another size is centered in the same box.
class GuiSavePreview : public GuiElement
{
	public:
		GuiSavePreview(int w, int h);
		//!\param img The screenshot, or nullptr for the placeholder
		void setImage(GuiImageData * img);
		void draw() override;
	private:
		GuiShape placeholder;
		GuiText noImage;
		GuiImage image;
		bool hasImage;
};

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

		GuiShape * saveBgImg[SAVELISTSIZE];
		GuiSavePreview * savePreviewImg[SAVELISTSIZE];

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;

		GuiShape * trackShape;
		GuiShape * thumbShape;		// shows which rows are on screen; not interactive
		GuiShape * arrowUpShape;
		GuiShape * arrowDownShape;
		GuiArrow * arrowUpGlyph;
		GuiArrow * arrowDownGlyph;

		int trackHeight;
		void updateThumb();

		GuiSound * btnSoundOver;
		GuiSound * btnSoundClick;
		GuiTrigger * trigA;

		bool saveBtnLastOver[SAVELISTSIZE];
};

#endif // GUISAVEBROWSER_H

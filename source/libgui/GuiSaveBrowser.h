#ifndef GUISAVEBROWSER_H
#define GUISAVEBROWSER_H

#include <memory>
#include <string>
#include <vector>
#include "Gui.h"

#define MAX_SAVES 				100
#define SAVELISTSIZE 			4

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
//!The box is the size of a full screenshot; the placeholder is smaller and a screenshot
//!of another size is centered in the same box.
class GuiSavePreview : public GuiElement
{
	public:
		//!\param w,h Box size, the size of a full screenshot
		//!\param phW,phH Size of the placeholder drawn when there is no screenshot
		GuiSavePreview(int w, int h, int phW, int phH);
		//!\param img The screenshot, or nullptr for the placeholder
		void setImage(GuiImageData * img);
		//!\param text What the placeholder says while there is no screenshot
		void setPlaceholderText(const char * text);
		void draw() override;
	private:
		GuiShape placeholder;
		GuiText noImage;
		GuiImage image;
		bool hasImage;
};

//!One card of the save browser. The browser only shows these: what a card means (a snapshot, a
//!save file, a "new" slot) and what happens when it is chosen is up to the screen that built them.
struct SaveEntry
{
	enum Kind { NEW, SAVE };
	Kind kind = SAVE;
	std::string title;				//!< "Snapshot 3"
	std::string subtitle;			//!< "Wed Oct 21, 9:02 PM", or a hint on a NEW card
	GuiImageData * preview = nullptr;	//!< The screenshot, not owned; nullptr shows the placeholder
	std::string placeholder;		//!< What the placeholder says (no screenshot)
	bool playing = false;			//!< Tagged "Playing from": the save the game was started from
	int id = -1;					//!< Whatever the caller wants to find this card by (an index into its own list)
};

//!A paged grid of save cards (two across), with a scrollbar. Choosing a card (A or a click) is reported
//!through getClickedSave(); the browser itself changes nothing.
class GuiSaveBrowser : public GuiElement
{
	public:
		GuiSaveBrowser(int w, int h);
		~GuiSaveBrowser();
		//!One card per row (wide), or two (the default). Call before setEntries().
		void setColumns(int columns);
		//!Replaces the cards. \param select The card (by position) that gets the cursor.
		void setEntries(const std::vector<SaveEntry>& list, int select = 0);
		//!\return The position of the card that was chosen since the last call, or -1.
		int getClickedSave();
		//!\return The position of the card under the cursor, or -1 with no cards.
		int getSelectedPosition() const;
		//!\return The button showing the card at `position` (for a popover to open beside), or nullptr when it is scrolled out of view.
		GuiButton * getEntryButton(int position);
		const SaveEntry * getEntry(int position) const;
		void resetState();
		void setFocus(int f);
		void draw();
		void update(InputController * c);
	protected:
		std::vector<SaveEntry> entries;
		int selectedItem;
		int listOffset;
		int cols;				//!< cards across: 1 or 2
		int pageSize() const { return 2 * cols; }
		GuiButton * saveBtn[SAVELISTSIZE];
		GuiText * saveTitle[SAVELISTSIZE];
		GuiText * saveSubtitle[SAVELISTSIZE];

		GuiShape * saveBgImg[SAVELISTSIZE];
		GuiSavePreview * savePreviewImg[SAVELISTSIZE];
		GuiShape * tagShape[SAVELISTSIZE];		// the "Playing from" tag, on the card's top edge
		GuiText * tagText[SAVELISTSIZE];

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;

		GuiShape * trackShape;
		GuiShape * thumbShape;		// shows which rows are on screen; not interactive
		GuiShape * arrowUpShape;
		GuiShape * arrowDownShape;
		GuiShape * arrowUpGlyph;
		GuiShape * arrowDownGlyph;

		int trackHeight;
		void updateThumb();

		GuiTrigger * trigA;

		bool saveBtnLastOver[SAVELISTSIZE];
};

#endif

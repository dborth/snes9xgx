#ifndef GUISAVEBROWSER_H
#define GUISAVEBROWSER_H

#include <memory>
#include <string>
#include <vector>
#include "Gui.h"

#define MAX_SAVES 				100
#define SAVELISTSIZE 			6		// three rows of two: the most the grid ever shows

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

//!The icon of a game's own save file, drawn in code: a small cartridge with a battery in its label. It stands
//!where a snapshot's screenshot does, because a save file has no picture of its own.
class GuiSaveFileIcon : public GuiElement
{
	public:
		GuiSaveFileIcon();
		void draw() override;
	private:
		GuiShape body;
		GuiShape label;
		GuiShape battery;
		GuiShape nub;
		GuiShape slot;
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
	bool pinned = false;			//!< Shown on its own in a wide card under the grid, with the save-file icon instead of a screenshot. At most one, and the last entry
	std::string chip;				//!< A small label at the right of a pinned card ("In-game save")
	bool playing = false;			//!< Tagged "Playing from": the save the game was started from
	int id = -1;					//!< Whatever the caller wants to find this card by (an index into its own list)
};

//!A paged grid of save cards (two across), with a scrollbar that shows only when there is more than fits. A pinned
//!entry is not part of the grid: it sits under it in a wide card, which the D-pad reaches by going down from the last
//!row. Choosing a card (A or a click) is reported through getClickedSave(); the browser itself changes nothing.
class GuiSaveBrowser : public GuiElement
{
	public:
		//!\param w Width, including the scrollbar column
		//!\param rows How many rows of cards the grid shows at once. A pinned card goes under them
		GuiSaveBrowser(int w, int rows);
		~GuiSaveBrowser();
		//!Replaces the cards. \param select The card (by position) that gets the cursor.
		void setEntries(const std::vector<SaveEntry>& list, int select = 0);
		//!\return The position of the card that was chosen since the last call, or -1.
		int getClickedSave();
		//!\return The position of the card under the cursor, or -1 with no cards.
		int getSelectedPosition() const;
		//!\return Where a popover for the card at `position` opens: beside the card, or for the pinned card over its label. Not valid when the card is scrolled out of view.
		GuiPopover::Anchor getEntryAnchor(int position);
		const SaveEntry * getEntry(int position) const;
		void resetState();
		void setFocus(int f);
		void draw();
		void update(InputController * c);
	protected:
		std::vector<SaveEntry> entries;
		int selectedItem;
		int listOffset;
		enum { COLS = 2 };
		int rows;				//!< rows of cards shown at once
		int pageSize() const { return rows * COLS; }
		bool hasPinned() const { return !entries.empty() && entries.back().pinned; }
		int gridCount() const { return (int)entries.size() - (hasPinned() ? 1 : 0); }
		int lastRowItem() const;
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

		GuiButton * pinnedBtn;		// the pinned card, under the grid
		GuiShape * pinnedBg;
		GuiSaveFileIcon * pinnedIcon;
		GuiText * pinnedTitle;
		GuiText * pinnedSubtitle;
		GuiShape * chipShape;
		GuiText * chipText;
		bool pinnedOn;				// the cursor is on the pinned card (the grid keeps its column in selectedItem)
		bool pinnedLastOver;

		int trackHeight;
		bool scrollable;			// more rows than fit: the scrollbar shows
		void updateThumb();

		GuiTrigger * trigA;

		bool saveBtnLastOver[SAVELISTSIZE];
};

#endif

#ifndef GUISAVEBROWSER_H
#define GUISAVEBROWSER_H

#include <memory>
#include <string>
#include <vector>
#include "libgui/Gui.h"

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
		//!A cartridge with an empty (gray) battery: a game that has no save file of its own
		void setNoBattery(bool none);
		void draw() override;
	private:
		GuiShape body;
		GuiShape label;
		GuiShape battery;
		GuiShape nub;
		GuiShape slot;
		bool noBattery;
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
	//!What a slot badge, on the screenshot's corner, says
	enum { BADGE_NONE = 0, BADGE_AUTO = -1, BADGE_AUTO_PAUSED = -2 };
	//!The tag on a card's top edge
	enum Tag { TAG_NONE, TAG_PLAYING, TAG_AUTOSAVE, TAG_PAUSED };
	bool pinned = false;			//!< Shown on its own in a wide card under the grid, with the save-file icon instead of a screenshot. At most one, and the last entry
	std::string chip;				//!< A small label at the right of a pinned card ("Auto-save on")
	bool chipOn = false;			//!< The label states something that is on: it gets a green dot and the accent color, and is gray otherwise
	int badge = BADGE_NONE;			//!< A slot number (1 or more) in a circle, or BADGE_AUTO / BADGE_AUTO_PAUSED in a pill, on the screenshot
	Tag tag = TAG_NONE;				//!< "Playing from" (the save the game was started from), "Autosave" (live), or "Paused" (an autosave nothing is updating)
	bool dimPreview = false;		//!< The screenshot is dimmed: it is not changing
	bool info = false;				//!< A note, not a card: dark, translucent, and the cursor skips it
	bool noBattery = false;			//!< (pinned) The icon is a cartridge with an empty battery
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
		//!How many rows of cards the grid shows at once (1 to 3). Call before setEntries() when it changes.
		void setRows(int r);
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
		//!The cursor can go to the pinned card: there is one, and it is not a note
		bool pinnedSelectable() const { return hasPinned() && !entries.back().info; }
		bool isInfoAt(int position) const { return position >= 0 && position < gridCount() && entries[(size_t)position].info; }
		int gridCount() const { return (int)entries.size() - (hasPinned() ? 1 : 0); }
		int lastRowItem() const;
		GuiButton * saveBtn[SAVELISTSIZE];
		GuiText * saveTitle[SAVELISTSIZE];
		GuiText * saveSubtitle[SAVELISTSIZE];

		GuiShape * saveBgImg[SAVELISTSIZE];
		GuiSavePreview * savePreviewImg[SAVELISTSIZE];
		GuiShape * tagShape[SAVELISTSIZE];		// the tag on the card's top edge
		GuiShape * tagDot[SAVELISTSIZE];		// the dot in the "Autosave" tag
		GuiText * tagText[SAVELISTSIZE];
		GuiShape * badgeAutoShape[SAVELISTSIZE];	// AUTO, on the screenshot's corner
		GuiText * badgeAutoText[SAVELISTSIZE];
		GuiShape * badgeNumShape[SAVELISTSIZE];	// a slot number, there
		GuiText * badgeNumText[SAVELISTSIZE];
		GuiShape * dimShape[SAVELISTSIZE];		// over a screenshot that is not changing
		int tagShown[SAVELISTSIZE];				// what each card's tag is drawn as now, so it is restyled only when it changes
		int badgeShown[SAVELISTSIZE];
		int infoShown[SAVELISTSIZE];

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;

		GuiShape * trackShape;
		GuiShape * thumbShape;		// the thumb: shows which rows are on screen
		GuiButton * thumbBtn;		// the thumb's hit area: drag it to scroll
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
		GuiShape * chipDot;
		GuiText * chipText;
		int chipState;				// what the chip is drawn as: -1 not yet, 0 off, 1 on, 2 on over a note
		int pinnedInfoShown;		// the pinned card is drawn as: -1 not yet, 0 a card, 1 a note
		bool pinnedOn;				// the cursor is on the pinned card (the grid keeps its column in selectedItem)
		bool pinnedLastOver;

		int trackHeight;
		int thumbHeight;			// current thumb size, in pixels
		int thumbTravel;			// how far the thumb can move, in pixels
		int dragPos;				// thumb offset while it is being dragged
		bool dragging;				// the thumb is held and following the pointer
		bool scrollable;			// more rows than fit: the scrollbar shows
		void updateThumb();

		GuiTrigger * trigA;
		GuiTrigger * trigHeldA;

		bool saveBtnLastOver[SAVELISTSIZE];
};

#endif

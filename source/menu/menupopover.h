/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * menupopover.h
 *
 * The menu's popover look: the one light panel every question asked in place
 * uses (see GuiPopover.h for where it goes and how it closes). A row, a rule and
 * a close button to build a panel from, and PickerView, the list of choices.
 ***************************************************************************/
#pragma once

#include "libgui/Gui.h"
#include "menu.h"

/**
 * A full-screen page on the menu's main window. It can be taken off the window while another screen runs on top (a
 * window only hands the D-pad to the topmost screen that way).
 */
class MenuScreenWindow
{
	public:
		MenuScreenWindow(GuiElement& view) :
			w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight()),
			shown(false)
		{
			w.append(&view);
			show();
		}
		~MenuScreenWindow() { hide(); }
		void show()
		{
			if(!shown)
			{
				MenuMainWindow().append(&w);
				shown = true;
			}
		}
		void hide()
		{
			if(shown)
			{
				MenuMainWindow().remove(&w);
				shown = false;
			}
		}
		GuiWindow& window() { return w; }
		// A disabled screen stays drawn, dimmed, and takes no input, so a modal put over it is the only thing the D-pad and pointer reach
		void setEnabled(bool on) { w.setState(on ? STATE::DEFAULT : STATE::DISABLED); }
	private:
		GuiWindow w;
		bool shown;
};

/** The space the menu's popovers keep clear: the header and footer bars and the page margin. */
GuiPopover::Insets MenuPopover_Insets();

/** One row of a popover list: the name on the left, a dot on the right when `marked` (the one in use), or a chevron when `chevron` (the row opens another list). The caller adds onClick / onFocus. */
GuiCard& MenuPopover_Row(UiScope& ui, int width, int height, const std::string& title, bool marked, bool dotSlot = true, const std::string& hint = std::string(), bool chevron = false, bool danger = false);

/** A chip that opens a popover: what it chooses in small capitals, what is chosen under it, an arrow pointing the way it opens. The caller adds onClick / onFocus. */
GuiCard& MenuPopover_Chip(UiScope& ui, int width, const char * what, std::function<std::string()> value);

/** How many characters of a hint (the small text under a row's name) fit `width` pixels, for wrapping it. */
size_t MenuPopover_HintChars(int width);

/** A one pixel rule `w` wide, in the outline color. */
GuiShape& MenuPopover_Divider(UiScope& ui, int w);

/** One line of a picker's detail pane: what a button does under that choice. */
struct PickerRow {
	std::string label;	/**< "A Button" */
	std::string value;	/**< "Key: Ctrl" */
};

/** A choice in a picker. */
struct PickerItem {
	std::string name;					/**< "Arrow keys" */
	std::vector<std::string> lines;		/**< What it is, a few short lines (at most three are shown) */
	std::string hint;					/**< A line under the name, on the row itself. When any item has one the list is "hinted": taller rows, and no caption under the list */
	std::vector<PickerRow> rows;		/**< What it does for the buttons (at most seven are shown) */
	bool current;						/**< The one in use now */
	bool preferred = false;				/**< The cursor starts here when nothing is `current` (no dot is drawn) */
	bool divider = false;				/**< A rule is drawn after this item: what follows is a different kind of choice */
	bool chevron = false;				/**< A chevron at the row's end: choosing it opens another list (a hinted list only) */
	bool danger = false;				/**< Removes something: the name is red */
};

/** How a picker differs from the usual: all optional. */
struct PickerOptions {
	int width = 0;						/**< Panel width in design pixels; 0 for the usual. Hints wrap to what is left of it */
	std::string footnote;				/**< A few muted lines under the list, after a rule: not a choice, just what the list is about */
};

/**
 * A list of choices in a popover: one flat panel beside the control that asked, over the screen it came from. Plain
 * rows (the one in use has a dot), a rule where an item says so, and under the list a caption for the row under the
 * cursor: its description. Nothing else is drawn on it.
 */
class PickerView : public GuiPopover
{
	public:
		/** `anchor` is the control it opens beside; with none it is centred. B or a click outside closes it (isDismissed()). An empty `title` leaves off the heading and the rule under it. */
		PickerView(const char * title, const std::vector<PickerItem>& items, const GuiPopover::Anchor& anchor = GuiPopover::Anchor(), const PickerOptions * options = nullptr);
		/** Whether a choice was made (after any confirmation). */
		bool chosen() const { return done; }
		/** The index of the item chosen. */
		int result() const { return picked; }
		/** The item under the cursor. */
		int focused() const { return focus; }
		/** What A does on an item: it is chosen. */
		void choose(int index);
		/** The cursor moved to an item . */
		void focusItem(int index);
		/** Puts the selection on the item in use (or the first), whatever it was on: once the popover has the focus. */
		void selectInUse();
		/** Puts the selection (and the cursor) on an item. */
		void selectItem(int index);
		/** Called with an item's index whenever the cursor moves to it, and once now: the screen under the popover previews it. */
		void onFocusChange(std::function<void(int)> fn);
		/** The caption lines under the list (always the same number, some empty). */
		const std::vector<std::string>& caption() const { return captionLines; }
		/** Where the panel is, and how tall its rows are (design pixels). */
		int panelX() const { return position().x; }
		int panelY() const { return position().y; }
		int panelW() const { return panelWidth; }
		int panelH() const { return panelHeight; }
		int rowH() const { return rowHeight; }
	private:
		void refreshCaption();

		std::vector<PickerItem> items;
		int picked;
		bool done;
		int focus;
		std::vector<GuiCard*> itemCards;
		std::vector<std::string> captionLines;
		std::function<void(int)> focusCb;
		int inUse;
		int panelWidth, panelHeight, rowHeight;
};

/**
 * Splits a sentence into lines of at most `max` characters, at spaces, and appends them to `lines`: for the panes and
 * captions of a popover, which cannot wrap text (a label does not wrap).
 */
void MenuPopover_Wrap(std::vector<std::string>& lines, const std::string& text, size_t max);

/**
 * Runs a PickerView as a popover over the screen that is showing (which the caller has disabled and which stays drawn),
 * until a choice is made or it is closed. The index chosen, or -1. `preview` is told which item has the focus; `perFrame`
 * runs every frame (for what must be checked while it is open).
 */
int MenuPopover_Pick(const char * title, const std::vector<PickerItem>& items, const GuiPopover::Anchor& anchor,
	std::function<void(int)> preview = nullptr, std::function<void()> perFrame = nullptr, const PickerOptions * options = nullptr);

/** A question with a few answers and a line of detail, as a popover (see MenuPopover_Pick). The index of the answer, or -1. */
int MenuPopover_Ask(const char * title, const std::vector<std::string>& answers, const std::string& detail, const GuiPopover::Anchor& anchor);

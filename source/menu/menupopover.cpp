/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * menupopover.cpp
 *
 * See menupopover.h.
 ***************************************************************************/
#include "menupopover.h"
#include "menustyle.h"
#include <algorithm>

GuiPopover::Insets MenuPopover_Insets()
{
	GuiPopover::Insets i = { MENU_BAR_TOP_H, MENU_BAR_BOTTOM_H, MENU_PAGE_MARGIN };
	return i;
}

GuiShape& MenuPopover_Divider(UiScope& ui, int w)
{
	GuiShape& line = ui.make<GuiShape>(w, 1);
	line.setPaint(GuiPaint().color(MenuPaletteCurrent().outline));
	return line;
}

GuiCard& MenuPopover_Row(UiScope& ui, int width, int height, const std::string& title, bool marked, bool withDotSlot, const std::string& hint)
{
	const int padX = 10, dotSlot = withDotSlot ? 14 : 0;
	const int textW = width - 2 * padX - (withDotSlot ? dotSlot + 8 : 0);
	GuiLabel& name = ui.label(title.c_str()).fontSize(16);
	name.setMaxWidth(textW);
	GuiLayout& dotBox = ui.row().size(dotSlot, dotSlot).align(LAYOUT_ALIGN::CENTRE);
	if(marked)
	{
		GuiShape& dot = ui.make<GuiShape>(10, 10);
		dot.setShape(SHAPE::ELLIPSE);
		dot.setPaint(GuiPaint().color(MenuPaletteCurrent().accent));
		dotBox.add(dot);
	}

	GuiLayout& content = ui.row().size(width - 2 * padX, height).gap(8).align(LAYOUT_ALIGN::CENTRE);
	if(hint.empty())
		content.add(name);
	else
	{
		// the name with a line of what it does under it, the two together centered in the row
		GuiLabel& about = ui.label(hint.c_str()).fontSize(UI_FONT_SMALL).muted();
		about.setMaxWidth(textW);
		const int textH = 20 + 3 + 16;
		GuiLayout& text = ui.column().size(textW, height).gap(3).pad(0, (height - textH) / 2);
		text.add(name).add(about);
		content.add(text);
	}
	content.addSpace();
	if(withDotSlot)
		content.add(dotBox);
	GuiCard& row = ui.card();
	row.size(width, height).style(MenuRowHighlightStyle()).corners(GuiCorners(8.0f)).pad(padX, 0);
	row.add(content);
	return row;
}

// ---- the list picker

namespace {

const int kPopoverW = 280;
const int kHintedPopoverW = 262;	// narrower, so beside a card it leaves the card showing
const int kHintedRowH = 50;
const int kPopoverPad = 10;
const int kPopoverTitleH = 24;
const int kPopoverCaptionLineH = 16;
const int kPopoverDividerH = 9;
const int kPopoverRowGap = 2;
const int kPopoverCaptionChars = 38;	// at the caption's size, in the width inside the padding

}

void PickerView::choose(int index)
{
	if(index < 0 || (size_t)index >= items.size())
		return;
	picked = index;
	done = true;
}

void PickerView::focusItem(int index)
{
	if(index < 0 || (size_t)index >= items.size())
		return;
	const bool moved = focus != index;
	focus = index;
	refreshCaption();
	if(moved && focusCb)
		focusCb(index);
}

void PickerView::onFocusChange(std::function<void(int)> fn)
{
	focusCb = fn;
	refreshCaption();
	if(focusCb && !items.empty())
		focusCb(focus);
}

// The caption under the list: what the row under the cursor is
void PickerView::refreshCaption()
{
	if(captionLines.empty())
		return;	// a hinted list: each row says what it does itself
	for(size_t i = 0; i < captionLines.size(); i++)
		captionLines[i].clear();
	if(items.empty() || focus < 0 || (size_t)focus >= items.size())
		return;
	const PickerItem& it = items[(size_t)focus];
	std::string text;
	for(size_t i = 0; i < it.lines.size(); i++)
		text += (text.empty() ? "" : " ") + it.lines[i];
	// the rows say what the choice does for each button; a screen that previews the choice shows that itself
	if(!focusCb)
		for(size_t i = 0; i < it.rows.size(); i++)
			text += (text.empty() ? "" : (i == 0 ? "  " : ", ")) + it.rows[i].label + ": " + it.rows[i].value;
	std::vector<std::string> wrapped;
	MenuPopover_Wrap(wrapped, text, kPopoverCaptionChars);
	for(size_t i = 0; i < captionLines.size() && i < wrapped.size(); i++)
		captionLines[i] = wrapped[i];
	if(wrapped.size() > captionLines.size())
	{
		std::string& last = captionLines.back();
		if(last.size() > 2)
			last.resize(last.size() - 2);
		last += "..";
	}
}

PickerView::PickerView(const char * title, const std::vector<PickerItem>& itemList, const GuiPopover::Anchor& anchor) :
	GuiPopover(anchor, MenuPopover_Insets()),
	items(itemList), picked(-1), done(false), focus(0), inUse(0), panelWidth(kPopoverW), panelHeight(0), rowHeight(0)
{
	int current = 0;
	bool found = false;
	for(size_t i = 0; i < items.size(); i++)
		if(items[i].current || (items[i].preferred && !found))
		{
			current = (int)i;
			found = found || items[i].current;
		}
	focus = current;
	inUse = current;

	// Size: a short list gets roomy rows; a long one shrinks its rows (down to 28 px) to stay between the bars.
	// A hinted list (each row has a line saying what it does) has taller rows, a narrower panel, and no caption.
	bool hinted = false;
	for(size_t i = 0; i < items.size(); i++)
		hinted = hinted || !items[i].hint.empty();
	panelWidth = hinted ? kHintedPopoverW : kPopoverW;
	const int n = (int)items.size();
	int dividers = 0;
	for(size_t i = 0; i < items.size(); i++)
		dividers += items[i].divider ? 1 : 0;
	const int captionLinesN = hinted ? 0 : (n <= 5 ? 3 : 2);
	captionLines.assign((size_t)captionLinesN, std::string());
	const int captionH = hinted ? 0 : captionLinesN * kPopoverCaptionLineH + 4;
	const int fixedH = 2 * kPopoverPad + kPopoverTitleH + 7 + (hinted ? 0 : 8) + captionH;
	const int spare = maxPanelHeight() - fixedH - dividers * kPopoverDividerH - std::max(0, n - 1) * kPopoverRowGap;
	if(hinted)
		rowHeight = std::max(40, std::min(kHintedRowH, n > 0 ? spare / n : kHintedRowH));
	else
		rowHeight = std::max(28, std::min(36, n > 0 ? spare / n : 36));
	const int listH = n * rowHeight + std::max(0, n - 1) * kPopoverRowGap + dividers * kPopoverDividerH;
	panelHeight = fixedH + listH;
	const int innerW = panelWidth - 2 * kPopoverPad;

	// the list: plain rows, a dot on the one in use, the highlight tint on the one under the cursor
	GuiLayout& list = ui.column().gap(kPopoverRowGap).size(innerW, listH);
	for(size_t i = 0; i < items.size(); i++)
	{
		const int idx = (int)i;
		GuiCard& row = MenuPopover_Row(ui, innerW, rowHeight, items[i].name, items[i].current, !hinted, items[i].hint);
		row.onClick([this, idx]{ choose(idx); });
		row.onFocus([this, idx]{ focusItem(idx); });
		itemCards.push_back(&row);
		list.add(row);
		if(items[i].divider)
			list.add(ui.row().size(innerW, kPopoverDividerH).align(LAYOUT_ALIGN::CENTRE).add(MenuPopover_Divider(ui, innerW)));
	}

	// what the popover is about, then a rule; the caption goes under the list
	GuiLabel& heading = ui.label(title).fontSize(UI_FONT_SMALL).muted();
	GuiLayout& content = ui.column().size(innerW, panelHeight - 2 * kPopoverPad)
		.add(ui.row().size(innerW, kPopoverTitleH).align(LAYOUT_ALIGN::CENTRE).add(heading))
		.add(MenuPopover_Divider(ui, innerW))
		.addGap(6)
		.add(list);
	if(!hinted)
	{
		GuiLayout& captionColumn = ui.column().size(innerW, captionH).pad(0, 0);
		for(int k = 0; k < captionLinesN; k++)
		{
			GuiLabel& line = ui.label().fontSize(UI_FONT_SMALL).muted().bind([this, k]{ return captionLines[(size_t)k]; });
			captionColumn.add(ui.row().size(innerW, kPopoverCaptionLineH).align(LAYOUT_ALIGN::CENTRE).add(line));
		}
		content.addGap(8).add(captionColumn);
	}

	GuiCard& panel = ui.card().selectable(false).style(MenuPopoverStyle()).corners(GuiCorners(12.0f)).pad(kPopoverPad, kPopoverPad).size(panelWidth, panelHeight);
	panel.add(content);

	setPanel(panel, MenuPaletteCurrent().surfaceRaised);

	refreshCaption();
	// the window steps the selection down from what is selected (or to the first control when nothing is): start just
	// above the item in use, so the step lands on it
	if(current > 0)
		itemCards[(size_t)current - 1]->select();
}

void PickerView::selectItem(int index)
{
	if(index < 0 || (size_t)index >= itemCards.size())
		return;
	for(size_t i = 0; i < itemCards.size(); i++)
		itemCards[i]->deselect();
	itemCards[(size_t)index]->select();
	focusItem(index);
}

void PickerView::selectInUse()
{
	if(itemCards.empty())
		return;
	for(size_t i = 0; i < itemCards.size(); i++)
		itemCards[i]->deselect();
	itemCards[(size_t)inUse < itemCards.size() ? (size_t)inUse : 0]->select();
}

// Splits a sentence into lines of at most `max` characters, at spaces, so it fits a pane that cannot wrap text.
void MenuPopover_Wrap(std::vector<std::string>& lines, const std::string& text, size_t max)
{
	size_t start = 0;
	while(start < text.size())
	{
		size_t end = start + max;
		if(end >= text.size())
			end = text.size();
		else
		{
			const size_t space = text.rfind(' ', end);
			if(space != std::string::npos && space > start)
				end = space;
		}
		lines.push_back(text.substr(start, end - start));
		start = end < text.size() && text[end] == ' ' ? end + 1 : end;
	}
}

// ---- running one on the menu's main window

int MenuPopover_Pick(const char * title, const std::vector<PickerItem>& items, const GuiPopover::Anchor& anchor,
	std::function<void(int)> preview, std::function<void()> perFrame)
{
	PickerView view(title, items, anchor);
	if(preview)
		view.onFocusChange(preview);
	MenuScreenWindow screen(view);
	// The screen under it still holds the focus, which only moves by itself when that screen is removed. Hand it over
	// once the popover has been laid out: taking it steps the selection down, and that needs to know where things are.
	MenuUpdateGui();
	MenuMainWindow().changeFocus(&screen.window());
	view.selectInUse(); // (taking the focus stepped the selection, from wherever it was)

	while(true)
	{
		if(!MenuUpdateGui())
			return -1;	// the app is exiting
		if(perFrame)
			perFrame();
		if(view.chosen())
			return view.result();
		if(view.isDismissed())
			return -1;
	}
}

int MenuPopover_Ask(const char * title, const std::vector<std::string>& answers, const std::string& detail, const GuiPopover::Anchor& anchor)
{
	std::vector<PickerItem> items;
	for(size_t i = 0; i < answers.size(); i++)
	{
		PickerItem it;
		it.name = answers[i];
		it.current = false;
		MenuPopover_Wrap(it.lines, detail, 44);
		items.push_back(it);
	}
	return MenuPopover_Pick(title, items, anchor);
}

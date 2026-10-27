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

size_t MenuPopover_HintChars(int width)
{
	// the small text runs about 5 px a character; this allows a little over 5.6, so a line never clips
	return (size_t)std::max(8, width * 10 / 56);
}

GuiCard& MenuPopover_Row(UiScope& ui, int width, int height, const std::string& title, bool marked, bool withDotSlot, const std::string& hint, bool chevron, bool danger)
{
	const int padX = 10, dotSlot = withDotSlot ? 14 : 0, chevronSlot = chevron ? 16 : 0;
	const int textW = width - 2 * padX - (withDotSlot ? dotSlot + 8 : 0) - (chevron ? chevronSlot + 8 : 0);
	GuiLabel& name = ui.label(title.c_str()).fontSize(16);
	name.setMaxWidth(textW);
	if(danger)
		name.setColor(MenuRGBA(176, 42, 42));
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
		// the name with what it does under it (a line, or a few when it is long), the lot centered in the row
		std::vector<std::string> aboutLines;
		MenuPopover_Wrap(aboutLines, hint, MenuPopover_HintChars(textW));
		// no fixed size: the column is as tall as its lines, and the row centers it
		GuiLayout& text = ui.column().gap(3);
		text.add(name);
		for(size_t k = 0; k < aboutLines.size(); k++)
		{
			GuiLabel& about = ui.label(aboutLines[k].c_str()).fontSize(UI_FONT_SMALL).muted();
			about.setMaxWidth(textW);
			text.add(about);
		}
		content.add(text);
	}
	content.addSpace();
	if(withDotSlot)
		content.add(dotBox);
	if(chevron)
		content.add(ui.row().size(chevronSlot, height).align(LAYOUT_ALIGN::CENTRE).add(ui.label("\xE2\x80\xBA").fontSize(26).muted()));
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
const int kHintedLineH = 16;		// one line of a hint
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

PickerView::PickerView(const char * title, const std::vector<PickerItem>& itemList, const GuiPopover::Anchor& anchor, const PickerOptions * options) :
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
	panelWidth = (options && options->width > 0) ? options->width : (hinted ? kHintedPopoverW : kPopoverW);
	const int innerW = panelWidth - 2 * kPopoverPad;

	// the most lines any hint wraps to: rows are all as tall as the tallest
	bool anyCurrent = false;
	for(size_t i = 0; i < items.size(); i++)
		anyCurrent = anyCurrent || items[i].current;
	// a hinted list has no room for the dot, unless one of its rows is in use
	const bool dotSlot = !hinted || anyCurrent;

	int hintLinesMax = 1;
	for(size_t i = 0; hinted && i < items.size(); i++)
	{
		std::vector<std::string> wrapped;
		const int textW = innerW - 20 - (dotSlot ? 22 : 0) - (items[i].chevron ? 24 : 0);
		MenuPopover_Wrap(wrapped, items[i].hint, MenuPopover_HintChars(textW));
		hintLinesMax = std::max(hintLinesMax, (int)wrapped.size());
	}

	// a footnote: a rule and a few muted lines under the list
	std::vector<std::string> footLines;
	if(options && !options->footnote.empty())
		MenuPopover_Wrap(footLines, options->footnote, MenuPopover_HintChars(innerW - 8));
	const int footH = footLines.empty() ? 0 : kPopoverDividerH + 4 + (int)footLines.size() * kHintedLineH + 4;
	const int n = (int)items.size();
	int dividers = 0;
	for(size_t i = 0; i < items.size(); i++)
		dividers += items[i].divider ? 1 : 0;
	const int captionLinesN = hinted ? 0 : (n <= 5 ? 3 : 2);
	captionLines.assign((size_t)captionLinesN, std::string());
	const int captionH = hinted ? 0 : captionLinesN * kPopoverCaptionLineH + 4;
	const bool titled = title != nullptr && title[0] != 0;
	const int headingH = titled ? kPopoverTitleH + 7 : 0;	// the heading, the rule under it and the gap after
	const int fixedH = 2 * kPopoverPad + headingH + (hinted ? 0 : 8) + captionH + footH;
	const int spare = maxPanelHeight() - fixedH - dividers * kPopoverDividerH - std::max(0, n - 1) * kPopoverRowGap;
	if(hinted)
	{
		const int wanted = kHintedRowH + (hintLinesMax - 1) * kHintedLineH;
		rowHeight = std::max(40, std::min(wanted, n > 0 ? spare / n : wanted));
	}
	else
		rowHeight = std::max(28, std::min(36, n > 0 ? spare / n : 36));
	const int listH = n * rowHeight + std::max(0, n - 1) * kPopoverRowGap + dividers * kPopoverDividerH;
	panelHeight = fixedH + listH;

	// the list: plain rows, a dot on the one in use, the highlight tint on the one under the cursor
	GuiLayout& list = ui.column().gap(kPopoverRowGap).size(innerW, listH);
	for(size_t i = 0; i < items.size(); i++)
	{
		const int idx = (int)i;
		GuiCard& row = MenuPopover_Row(ui, innerW, rowHeight, items[i].name, items[i].current, dotSlot, items[i].hint, items[i].chevron, items[i].danger);
		row.onClick([this, idx]{ choose(idx); });
		row.onFocus([this, idx]{ focusItem(idx); });
		itemCards.push_back(&row);
		list.add(row);
		if(items[i].divider)
			list.add(ui.row().size(innerW, kPopoverDividerH).align(LAYOUT_ALIGN::CENTRE).add(MenuPopover_Divider(ui, innerW)));
	}

	// what the popover is about, then a rule; the caption goes under the list
	GuiLayout& content = ui.column().size(innerW, panelHeight - 2 * kPopoverPad);
	if(titled)
	{
		GuiLabel& heading = ui.label(title).fontSize(UI_FONT_SMALL).muted();
		content.add(ui.row().size(innerW, kPopoverTitleH).align(LAYOUT_ALIGN::CENTRE).add(heading))
			.add(MenuPopover_Divider(ui, innerW))
			.addGap(6);
	}
	content.add(list);
	if(!footLines.empty())
	{
		content.add(ui.row().size(innerW, kPopoverDividerH).align(LAYOUT_ALIGN::CENTRE).add(MenuPopover_Divider(ui, innerW))).addGap(4);
		for(size_t k = 0; k < footLines.size(); k++)
		{
			GuiLabel& line = ui.label(footLines[k].c_str()).fontSize(UI_FONT_SMALL).muted();
			content.add(ui.row().size(innerW, kHintedLineH).align(LAYOUT_ALIGN::CENTRE).pad(4, 0).add(line));
		}
	}
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

GuiCard& MenuPopover_Chip(UiScope& ui, int width, const char * what, std::function<std::string()> value)
{
	GuiLabel& caption = ui.label(what).fontSize(UI_FONT_SMALL - 3).muted();
	GuiLabel& chosen = ui.label().fontSize(UI_FONT_SMALL + 1).bind(value);
	chosen.setMaxWidth(width - 20 - 16);
	GuiShape& arrow = ui.own<GuiShape>(10, 6);
	arrow.setArrow(SHAPE::TRIANGLE_DOWN, MenuBarArrowRest(), MenuBarArrowRest());
	GuiLayout& content = ui.row().size(width - 20, 0).gap(6).align(LAYOUT_ALIGN::CENTRE)
		.add(ui.column().gap(1).add(caption).add(chosen)).addSpace().add(arrow);
	GuiCard& chip = ui.card();
	chip.size(width, 40).style(MenuChipStyle()).corners(GuiCorners(10.0f)).pad(10, 3);
	chip.add(content);
	return chip;
}

int MenuPopover_Pick(const char * title, const std::vector<PickerItem>& items, const GuiPopover::Anchor& anchor,
	std::function<void(int)> preview, std::function<void()> perFrame, const PickerOptions * options)
{
	PickerView view(title, items, anchor, options);
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

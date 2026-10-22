/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * mappingchooser.cpp
 *
 * See mappingchooser.h.
 ***************************************************************************/

#include <algorithm>
#include <string>
#include <vector>

#include "mappingchooser.h"
#include "menupopover.h"
#include "menustyle.h"
#include "menu.h"
#include "button_mapping.h"
#include "mappingmodel.h"
#include "drivers/InputController.h"

extern GuiTextTranslator * textTranslator;

namespace {

std::string Tr(const char * s)
{
	return textTranslator ? std::string(textTranslator->getText(s)) : std::string(s);
}

std::string ButtonText(int hw, uint32_t btn)
{
	const char * name = MappingButtonName(hw, btn);
	return name ? std::string(name) : Tr("None");
}

// How the hardware says "forget this mapping" (the prompt has always told the user)
const char * ClearHint(int hw)
{
	switch(hw)
	{
		case INPUT_HW_GAMECUBE: return "Home or the C-Stick clears it.";
		case INPUT_HW_DS4: return "PS clears it.";
	}
	return "Home clears it.";
}

enum { KIND_PRESS = 0, KIND_BUTTONS, KIND_NONE, KIND_COUNT };

class ChooserView : public GuiPopover
{
	public:
		ChooserView(const char * rowName, int hw, uint32_t now, uint32_t defaultButton, const GuiPopover::Anchor& anchor);

		bool chosen() const { return done; }
		uint32_t result() const { return picked; }
		bool listening() const { return shown == KIND_PRESS; }
		//!Takes what was pressed on the hardware since the last frame as the answer (see below)
		void poll();
		//!Puts the selection on the kind that is showing
		void selectKind();

	private:
		void showKind(int kind) { shown = kind; hintText = defaultHint(); }
		std::string defaultHint() const { return Tr("Move over a kind to see its choices"); }
		void choose(uint32_t btn) { picked = btn; done = true; }

		int hardware;
		uint32_t current;
		int shown;
		bool done;
		uint32_t picked;
		std::string hintText;
		std::vector<GuiCard*> kindCards;
		int panelWidth, panelHeight;
};

// While waiting for a button: the first one pressed on the hardware being mapped is the answer, whatever it is.
// Home on any of them, or the C-Stick on a GameCube pad, clears the mapping.
void ChooserView::poll()
{
	if(done || !listening() || !controller[0])
		return;
	const InputPadData& pad = controller[0]->getPadData();
	uint32_t pressed = pad.hw_buttons_d[hardware];

	if(hardware == INPUT_HW_GAMECUBE)
	{
		if(pad.hw_substickX[INPUT_HW_GAMECUBE] < -0.55f || pad.hw_substickX[INPUT_HW_GAMECUBE] > 0.55f ||
		   pad.hw_substickY[INPUT_HW_GAMECUBE] < -0.55f || pad.hw_substickY[INPUT_HW_GAMECUBE] > 0.55f)
			pressed = INPUT_BTN_HOME;
	}
	if(pressed == 0)
		return;
	if(pressed & INPUT_BTN_HOME)
		pressed = 0;						// clears
	else
		pressed &= ~pressed + 1;			// two at once: the lower one, so the answer is one button
	choose(pressed);
}

void ChooserView::selectKind()
{
	for(size_t i = 0; i < kindCards.size(); i++)
		kindCards[i]->deselect();
	kindCards[(size_t)shown < kindCards.size() ? (size_t)shown : 0]->select();
}

ChooserView::ChooserView(const char * rowName, int hw, uint32_t now, uint32_t defaultButton, const GuiPopover::Anchor& anchor) :
	GuiPopover(anchor, MenuPopover_Insets()),
	hardware(hw), current(now), shown(KIND_PRESS), done(false), picked(0), panelWidth(372), panelHeight(0)
{
	hintText = defaultHint();

	const int pad = 10, inner = panelWidth - 2 * pad;
	const int leftW = 112, gap = 8, rightW = inner - leftW - gap;
	const int rowH = 26, rowGap = 2, kindH = 30;

	// the hardware's buttons, as many as it has
	std::vector<uint32_t> buttons;
	const CtrlrMap& def = ctrlr_def[hw];
	for(int i = 0; i < def.num_btns && i < 15; i++)
		if(def.map[i].btn != 0)
			buttons.push_back(def.map[i].btn);

	// ---- the kinds: what the right side shows for each

	// Press a button: what the old prompt said, and how to clear
	GuiLayout& press = ui.column().gap(3).size(rightW, 0);
	{
		std::vector<std::string> lines;
		MenuPopover_Wrap(lines, GuiFormat("Press a button on the %s now.", Tr(ctrlrName[hw]).c_str()), 28);
		for(size_t i = 0; i < lines.size() && i < 3; i++)
			press.add(ui.label(lines[i].c_str()).fontSize(UI_FONT_SMALL + 1));
		press.addGap(4);
		std::vector<std::string> clear;
		MenuPopover_Wrap(clear, Tr(ClearHint(hw)), 30);
		for(size_t i = 0; i < clear.size() && i < 2; i++)
			press.add(ui.label(clear[i].c_str()).fontSize(UI_FONT_SMALL).muted());
	}

	// Buttons: the hardware's buttons as keys, the one in use outlined
	const int cols = 3;
	const int cellW = (rightW - (cols - 1) * rowGap) / cols;
	GuiLayout& list = ui.column().gap(rowGap).size(rightW, 0);
	{
		GuiLayout * line = nullptr;
		for(size_t i = 0; i < buttons.size(); i++)
		{
			if(i % cols == 0)
			{
				line = &ui.row().gap(rowGap);
				list.add(*line);
			}
			const uint32_t btn = buttons[i];
			GuiLabel& name = ui.label(ButtonText(hw, btn).c_str()).fontSize(UI_FONT_SMALL + 1);
			name.setMaxWidth(cellW - 8);
			GuiCard& cell = ui.card();
			cell.size(cellW, rowH).style(MenuKeycapStyle(btn == now)).corners(GuiCorners(7.0f)).pad(4, 0);
			cell.add(name, GuiItem().at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE));
			cell.onClick([this, btn]{ choose(btn); });
			cell.onFocus([this, btn]{ hintText = GuiFormat(Tr("A uses %s").c_str(), ButtonText(hardware, btn).c_str()); });
			line->add(cell);
		}
	}
	const int listH = (int)((buttons.size() + cols - 1) / cols) * (rowH + rowGap);

	// None
	GuiLayout& none = ui.column().gap(6).size(rightW, 0);
	{
		GuiCard& clear = MenuPopover_Row(ui, rightW, rowH + 2, Tr("Leave unassigned").c_str(), now == 0);
		clear.onClick([this]{ choose(0); });
		none.add(clear);
		none.add(ui.label(GuiFormat(Tr("%s will do nothing.").c_str(), Tr(rowName).c_str()).c_str()).fontSize(UI_FONT_SMALL).muted());
	}

	struct Kind { const char * name; GuiLayout * layout; int height; };
	const Kind kinds[KIND_COUNT] = {
		{ "Press a button", &press, 90 },
		{ "Buttons", &list, listH },
		{ "None", &none, rowH + 2 + 6 + 18 }
	};

	// one fixed size for every kind, so the panel does not jump as the cursor moves down the left
	int bodyH = KIND_COUNT * (kindH + rowGap);
	for(int i = 0; i < KIND_COUNT; i++)
		bodyH = std::max(bodyH, kinds[i].height);
	const int headH = 24, footH = 26;
	panelHeight = 2 * pad + headH + 7 + bodyH + 8 + footH;

	// the kinds down the left; the one showing is filled
	GuiLayout& kindColumn = ui.column().gap(rowGap).size(leftW, bodyH);
	for(int i = 0; i < KIND_COUNT; i++)
	{
		GuiCard& row = MenuPopover_Row(ui, leftW, kindH, Tr(kinds[i].name), false, false);
		row.onFocus([this, i]{ showKind(i); });
		row.onClick([this, i]{ showKind(i); });
		kindCards.push_back(&row);
		kindColumn.add(row);
	}
	{
		std::vector<GuiCard*>& cards = kindCards;
		int styled = -1;
		ui.bind([&cards, styled, this]() mutable {
			if(styled == shown)
				return;
			styled = shown;
			for(size_t k = 0; k < cards.size(); k++)
				cards[k]->style((int)k == shown ? MenuPopoverShownStyle() : MenuRowHighlightStyle());
		});
	}

	GuiLayout& stack = ui.overlay().size(rightW, bodyH);
	for(int i = 0; i < KIND_COUNT; i++)
	{
		stack.add(*kinds[i].layout, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START));
		ui.visibleWhen(*kinds[i].layout, [this, i]{ return shown == i; });
	}

	// the head says what is being set and what it is now; the foot says what the cursor is on, and offers the default
	GuiLabel& title = ui.label(GuiFormat(Tr("What should %s use?").c_str(), Tr(rowName).c_str()).c_str()).fontSize(UI_FONT_SMALL).muted();
	GuiLabel& nowLabel = ui.label(GuiFormat(Tr("Now: %s").c_str(), ButtonText(hw, now).c_str()).c_str()).fontSize(UI_FONT_SMALL).muted();
	GuiLabel& hint = ui.label().fontSize(UI_FONT_SMALL).muted().bind([this]{ return hintText; });
	hint.setMaxWidth(inner - 78);
	const uint32_t def0 = defaultButton;
	GuiTextButton& useDefault = ui.button(Tr("Default").c_str()).size(70, footH).fontSize(UI_FONT_SMALL).style(MenuKeycapStyle())
		.rounded(8.0f).litTextColor(MenuPaletteCurrent().text)
		.onClick([this, def0]{ choose(def0); })
		.onFocus([this, def0, hw]{ hintText = GuiFormat(Tr("A uses the default: %s").c_str(), ButtonText(hw, def0).c_str()); });

	GuiLayout& content = ui.column().size(inner, panelHeight - 2 * pad)
		.add(ui.row().size(inner, headH).align(LAYOUT_ALIGN::CENTRE).add(title).addSpace().add(nowLabel))
		.add(MenuPopover_Divider(ui, inner))
		.addGap(6)
		.add(ui.row().size(inner, bodyH).gap(gap).align(LAYOUT_ALIGN::START).add(kindColumn).add(stack))
		.addGap(8)
		.add(ui.row().size(inner, footH).align(LAYOUT_ALIGN::CENTRE).add(hint).addSpace().add(useDefault));

	GuiCard& panel = ui.card().selectable(false).style(MenuPopoverStyle()).corners(GuiCorners(12.0f)).pad(pad, pad).size(panelWidth, panelHeight);
	panel.add(content);
	setPanel(panel, MenuPaletteCurrent().surfaceRaised);
}

} // namespace

bool MappingChoose(const char * rowName, int hw, uint32_t now, uint32_t defaultButton, const GuiPopover::Anchor& anchor, uint32_t & chosen)
{
	ChooserView view(rowName, hw, now, defaultButton, anchor);
	MenuScreenWindow screen(view);
	if(!MenuUpdateGui())
		return false;
	MenuMainWindow().changeFocus(&screen.window());
	screen.window().resetState();	// taking the focus selected the first control; the kind showing is the one to start on
	view.selectKind();

	while(true)
	{
		if(!MenuUpdateGui())
			return false;
		// A press on the hardware being mapped is the answer, even a B (which also asks to close the popover) or an A
		// that is a click outside the panel: look before the closing
		view.poll();
		if(view.chosen())
		{
			chosen = view.result();
			return true;
		}
		if(view.isDismissed())
			return false;
	}
}

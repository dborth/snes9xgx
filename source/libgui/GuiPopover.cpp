/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiPopover.cpp
 *
 * See GuiPopover.h.
 ***************************************************************************/
#include "Gui.h"
#include <algorithm>

static const int kGap = 6;	// between the anchor and the panel

// ---- placement: pure arithmetic on rectangles

int GuiPopover::maxHeight(int screenHeight, const Insets& insets)
{
	return screenHeight - insets.top - insets.bottom - 2 * kGap;
}

GuiPopover::Position GuiPopover::place(const Anchor& a, int w, int h, int screenW, int screenH, const Insets& insets)
{
	const int top = insets.top + kGap;
	const int bottom = screenH - insets.bottom - kGap;
	Position p;
	if(!a.valid)
	{
		p.x = (screenW - w) / 2;
		p.y = (screenH - h) / 2;
	}
	else
	{
		// Straight under its anchor if that fits above the footer bar, else straight over it; a chip in a bar
		// gets a panel that comes right out of it. Otherwise (a row in the middle of the page) it keeps to the band.
		const int below = a.y + a.h + kGap, above = a.y - h - kGap;
		if(below + h <= bottom)
			p.y = below;
		else if(above >= top)
			p.y = above;
		else
			p.y = std::max(top, std::min(below, bottom - h));
		if(a.edge == Anchor::LEFT_MARGIN)
			p.x = insets.side;
		else if(a.edge == Anchor::RIGHT_MARGIN)
			p.x = screenW - insets.side - w;
		else
			p.x = (a.x + a.w / 2 > screenW / 2) ? a.x + a.w - w : a.x;
	}
	p.x = std::max(insets.side, std::min(p.x, screenW - insets.side - w));
	return p;
}

GuiPopover::Tail GuiPopover::tailAt(const Anchor& a, const Position& at, int panelW, int panelH)
{
	const int len = 8, base = 16, edge = 18;	// how far it sticks out, how wide it is, how far from a corner it stays
	Tail t = { false, TAIL::UP, 0, 0, 0, 0 };
	if(!a.valid)
		return t;
	const int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
	if(at.y >= a.y + a.h)			// below it: pointing up, under the anchor's middle
	{
		t = { true, TAIL::UP, std::max(at.x + edge, std::min(ax - base / 2, at.x + panelW - edge - base)), at.y - len + 1, base, len };
	}
	else if(at.y + panelH <= a.y)	// above it: pointing down
	{
		t = { true, TAIL::DOWN, std::max(at.x + edge, std::min(ax - base / 2, at.x + panelW - edge - base)), at.y + panelH - 1, base, len };
	}
	else if(at.x >= ax)				// beside it, on its right (it may cover the anchor's end): pointing left, level with the anchor
	{
		t = { true, TAIL::LEFT, at.x - len + 1, std::max(at.y + edge, std::min(ay - base / 2, at.y + panelH - edge - base)), len, base };
	}
	else if(at.x + panelW <= ax)	// on its left: pointing right
	{
		t = { true, TAIL::RIGHT, at.x + panelW - 1, std::max(at.y + edge, std::min(ay - base / 2, at.y + panelH - edge - base)), len, base };
	}
	return t;
}

// ---- the popover

GuiPopover::GuiPopover(const Anchor& anchorAt, const Insets& keepClear) :
	GuiPopover(anchorAt, keepClear, platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight())
{
}

GuiPopover::GuiPopover(const Anchor& anchorAt, const Insets& keepClear, int screenWidth, int screenHeight) :
	anchor(anchorAt), insets(keepClear), screenW(screenWidth), screenH(screenHeight), cancel(nullptr), click(nullptr), panel(nullptr), dismissed(false), by(-1)
{
	placed.x = 0;
	placed.y = 0;
	placedTail = { false, TAIL::UP, 0, 0, 0, 0 };
	cancel = &ui.own<GuiTrigger>();
	cancel->setSecondaryTrigger();
	click = ui.trigger();

	setFixedSize(screenW, screenH);
	setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	setPosition(0, 0);
}

void GuiPopover::dismiss()
{
	dismissed = true;
	by = -1;
}

void GuiPopover::update(InputController * c)
{
	GuiOverlay::update(c);
	if(dismissed || !c || getState() == STATE::DISABLED)
		return;
	if(parentElement && parentElement->getState() == STATE::DISABLED)
		return;

	// B, or a click anywhere outside the panel. The popover covers the whole screen, so every pointer is always over
	// it: each controller decides for itself, whichever the screen happens to update first.
	const int chan = c->getChannel();
	bool leave = cancel->isClicked(c);
	if(!leave && chan >= 0 && click->isClicked(c))
	{
		const InputPadData& pad = c->getPadData();
		leave = pad.validPointer && (!panel || !panel->isInside(pad.cursor_x, pad.cursor_y));
	}
	if(leave)
	{
		dismissed = true;
		by = chan;
	}
}

void GuiPopover::setPanel(GuiCard& card)
{
	setPanel(card, GuiTheme::current().surfaceRaised);
}

void GuiPopover::setPanel(GuiCard& card, PixelColor tailColor)
{
	if(panel)
		return;		// once
	panel = &card;
	card.layout();		// measure it: a card that sizes itself to its content works as well as a fixed one
	const int panelW = card.getWidth(), panelH = card.getHeight();
	placed = place(anchor, panelW, panelH, screenW, screenH, insets);

	GuiLayout& at = ui.column().size(screenW, screenH).addGap((float)placed.y).add(ui.row().size(screenW, panelH).addGap((float)placed.x).add(card));
	add(&at);

	// the tail, drawn over the panel's edge in the panel's color, pointing at what the popover belongs to
	placedTail = tailAt(anchor, placed, panelW, panelH);
	if(placedTail.shown)
	{
		static const SHAPE kinds[] = { SHAPE::TRIANGLE_UP, SHAPE::TRIANGLE_DOWN, SHAPE::TRIANGLE_LEFT, SHAPE::TRIANGLE_RIGHT };
		GuiShape& arrow = ui.own<GuiShape>(placedTail.w, placedTail.h);
		arrow.setArrow(kinds[(int)placedTail.direction], tailColor, tailColor);
		GuiLayout& tailPlaced = ui.column().size(screenW, screenH).addGap((float)placedTail.y).add(ui.row().size(screenW, placedTail.h).addGap((float)placedTail.x).add(arrow));
		add(&tailPlaced);
	}
	ui.attachTo(*this);
}

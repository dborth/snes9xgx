/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * GuiOptionBrowser.cpp
 ***************************************************************************/

#include "Gui.h"

// Layout of the browser, in design pixels: one rounded card, rows inset from its edge,
// the scrollbar column down the right side. Matches GuiFileBrowser.
#define OB_PAD				6		// card edge to rows, and top/bottom of the scrollbar column
#define OB_ROW_HEIGHT		30		// the highlight pill is 2 shorter, leaving a gap between rows
#define OB_ROW_TEXT_Y		2		// text sits this far below the row's middle (see GuiFileBrowser)
#define OB_ROW_GAP			6		// rows to scrollbar column
#define OB_SB_WIDTH			22		// scrollbar column (arrow buttons)
#define OB_SB_INSET			7		// column to card's right edge
#define OB_SB_ARROW			22
#define OB_SB_TRACK_WIDTH	6
#define OB_SB_TRACK_Y		(OB_PAD + OB_SB_ARROW + 6)
#define OB_THUMB_WIDTH		14
#define OB_THUMB_MIN_HEIGHT	24
#define OB_TEXT_X			12
#define OB_TEXT_PAD			6		// room kept at a column's right end

//! The selected-row tint a little deeper, for the moment the row is clicked
static PixelColor optionRowPressColor(PixelColor tint, PixelColor accent)
{
	PixelColor c = tint;
	c.r = (uint8_t)(tint.r + (accent.r - tint.r) * 0.18f + 0.5f);
	c.g = (uint8_t)(tint.g + (accent.g - tint.g) * 0.18f + 0.5f);
	c.b = (uint8_t)(tint.b + (accent.b - tint.b) * 0.18f + 0.5f);
	return c;
}

GuiOptionBrowser::GuiOptionBrowser(int w, int h, OptionList * l)
{
	width = w;
	height = h;
	options = l;
	selectable = true;
	listOffset = this->findMenuItem(-1, 1);
	listChanged = true; // trigger an initial list update
	selectedItem = 0;
	focus = 0; // allow focus
	visibleCount = 0;
	visiblePos = 0;
	thumbHeight = 0;
	thumbTravel = 0;
	dragPos = 0;
	dragging = false;
	col1X = OB_TEXT_X;
	col2X = 400;

	const GuiTheme& theme = GuiTheme::current();
	rowWidth = w - OB_PAD - OB_ROW_GAP - OB_SB_WIDTH - OB_SB_INSET;
	trackHeight = h - 2 * OB_SB_TRACK_Y;
	const int rowTop = (h - OPTION_PAGESIZE * OB_ROW_HEIGHT) / 2;	// rows centred in the card

	trigA = new GuiTrigger;
	trigA->setPrimaryTrigger();

	trigHeldA = new GuiTrigger;
	trigHeldA->setHeldTrigger(-1, INPUT_BTN_A);

	// the card everything sits on
	cardShape = new GuiShape(w, h);
	cardShape->setParent(this);
	cardShape->setCorners(GuiCorners(theme.radius));
	cardShape->setStyle(theme.cardStyle());

	// scrollbar: a thin track with a thumb showing the position, and round arrow buttons
	// that gray at rest and take the accent color when the pointer is on them
	trackShape = new GuiShape(OB_SB_TRACK_WIDTH, trackHeight);
	trackShape->setParent(this);
	trackShape->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	trackShape->setPosition(-(OB_SB_INSET + (OB_SB_WIDTH - OB_SB_TRACK_WIDTH) / 2), OB_SB_TRACK_Y);
	trackShape->setCapsule();
	trackShape->setPaint(GuiPaint().color(theme.outline));

	// the thumb is a button so it can be grabbed; it takes the accent color when the
	// pointer is on it and a deeper one while held (as in GuiFileBrowser)
	thumbShape = new GuiShape(OB_THUMB_WIDTH, trackHeight);
	thumbShape->setCapsule();
	thumbShape->setStyle(GuiShapeStyle(GuiPaint().color(theme.outlineStrong))
		.onFocus(GuiPaint().color(theme.accent)).onPress(GuiPaint().color(theme.accentDark)));
	thumbShape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	thumbBtn = new GuiButton(OB_SB_WIDTH, trackHeight);
	thumbBtn->setParent(this);
	thumbBtn->setImage(thumbShape);
	thumbBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	thumbBtn->setPosition(-OB_SB_INSET, OB_SB_TRACK_Y);
	thumbBtn->setSelectable(false);
	thumbBtn->setClickable(false);
	thumbBtn->setHoldable(true);
	thumbBtn->setTrigger(trigHeldA);
	thumbBtn->setVisible(false);

	const GuiPaint controlRest = GuiPaint().color(theme.surfaceRaised).stroke(1.5f, theme.outline);
	const GuiPaint controlLit = GuiPaint().color(theme.accent).stroke(1.5f, theme.accent);
	const GuiPaint controlHeld = GuiPaint().color(theme.accentDark).stroke(1.5f, theme.accentDark);
	const GuiShapeStyle controlStyle = GuiShapeStyle(controlRest).onFocus(controlLit).onPress(controlHeld);

	arrowUpShape = new GuiShape(OB_SB_ARROW, OB_SB_ARROW);
	arrowUpShape->setShape(SHAPE::ELLIPSE);
	arrowUpShape->setStyle(controlStyle);
	arrowUpShape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowUpGlyph = new GuiArrow(ARROW::UP, 10, 7, theme.textMuted, theme.textOnAccent);
	arrowUpGlyph->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowUpGlyph->setPosition(0, -1);

	arrowDownShape = new GuiShape(OB_SB_ARROW, OB_SB_ARROW);
	arrowDownShape->setShape(SHAPE::ELLIPSE);
	arrowDownShape->setStyle(controlStyle);
	arrowDownShape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowDownGlyph = new GuiArrow(ARROW::DOWN, 10, 7, theme.textMuted, theme.textOnAccent);
	arrowDownGlyph->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowDownGlyph->setPosition(0, 1);

	arrowUpBtn = new GuiButton(OB_SB_WIDTH, OB_SB_ARROW);
	arrowUpBtn->setParent(this);
	arrowUpBtn->setImage(arrowUpShape);
	arrowUpBtn->setIcon(arrowUpGlyph);
	arrowUpBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	arrowUpBtn->setPosition(-OB_SB_INSET, OB_PAD);
	arrowUpBtn->setSelectable(false);
	arrowUpBtn->setTrigger(trigA);

	arrowDownBtn = new GuiButton(OB_SB_WIDTH, OB_SB_ARROW);
	arrowDownBtn->setParent(this);
	arrowDownBtn->setImage(arrowDownShape);
	arrowDownBtn->setIcon(arrowDownGlyph);
	arrowDownBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	arrowDownBtn->setPosition(-OB_SB_INSET, -OB_PAD);
	arrowDownBtn->setSelectable(false);
	arrowDownBtn->setTrigger(trigA);

	// The selected row is a rounded, tinted pill inset from the card edge, so it never
	// touches the card's corners. It fades in and out as the selection moves.
	const GuiPaint rowLit = GuiPaint().color(theme.highlight).stroke(1.5f, theme.accent);
	const GuiPaint rowHeld = GuiPaint().color(optionRowPressColor(theme.highlight, theme.accent)).stroke(1.5f, theme.accent);
	const GuiShapeStyle rowStyle = GuiShapeStyle(GuiPaint()).onFocus(rowLit).onPress(rowHeld);

	for(int i=0; i<OPTION_PAGESIZE; i++)
	{
		// names in the theme's text color, values in the accent so the two columns read apart
		optionTxt[i] = new GuiText(nullptr, 20, theme.text);
		optionTxt[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);

		optionVal[i] = new GuiText(nullptr, 20, theme.accentDark);
		optionVal[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);

		optionBg[i] = new GuiShape(rowWidth, OB_ROW_HEIGHT - 2);
		optionBg[i]->setCorners(GuiCorners(10.0f));
		optionBg[i]->setStyle(rowStyle);
		optionBg[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);

		optionBtn[i] = new GuiButton(rowWidth, OB_ROW_HEIGHT);
		optionBtn[i]->setParent(this);
		optionBtn[i]->setLabel(optionTxt[i], 0);
		optionBtn[i]->setLabel(optionVal[i], 1);
		optionBtn[i]->setImage(optionBg[i]);
		optionBtn[i]->setPosition(OB_PAD, rowTop + OB_ROW_HEIGHT * i);
		optionBtn[i]->setTrigger(trigA);
		optionBtn[i]->setSoundOver(nullptr); // rows click, but are silent on hover
	}
	updateColumns();
}

GuiOptionBrowser::~GuiOptionBrowser()
{
	delete arrowUpBtn;
	delete arrowDownBtn;
	delete thumbBtn;

	delete cardShape;
	delete trackShape;
	delete thumbShape;
	delete arrowUpShape;
	delete arrowDownShape;
	delete arrowUpGlyph;
	delete arrowDownGlyph;

	delete trigA;
	delete trigHeldA;

	for(int i=0; i<OPTION_PAGESIZE; i++)
	{
		delete optionTxt[i];
		delete optionVal[i];
		delete optionBg[i];
		delete optionBtn[i];
	}
}

//! Positions the two text columns, and limits each to the room it has: a name stops
//! short of the value column and a value stops short of the row's end (longer text
//! scrolls while its row is selected, rather than running into its neighbor).
void GuiOptionBrowser::updateColumns()
{
	for(int i=0; i<OPTION_PAGESIZE; i++)
	{
		optionTxt[i]->setPosition(col1X, OB_ROW_TEXT_Y);
		optionTxt[i]->setMaxWidth(std::max(col2X - col1X - 8, 20));
		optionVal[i]->setPosition(col2X, OB_ROW_TEXT_Y);
		optionVal[i]->setMaxWidth(std::max(rowWidth - col2X - OB_TEXT_PAD, 20));
	}
}

//! Sizes and places the scrollbar thumb for the rows currently shown
void GuiOptionBrowser::updateThumb()
{
	visibleCount = 0;
	visiblePos = 0;
	for(int i=0; i<options->length; i++)
	{
		if(strlen(options->name[i]) == 0)
			continue;
		if(i < listOffset)
			visiblePos++;
		visibleCount++;
	}

	if(visibleCount <= OPTION_PAGESIZE)
	{
		thumbBtn->setVisible(false);
		thumbTravel = 0;
		return;
	}

	thumbHeight = std::max(OB_THUMB_MIN_HEIGHT, trackHeight * OPTION_PAGESIZE / visibleCount);
	thumbTravel = trackHeight - thumbHeight;

	// while dragged the thumb follows the pointer exactly; otherwise it sits at the list position
	int y = dragging ? dragPos : thumbTravel * visiblePos / (visibleCount - OPTION_PAGESIZE);
	if(y > thumbTravel) y = thumbTravel;

	thumbBtn->setVisible(true);
	thumbBtn->setSize(OB_SB_WIDTH, thumbHeight);
	thumbShape->setSize(OB_THUMB_WIDTH, thumbHeight);
	thumbBtn->setMinY(0);
	thumbBtn->setMaxY(thumbTravel);
	thumbBtn->setPosition(-OB_SB_INSET, OB_SB_TRACK_Y + y);
}

void GuiOptionBrowser::setCol1Position(int x)
{
	col1X = x;
	updateColumns();
}

void GuiOptionBrowser::setCol2Position(int x)
{
	col2X = x;
	updateColumns();
}

void GuiOptionBrowser::setFocus(int f)
{
	focus = f;

	for(int i=0; i<OPTION_PAGESIZE; i++)
		optionBtn[i]->resetState();

	if(f == 1)
		optionBtn[selectedItem]->setState(STATE::SELECTED);
}

void GuiOptionBrowser::resetState()
{
	if(state != STATE::DISABLED)
	{
		state = STATE::DEFAULT;
		stateChan = -1;
	}

	for(int i=0; i<OPTION_PAGESIZE; i++)
	{
		optionBtn[i]->resetState();
	}
}

int GuiOptionBrowser::getClickedOption()
{
	int found = -1;
	for(int i=0; i<OPTION_PAGESIZE; i++)
	{
		if(optionBtn[i]->getState() == STATE::CLICKED)
		{
			optionBtn[i]->setState(STATE::SELECTED);
			found = optionIndex[i];
			break;
		}
	}
	return found;
}

/****************************************************************************
 * FindMenuItem
 *
 * Help function to find the next visible menu item on the list
 ***************************************************************************/

int GuiOptionBrowser::findMenuItem(int currentItem, int direction)
{
	int nextItem = currentItem + direction;

	if(nextItem < 0 || nextItem >= options->length)
		return -1;

	if(strlen(options->name[nextItem]) > 0)
		return nextItem;
	else
		return findMenuItem(nextItem, direction);
}

/**
 * Draw the button on screen
 */
void GuiOptionBrowser::draw()
{
	if(!this->isVisible())
		return;

	cardShape->draw();

	int next = listOffset;

	for(int i=0; i<OPTION_PAGESIZE; ++i)
	{
		if(next >= 0)
		{
			optionBtn[i]->draw();
			next = this->findMenuItem(next, 1);
		}
		else
			break;
	}

	trackShape->draw();
	thumbBtn->draw();
	arrowUpBtn->draw();
	arrowDownBtn->draw();

	this->updateEffects();
}

void GuiOptionBrowser::triggerUpdate()
{
	listChanged = true;
}

void GuiOptionBrowser::resetText()
{
	int next = listOffset;

	for(int i=0; i<OPTION_PAGESIZE; i++)
	{
		if(next >= 0)
		{
			optionBtn[i]->resetText();
			next = this->findMenuItem(next, 1);
		}
		else
			break;
	}
}

void GuiOptionBrowser::update(InputController * controller)
{
	if(state == STATE::DISABLED || !controller)
		return;

	int next, prev;

	arrowUpBtn->update(controller);
	arrowDownBtn->update(controller);

	auto pad = controller->getPadData();
	int currentChan = controller->getChannel();

	// drag the scrollbar thumb: the list follows the pointer
	if(thumbBtn->isVisible())
		thumbBtn->update(controller);

	const bool wasDragging = dragging;
	dragging = thumbBtn->isVisible() && thumbTravel > 0 &&
		thumbBtn->getState() == STATE::HELD &&
		thumbBtn->getStateChan() == currentChan &&
		pad.validPointer;

	if(dragging)
	{
		// put the thumb's center under the pointer
		int pos = pad.cursor_y - (this->getTop() + OB_SB_TRACK_Y + thumbHeight / 2);
		if(pos < 0) pos = 0;
		else if(pos > thumbTravel) pos = thumbTravel;
		dragPos = pos;

		// which listed option should lead the page (rows without a name are not listed)
		const int target = (pos * (visibleCount - OPTION_PAGESIZE) + thumbTravel / 2) / thumbTravel;
		int idx = this->findMenuItem(-1, 1);
		for(int n = 0; n < target && idx >= 0; n++)
		{
			int nxt = this->findMenuItem(idx, 1);
			if(nxt < 0) break;
			idx = nxt;
		}
		if(idx >= 0 && idx != listOffset)
			listOffset = idx;
		listChanged = true;
	}
	else if(wasDragging)
		listChanged = true; // snap the thumb to the list position

	next = listOffset;

	if(listChanged)
	{
		listChanged = false;
		updateThumb();
		for(int i=0; i<OPTION_PAGESIZE; ++i)
		{
			if(next >= 0)
			{
				if(optionBtn[i]->getState() == STATE::DISABLED)
				{
					optionBtn[i]->setVisible(true);
					optionBtn[i]->setState(STATE::DEFAULT);
				}

				optionTxt[i]->setText(options->name[next]);
				optionVal[i]->setText(options->value[next]);
				optionIndex[i] = next;
				next = this->findMenuItem(next, 1);
			}
			else
			{
				optionBtn[i]->setVisible(false);
				optionBtn[i]->setState(STATE::DISABLED);
			}
		}
	}

	// A InputController with no live signal this frame (e.g. a persistent
	// but currently-disconnected controller slot) must not be able to claim
	// or evict a selection just by taking its turn through this loop -- only
	// a channel that's genuinely doing something (pointing or pressing) gets
	// a say in who owns the selected slot.
	bool channelActive = pad.validPointer || pad.buttons_d != 0 || pad.buttons_h != 0;

	for(int i=0; i<OPTION_PAGESIZE; ++i)
	{
		if(i != selectedItem && optionBtn[i]->getState() == STATE::SELECTED)
			optionBtn[i]->resetState();
		else if(focus && channelActive && i == selectedItem && optionBtn[i]->getState() == STATE::DEFAULT)
			optionBtn[selectedItem]->setState(STATE::SELECTED, currentChan);
		else if(focus && channelActive && i == selectedItem && optionBtn[i]->getState() == STATE::SELECTED &&
			optionBtn[i]->getStateChan() != -1 && optionBtn[i]->getStateChan() != currentChan)
			// Slot is already SELECTED but carries a stale channel from an earlier
			// selection (e.g. a reused slot after paging, or a preselected item that
			// was never actually hovered by the real channel yet). Without this,
			// currentChan == stateChan never holds and clicks are silently ignored
			// until the item happens to be navigated away from and back.
			optionBtn[i]->resetState();

		// Present a "no channel" (-1) identity to any item the cursor isn't
		// currently over. Without this, a stale stateChan left on a reused
		// list slot (e.g. after paging/navigating) can permanently block
		// clicks from the real channel until the item is re-hovered.
		if(pad.validPointer && !optionBtn[i]->isInside(pad.cursor_x, pad.cursor_y))
			controller->setChannel(-1);

		optionBtn[i]->update(controller);
		controller->setChannel(currentChan);

		if(optionBtn[i]->getState() == STATE::SELECTED)
			selectedItem = i;

		if(selectedItem == i) {
			optionTxt[i]->setScroll(SCROLL::HORIZONTAL);
			optionVal[i]->setScroll(SCROLL::HORIZONTAL);
		}
		else
		{
			optionTxt[i]->setScroll(SCROLL::NONE);
			optionVal[i]->setScroll(SCROLL::NONE);
		}
	}

	// pad/joystick navigation
	if(!focus)
		return; // skip navigation

	if(controller->down() || arrowDownBtn->getState() == STATE::CLICKED)
	{
		next = this->findMenuItem(optionIndex[selectedItem], 1);

		if(next >= 0)
		{
			if(selectedItem == OPTION_PAGESIZE-1)
			{
				// move list down by 1
				listOffset = this->findMenuItem(listOffset, 1);
				listChanged = true;
			}
			else if(optionBtn[selectedItem+1]->isVisible())
			{
				optionBtn[selectedItem]->resetState();
				optionBtn[selectedItem+1]->setState(STATE::SELECTED, controller->getChannel());
				++selectedItem;
			}
		}
		arrowDownBtn->resetState();
	}
	else if(controller->up() || arrowUpBtn->getState() == STATE::CLICKED)
	{
		prev = this->findMenuItem(optionIndex[selectedItem], -1);

		if(prev >= 0)
		{
			if(selectedItem == 0)
			{
				// move list up by 1
				listOffset = prev;
				listChanged = true;
			}
			else
			{
				optionBtn[selectedItem]->resetState();
				optionBtn[selectedItem-1]->setState(STATE::SELECTED, controller->getChannel());
				--selectedItem;
			}
		}
		arrowUpBtn->resetState();
	}

	if(updateCB)
		updateCB(this);
}

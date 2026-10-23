/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiSaveBrowser.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "Gui.h"

// Layout, in design pixels. Each slot is a card; the scrollbar column runs down the
// right side with round arrow buttons and a thumb showing which rows are on screen.
#define SLOT_WIDTH			244
#define SLOT_HEIGHT			72
#define SLOT_PITCH_X		257
#define SLOT_PITCH_Y		84
#define PREVIEW_WIDTH		64		// screenshot box - matches the 64x48 the screenshots are decoded to
#define PREVIEW_HEIGHT		48
#define PREVIEW_X			8		// box to card's left edge, equal to the gap before the text (at x=80)
#define PLACEHOLDER_WIDTH	52		// the "No image" placeholder is smaller, centered in the box
#define PLACEHOLDER_HEIGHT	44
#define SB_WIDTH			22		// scrollbar column (arrow buttons)
#define SB_INSET			12		// column to the browser's right edge
#define SB_ARROW			22
#define SB_TRACK_WIDTH		6
#define SB_TRACK_Y			(SB_ARROW + 4)
#define SB_THUMB_WIDTH		14
#define SB_THUMB_MIN_HEIGHT	24
#define TAG_WIDTH			92		// the "Playing from" tag
#define TAG_HEIGHT			20

GuiSavePreview::GuiSavePreview(int w, int h, int phW, int phH)
	: placeholder(phW, phH), noImage("No image", 12, GuiTheme::current().textMuted), hasImage(false)
{
	width = w;
	height = h;
	const GuiTheme& theme = GuiTheme::current();

	placeholder.setParent(this);
	placeholder.setCorners(GuiCorners(8.0f));
	placeholder.setPaint(GuiPaint().gradient(theme.surface, theme.outline).stroke(1.0f, theme.outline));
	placeholder.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	noImage.setParent(this);
	noImage.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	image.setParent(this);
	image.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
}

void GuiSavePreview::setImage(GuiImageData * img)
{
	image.setImage(img);
	hasImage = img != nullptr && img->getTexture() != nullptr;
}

void GuiSavePreview::setPlaceholderText(const char * text)
{
	noImage.setText(text);
}

void GuiSavePreview::draw()
{
	if(!this->isVisible())
		return;

	if(hasImage)
		image.draw();
	else
	{
		placeholder.draw();
		noImage.draw();
	}

	this->updateEffects();
}

/**
 * Constructor for the GuiSaveBrowser class.
 */
GuiSaveBrowser::GuiSaveBrowser(int w, int h)
{
	width = w;
	height = h;
	selectable = true;

	listOffset = 0;
	selectedItem = 0;
	focus = 0; // allow focus

	trigA = new GuiTrigger;
	trigA->setPrimaryTrigger();

	const GuiTheme& theme = GuiTheme::current();
	trackHeight = h - 2 * SB_TRACK_Y;

	// scrollbar: a thin track with a thumb, and round arrow buttons that gray at rest
	// and take the accent color when the pointer is on them
	trackShape = new GuiShape(SB_TRACK_WIDTH, trackHeight);
	trackShape->setParent(this);
	trackShape->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	trackShape->setPosition(-(SB_INSET + (SB_WIDTH - SB_TRACK_WIDTH) / 2), SB_TRACK_Y);
	trackShape->setCapsule();
	trackShape->setPaint(GuiPaint().color(theme.outline));

	thumbShape = new GuiShape(SB_THUMB_WIDTH, trackHeight);
	thumbShape->setParent(this);
	thumbShape->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	thumbShape->setCapsule();
	thumbShape->setPaint(GuiPaint().color(theme.outlineStrong));
	thumbShape->setVisible(false);

	const GuiPaint controlRest = GuiPaint().color(theme.surfaceRaised).stroke(1.5f, theme.outline);
	const GuiPaint controlLit = GuiPaint().color(theme.accent).stroke(1.5f, theme.accent);
	const GuiPaint controlHeld = GuiPaint().color(theme.accentDark).stroke(1.5f, theme.accentDark);
	const GuiShapeStyle controlStyle = GuiShapeStyle(controlRest).onFocus(controlLit).onPress(controlHeld);

	arrowUpShape = new GuiShape(SB_ARROW, SB_ARROW);
	arrowUpShape->setShape(SHAPE::ELLIPSE);
	arrowUpShape->setStyle(controlStyle);
	arrowUpShape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowUpGlyph = new GuiShape(10, 7);
	arrowUpGlyph->setArrow(SHAPE::TRIANGLE_UP, theme.textMuted, theme.textOnAccent);
	arrowUpGlyph->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowUpGlyph->setPosition(0, -1);

	arrowDownShape = new GuiShape(SB_ARROW, SB_ARROW);
	arrowDownShape->setShape(SHAPE::ELLIPSE);
	arrowDownShape->setStyle(controlStyle);
	arrowDownShape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowDownGlyph = new GuiShape(10, 7);
	arrowDownGlyph->setArrow(SHAPE::TRIANGLE_DOWN, theme.textMuted, theme.textOnAccent);
	arrowDownGlyph->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowDownGlyph->setPosition(0, 1);

	arrowUpBtn = new GuiButton(SB_WIDTH, SB_ARROW);
	arrowUpBtn->setParent(this);
	arrowUpBtn->setImage(arrowUpShape);
	arrowUpBtn->setIcon(arrowUpGlyph);
	arrowUpBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	arrowUpBtn->setPosition(-SB_INSET, 0);
	arrowUpBtn->setSelectable(false);
	arrowUpBtn->setTrigger(trigA);

	arrowDownBtn = new GuiButton(SB_WIDTH, SB_ARROW);
	arrowDownBtn->setParent(this);
	arrowDownBtn->setImage(arrowDownShape);
	arrowDownBtn->setIcon(arrowDownGlyph);
	arrowDownBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	arrowDownBtn->setPosition(-SB_INSET, 0);
	arrowDownBtn->setSelectable(false);
	arrowDownBtn->setTrigger(trigA);

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		saveTitle[i] = new GuiText(nullptr, 18, theme.text);
		saveTitle[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		saveTitle[i]->setPosition(80,12);
		saveSubtitle[i] = new GuiText(nullptr, 14, theme.textMuted);
		saveSubtitle[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		saveSubtitle[i]->setPosition(80,40);

		// each slot is a card, taking the accent outline and glow when highlighted
		saveBgImg[i] = new GuiShape(SLOT_WIDTH, SLOT_HEIGHT);
		saveBgImg[i]->setCorners(GuiCorners(theme.radius));
		saveBgImg[i]->setStyle(theme.cardStyle());
		savePreviewImg[i] = new GuiSavePreview(PREVIEW_WIDTH, PREVIEW_HEIGHT, PLACEHOLDER_WIDTH, PLACEHOLDER_HEIGHT);
		savePreviewImg[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		savePreviewImg[i]->setPosition(PREVIEW_X,0);

		saveBtn[i] = new GuiButton(SLOT_WIDTH, SLOT_HEIGHT);
		saveBtn[i]->setParent(this);
		saveBtn[i]->setLabel(saveTitle[i], 0);
		saveBtn[i]->setLabel(saveSubtitle[i], 1);
		saveBtn[i]->setImage(saveBgImg[i]);
		saveBtn[i]->setIcon(savePreviewImg[i]);
		saveBtn[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		saveBtn[i]->setPosition(SLOT_PITCH_X*(i % 2),SLOT_PITCH_Y*(i>>1));
		saveBtn[i]->setTrigger(trigA);
		saveBtn[i]->setState(STATE::DISABLED);
		saveBtn[i]->setEffectGrow(106);
		saveBtn[i]->setVisible(false);
		saveBtnLastOver[i] = false;

		// the tag sits half over the card's top edge, near its right end
		tagShape[i] = new GuiShape(TAG_WIDTH, TAG_HEIGHT);
		tagShape[i]->setParent(saveBtn[i]);
		tagShape[i]->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
		tagShape[i]->setPosition(-12, -TAG_HEIGHT / 2 + 1);
		tagShape[i]->setCapsule();
		tagShape[i]->setPaint(GuiPaint().color(theme.accent));
		tagText[i] = new GuiText("Playing from", 12, theme.textOnAccent);
		tagText[i]->setParent(tagShape[i]);
		tagText[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	}
}

/**
 * Destructor for the GuiSaveBrowser class.
 */
GuiSaveBrowser::~GuiSaveBrowser()
{
	delete arrowUpBtn;
	delete arrowDownBtn;

	delete trackShape;
	delete thumbShape;
	delete arrowUpShape;
	delete arrowDownShape;
	delete arrowUpGlyph;
	delete arrowDownGlyph;

	delete trigA;

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		delete tagText[i];
		delete tagShape[i];
		delete saveBtn[i];
		delete saveTitle[i];
		delete saveSubtitle[i];
		delete saveBgImg[i];
		delete savePreviewImg[i];
	}
}

void GuiSaveBrowser::setEntries(const std::vector<SaveEntry>& list, int select)
{
	entries = list;
	const int n = (int)entries.size();

	if(select < 0 || select >= n)
		select = 0;

	// scroll so the card with the cursor is on the page: the first page, or with it on the last row
	if(select < SAVELISTSIZE)
		listOffset = 0;
	else
		listOffset = ((select / 2) - (SAVELISTSIZE / 2 - 1)) * 2;
	selectedItem = select - listOffset;

	// the cards show what they are given on the next update()
	for(int i=0; i<SAVELISTSIZE; i++)
	{
		saveBtn[i]->resetState();
		saveBtn[i]->setVisible(false);
		saveBtn[i]->setState(STATE::DISABLED);
		savePreviewImg[i]->setImage(nullptr);
		saveBtnLastOver[i] = false;
	}
}

int GuiSaveBrowser::getSelectedPosition() const
{
	if(entries.empty())
		return -1;
	const int p = listOffset + selectedItem;
	return (p >= 0 && p < (int)entries.size()) ? p : -1;
}

GuiButton * GuiSaveBrowser::getEntryButton(int position)
{
	const int i = position - listOffset;
	if(i < 0 || i >= SAVELISTSIZE || position >= (int)entries.size())
		return nullptr;
	return saveBtn[i];
}

const SaveEntry * GuiSaveBrowser::getEntry(int position) const
{
	return (position >= 0 && position < (int)entries.size()) ? &entries[(size_t)position] : nullptr;
}

void GuiSaveBrowser::setFocus(int f)
{
	focus = f;

	for(int i=0; i<SAVELISTSIZE; i++)
		saveBtn[i]->resetState();

	if(f == 1 && selectedItem >= 0)
		saveBtn[selectedItem]->setState(STATE::SELECTED);
}

void GuiSaveBrowser::resetState()
{
	if(state != STATE::DISABLED)
	{
		state = STATE::DEFAULT;
		stateChan = -1;
	}

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		saveBtn[i]->resetState();
	}
}

int GuiSaveBrowser::getClickedSave()
{
	int found = -1;
	for(int i=0; i<SAVELISTSIZE; i++)
	{
		if(saveBtn[i]->getState() == STATE::CLICKED)
		{
			saveBtn[i]->setState(STATE::SELECTED);
			found = listOffset+i;
			break;
		}
	}
	return found;
}

/**
 * Draw the button on screen
 */
void GuiSaveBrowser::draw()
{
	if(!this->isVisible())
		return;

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		saveBtn[i]->draw();

		// the tag goes over the card, so after it
		const int pos = listOffset + i;
		if(saveBtn[i]->isVisible() && pos < (int)entries.size() && entries[(size_t)pos].playing)
		{
			tagShape[i]->draw();
			tagText[i]->draw();
		}
	}

	trackShape->draw();
	thumbShape->draw();
	arrowUpBtn->draw();
	arrowDownBtn->draw();

	this->updateEffects();
}

void GuiSaveBrowser::update(InputController * controller)
{
	if(state == STATE::DISABLED || !controller)
		return;

	int i;
	const int length = (int)entries.size();

	arrowUpBtn->update(controller);
	arrowDownBtn->update(controller);

	// pad/joystick navigation
	if(!focus)
		goto endNavigation; // skip navigation

	if(selectedItem < 0) selectedItem = 0;

	if(controller->right())
	{
		if(selectedItem == SAVELISTSIZE-1)
		{
			if(listOffset + SAVELISTSIZE < length)
			{
				// move list down by 1 row
				listOffset += 2;
				selectedItem -= 1;
			}
		}
		else if(saveBtn[selectedItem+1]->isVisible())
		{
			saveBtn[selectedItem]->resetState();
			saveBtn[selectedItem+1]->setState(STATE::SELECTED, controller->getChannel());
			selectedItem += 1;
		}
	}
	else if(controller->left())
	{
		if(selectedItem == 0)
		{
			if(listOffset - 2 >= 0)
			{
				// move list up by 1
				listOffset -= 2;
				selectedItem = SAVELISTSIZE-1;
			}
		}
		else
		{
			if(saveBtn[selectedItem-1]->isVisible())
			{
				selectedItem -= 1;
			}
		}
	}
	else if(controller->down() || arrowDownBtn->getState() == STATE::CLICKED)
	{
		if(selectedItem >= SAVELISTSIZE-2)
		{
			if(listOffset + SAVELISTSIZE + 1 < length)
			{
				listOffset += 2;
			}
			else if(listOffset + SAVELISTSIZE < length)
			{
				listOffset += 2;

				if(selectedItem == SAVELISTSIZE-1)
					selectedItem -= 1;
			}
		}
		else if(saveBtn[selectedItem+2]->isVisible())
		{
			selectedItem += 2;
		}
	}
	else if(controller->up() || arrowUpBtn->getState() == STATE::CLICKED)
	{
		if(selectedItem < 2)
		{
			if(listOffset - 2 >= 0)
			{
				// move list up by 1
				listOffset -= 2;
			}
		}
		else
		{
			if(saveBtn[selectedItem-2]->isVisible())
			{
				selectedItem -= 2;
			}
		}
	}

	endNavigation:

	if(arrowDownBtn->getState() == STATE::CLICKED)
		arrowDownBtn->resetState();

	if(arrowUpBtn->getState() == STATE::CLICKED)
		arrowUpBtn->resetState();

	for(i=0; i<SAVELISTSIZE; i++)
	{
		if(listOffset+i < length)
		{
			const SaveEntry & e = entries[(size_t)(listOffset+i)];

			if(saveBtn[i]->getState() == STATE::DISABLED || !saveBtn[i]->isVisible())
			{
				saveBtn[i]->setVisible(true);
				saveBtn[i]->setState(STATE::DEFAULT);
			}

			saveTitle[i]->setText(e.title.c_str());
			saveSubtitle[i]->setText(e.subtitle.c_str());
			savePreviewImg[i]->setPlaceholderText(e.placeholder.empty() ? "No image" : e.placeholder.c_str());

			if(e.preview != nullptr && e.preview->getTexture() != nullptr)
				savePreviewImg[i]->setImage(e.preview);
			else
				savePreviewImg[i]->setImage(nullptr);
		}
		else
		{
			saveBtn[i]->setVisible(false);
			saveBtn[i]->setState(STATE::DISABLED);
		}

		if(i != selectedItem && saveBtn[i]->getState() == STATE::SELECTED)
			saveBtn[i]->resetState();
		else if(focus && i == selectedItem && saveBtn[i]->getState() == STATE::DEFAULT)
			saveBtn[selectedItem]->setState(STATE::SELECTED, controller->getChannel());

		auto pad = controller->getPadData();

		if(pad.validPointer)
		{
			if(!saveBtnLastOver[i] && saveBtn[i]->isInside(pad.cursor_x, pad.cursor_y))
				saveBtn[i]->resetState();
			saveBtnLastOver[i] = saveBtn[i]->isInside(pad.cursor_x, pad.cursor_y);
		}

		saveBtn[i]->update(controller);

		if(saveBtn[i]->getState() == STATE::SELECTED)
			selectedItem = i;
	}

	updateThumb();

	if(updateCB)
		updateCB(this);
}

//! Sizes and places the scrollbar thumb for the rows currently shown
void GuiSaveBrowser::updateThumb()
{
	const int pageRows = SAVELISTSIZE / 2;
	const int totalRows = ((int)entries.size() + 1) / 2;
	int firstRow = listOffset / 2;

	if(totalRows <= pageRows)
	{
		thumbShape->setVisible(false);
		return;
	}
	if(firstRow < 0) firstRow = 0;
	if(firstRow > totalRows - pageRows) firstRow = totalRows - pageRows;

	const int thumbHeight = std::max(SB_THUMB_MIN_HEIGHT, trackHeight * pageRows / totalRows);
	const int travel = trackHeight - thumbHeight;

	thumbShape->setVisible(true);
	thumbShape->setSize(SB_THUMB_WIDTH, thumbHeight);
	thumbShape->setPosition(-(SB_INSET + (SB_WIDTH - SB_THUMB_WIDTH) / 2), SB_TRACK_Y + travel * firstRow / (totalRows - pageRows));
}

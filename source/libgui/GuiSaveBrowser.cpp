/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiSaveBrowser.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "../filebrowser.h"
#include "Gui.h"

// Layout, in design pixels. Each slot is a card; the scrollbar column runs down the
// right side with round arrow buttons and a thumb showing which rows are on screen.
#define SLOT_WIDTH			244
#define SLOT_HEIGHT			72
#define SLOT_PITCH_X		257
#define SLOT_PITCH_Y		87
#define PREVIEW_WIDTH		52
#define PREVIEW_HEIGHT		44
#define SB_WIDTH			22		// scrollbar column (arrow buttons)
#define SB_INSET			12		// column to the browser's right edge
#define SB_ARROW			22
#define SB_TRACK_WIDTH		6
#define SB_TRACK_Y			(SB_ARROW + 6)
#define SB_THUMB_WIDTH		14
#define SB_THUMB_MIN_HEIGHT	24

GuiSavePreview::GuiSavePreview(int w, int h)
	: placeholder(w, h), noImage("No image", 12, GuiTheme::current().textMuted), hasImage(false)
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
GuiSaveBrowser::GuiSaveBrowser(int w, int h, SaveList * s, int a)
{
	width = w;
	height = h;
	saves = s;
	action = a;
	selectable = true;

	if(action == 0) // load
		listOffset = 0;
	else if(action == 2) // delete SRAM / State
		listOffset = 0;
	else
		listOffset = -2; // save - reserve -2 & -1 for new slots

	selectedItem = 0;
	focus = 0; // allow focus

	trigA = new GuiTrigger;
	trigA->setPrimaryTrigger();

	btnSoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	btnSoundClick = new GuiSound(button_click_pcm, button_click_pcm_size, SOUND::PCM);

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
	arrowUpGlyph = new GuiArrow(ARROW::UP, 10, 7, theme.textMuted, theme.textOnAccent);
	arrowUpGlyph->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowUpGlyph->setPosition(0, -1);

	arrowDownShape = new GuiShape(SB_ARROW, SB_ARROW);
	arrowDownShape->setShape(SHAPE::ELLIPSE);
	arrowDownShape->setStyle(controlStyle);
	arrowDownShape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	arrowDownGlyph = new GuiArrow(ARROW::DOWN, 10, 7, theme.textMuted, theme.textOnAccent);
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
	arrowUpBtn->setSoundOver(btnSoundOver);
	arrowUpBtn->setSoundClick(btnSoundClick);

	arrowDownBtn = new GuiButton(SB_WIDTH, SB_ARROW);
	arrowDownBtn->setParent(this);
	arrowDownBtn->setImage(arrowDownShape);
	arrowDownBtn->setIcon(arrowDownGlyph);
	arrowDownBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	arrowDownBtn->setPosition(-SB_INSET, 0);
	arrowDownBtn->setSelectable(false);
	arrowDownBtn->setTrigger(trigA);
	arrowDownBtn->setSoundOver(btnSoundOver);
	arrowDownBtn->setSoundClick(btnSoundClick);

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		saveDate[i] = new GuiText(nullptr, 18, theme.text);
		saveDate[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		saveDate[i]->setPosition(80,5);
		saveTime[i] = new GuiText(nullptr, 18, theme.text);
		saveTime[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		saveTime[i]->setPosition(80,27);

		saveType[i] = new GuiText(nullptr, 18, theme.text);
		saveType[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		saveType[i]->setPosition(80,50);

		// each slot is a card, taking the accent outline and glow when highlighted
		saveBgImg[i] = new GuiShape(SLOT_WIDTH, SLOT_HEIGHT);
		saveBgImg[i]->setCorners(GuiCorners(theme.radius));
		saveBgImg[i]->setStyle(theme.cardStyle());
		savePreviewImg[i] = new GuiSavePreview(PREVIEW_WIDTH, PREVIEW_HEIGHT);
		savePreviewImg[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		savePreviewImg[i]->setPosition(5,0);

		saveBtn[i] = new GuiButton(SLOT_WIDTH, SLOT_HEIGHT);
		saveBtn[i]->setParent(this);
		saveBtn[i]->setLabel(saveDate[i], 0);
		saveBtn[i]->setLabel(saveTime[i], 1);
		saveBtn[i]->setLabel(saveType[i], 2);
		saveBtn[i]->setImage(saveBgImg[i]);
		saveBtn[i]->setIcon(savePreviewImg[i]);
		saveBtn[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		saveBtn[i]->setPosition(SLOT_PITCH_X*(i % 2),SLOT_PITCH_Y*(i>>1));
		saveBtn[i]->setTrigger(trigA);
		saveBtn[i]->setState(STATE::DISABLED);
		saveBtn[i]->setEffectGrow(106);
		saveBtn[i]->setVisible(false);
		saveBtn[i]->setSoundOver(btnSoundOver);
		saveBtn[i]->setSoundClick(btnSoundClick);
		saveBtnLastOver[i] = false;
	}
	saveBtn[0]->setState(STATE::SELECTED, -1);
	saveBtn[0]->setVisible(true);
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

	delete btnSoundOver;
	delete btnSoundClick;
	delete trigA;

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		delete saveBtn[i];
		delete saveDate[i];
		delete saveTime[i];
		delete saveType[i];
		delete saveBgImg[i];
		delete savePreviewImg[i];
	}
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
	int found = -3;
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
		saveBtn[i]->draw();

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

	int i, len;
	char savetext[50];

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
			if(listOffset + SAVELISTSIZE < saves->length)
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
			if((listOffset - 2 >= 0 && action == 0) ||
				(listOffset >= 0 && action == 1) ||
				(listOffset - 2 >= 0 && action == 2))
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
			if(listOffset + SAVELISTSIZE + 1 < saves->length)
			{
				listOffset += 2;
			}
			else if(listOffset + SAVELISTSIZE < saves->length)
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
			if((listOffset - 2 >= 0 && action == 0) ||
				(listOffset >= 0 && action == 1) ||
				(listOffset - 2 >= 0 && action == 2))
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
		if(listOffset+i < 0 && action == 1)
		{
			saveDate[0]->setText(nullptr);
			saveTime[0]->setText("New");
			saveType[0]->setText("State");
			savePreviewImg[0]->setImage(nullptr);
			saveBtn[0]->setVisible(true);

			if(saveBtn[0]->getState() == STATE::DISABLED)
				saveBtn[0]->setState(STATE::DEFAULT);

			if (EmuSettings.hideSramSaving == 0)
			{
				saveDate[1]->setText(nullptr);
				saveTime[1]->setText("New");
				saveType[1]->setText("SRAM");
				savePreviewImg[1]->setImage(nullptr);
				saveBtn[1]->setVisible(true);

				if(saveBtn[1]->getState() == STATE::DISABLED)
					saveBtn[1]->setState(STATE::DEFAULT);
			}
		}
		else if(listOffset+i < saves->length)
		{
			if(saveBtn[i]->getState() == STATE::DISABLED || !saveBtn[i]->isVisible())
			{
				saveBtn[i]->setVisible(true);
				saveBtn[i]->setState(STATE::DEFAULT);
			}

			saveDate[i]->setText(saves->date[listOffset+i]);
			saveTime[i]->setText(saves->time[listOffset+i]);

			if(saves->type[listOffset+i] == FILE_SRAM)
				sprintf(savetext, "SRAM");
			else
				sprintf(savetext, "State");

			len = strlen(saves->filename[listOffset+i]);
			if(len > 10 &&
				((saves->filename[listOffset+i][len-8] == 'A' &&
				saves->filename[listOffset+i][len-7] == 'u' &&
				saves->filename[listOffset+i][len-6] == 't' &&
				saves->filename[listOffset+i][len-5] == 'o') ||
				saves->filename[listOffset+i][len-5] == '0')
				)
			{
				strcat(savetext, " (Auto)");
			}
			saveType[i]->setText(savetext);

			if(saves->previewImg[listOffset+i] != nullptr && saves->previewImg[listOffset+i]->getTexture() != nullptr)
				savePreviewImg[i]->setImage(saves->previewImg[listOffset+i].get());
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
	// saving reserves two "new" slots (state and RAM) before the existing saves
	const int reserved = (action == 1) ? 2 : 0;
	const int pageRows = SAVELISTSIZE / 2;
	const int totalRows = (saves->length + reserved + 1) / 2;
	int firstRow = (listOffset + reserved) / 2;

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

/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 *
 * GuiSaveBrowser.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "libgui/Gui.h"
#include "GuiSaveBrowser.h"

// Layout, in design pixels. Each slot is a card; the scrollbar column runs down the
// right side with round arrow buttons and a thumb showing which rows are on screen.
#define SLOT_WIDTH			250
#define SLOT_HEIGHT			72
#define SLOT_PITCH_X		258
#define SLOT_PITCH_Y		80
#define SLOT_GAP_Y			(SLOT_PITCH_Y - SLOT_HEIGHT)
// How much a card grows when highlighted. Both neighbors can be grown at once, so each may take half the gap
// between cards on its side: a card must not reach into the next one.
#define GRID_GROW			(100 + (SLOT_PITCH_X - SLOT_WIDTH) * 100 / SLOT_WIDTH)
#define PINNED_GROW			102
#define SLOT_WIDE_WIDTH		(SLOT_PITCH_X + SLOT_WIDTH)	// the pinned card spans both columns
#define PREVIEW_WIDTH		64		// screenshot box - matches the 64x48 the screenshots are decoded to
#define PREVIEW_HEIGHT		48
#define PREVIEW_X			8		// box to card's left edge, equal to the gap before the text (at x=80)
#define PLACEHOLDER_WIDTH	52		// the "No image" placeholder is smaller, centered in the box
#define PLACEHOLDER_HEIGHT	44
#define PINNED_HEIGHT		62		// the pinned card is a little shorter than the grid's
#define PINNED_GAP			17		// grid's last row to the pinned card
#define PINNED_ICON_X		16
#define PINNED_TEXT_X		76
#define CHIP_ON_WIDTH		96		// the label at the pinned card's right end: "Autosave", with its dot
#define CHIP_OFF_WIDTH		140		// "Saves when you do"
#define CHIP_HEIGHT			22
#define CHIP_INSET			16
#define ICON_WIDTH			44		// the save-file icon
#define ICON_HEIGHT			48
#define SB_WIDTH			22		// scrollbar column (arrow buttons)
#define SB_INSET			0		// column to the browser's right edge
#define SB_ARROW			22
#define SB_TRACK_WIDTH		6
#define SB_TRACK_Y			(SB_ARROW + 4)
#define SB_THUMB_WIDTH		14
#define SB_THUMB_MIN_HEIGHT	24
#define TAG_PLAYING_WIDTH	92		// the "Playing from" tag
#define TAG_AUTO_WIDTH		84		// "Autosave", with a dot
#define TAG_PAUSED_WIDTH	58
#define BADGE_AUTO_WIDTH	40		// AUTO, a pill on the screenshot's corner
#define BADGE_HEIGHT		16
#define BADGE_NUM_SIZE		18		// a slot number, in a circle
#define BADGE_INSET			3
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

static PixelColor IconRGB(int r, int g, int b)
{
	PixelColor c = { (uint8_t)r, (uint8_t)g, (uint8_t)b, 255 };
	return c;
}

static PixelColor Rgba(int r, int g, int b, int a)
{
	PixelColor c = { (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a };
	return c;
}

// What is information, not a card to choose (the "Cartridge Save" note, an Auto Snapshot not made yet): the same dark
// translucent card as the screen's "No snapshots yet", whatever is behind it
static GuiShapeStyle SaveNoteStyle()
{
	GuiPaint note = GuiPaint().color(Rgba(0, 0, 0, 150)).stroke(1.5f, Rgba(255, 255, 255, 90));
	return GuiShapeStyle(note).onDisable(note);
}

// A card of the browser: the theme's, with the focus made plain to see - a thick accent outline and a strong glow,
// on top of the card growing - because the theme's thin outline is easy to lose on a TV.
static GuiShapeStyle SaveCardStyle()
{
	const GuiTheme& theme = GuiTheme::current();
	PixelColor glowColor = theme.accent;
	glowColor.a = 210;
	PixelColor heldGlow = theme.accent;
	heldGlow.a = 140;
	PixelColor offFill = theme.surface;
	offFill.a = 140;
	PixelColor offEdge = theme.outline;
	offEdge.a = 140;

	GuiPaint rest = GuiPaint()
		.gradient(theme.surfaceRaised, theme.surface)
		.stroke(theme.strokeWidth, theme.outline)
		.shadow(theme.shadowBlur, theme.shadow, 0.0f, 4.0f);
	GuiPaint lit = GuiPaint()
		.gradient(theme.surfaceRaised, theme.surfaceRaised)
		.stroke(4.0f, theme.accent)
		.glow(theme.shadowBlur + 6.0f, glowColor, 3.0f);
	GuiPaint pressed = GuiPaint()
		.gradient(theme.surface, theme.surfaceRaised)
		.stroke(4.0f, theme.accentDark)
		.glow(theme.shadowBlur + 2.0f, heldGlow, 2.0f);
	GuiPaint off = GuiPaint()
		.color(offFill)
		.stroke(theme.strokeWidth, offEdge);
	return GuiShapeStyle(rest).onFocus(lit).onPress(pressed).onDisable(off);
}

GuiSaveFileIcon::GuiSaveFileIcon()
	: body(ICON_WIDTH, ICON_HEIGHT), label(32, 20), battery(14, 8), nub(2, 4), slot(28, 9), noBattery(false)
{
	width = ICON_WIDTH;
	height = ICON_HEIGHT;

	GuiShape * parts[5] = { &body, &label, &battery, &nub, &slot };
	for(int i = 0; i < 5; i++)
	{
		parts[i]->setParent(this);
		parts[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	}

	body.setCorners(GuiCorners(7.0f));
	body.setPaint(GuiPaint().gradient(IconRGB(122, 134, 152), IconRGB(90, 102, 122)).stroke(1.5f, IconRGB(52, 62, 80)));

	label.setPosition(6, 6);
	label.setCorners(GuiCorners(4.0f));
	label.setPaint(GuiPaint().gradient(IconRGB(242, 245, 248), IconRGB(222, 228, 236)).stroke(1.0f, IconRGB(70, 80, 100)));

	// the battery, centered in the label: the cell and the nub at its end
	battery.setPosition(14, 12);
	battery.setCorners(GuiCorners(2.0f));
	battery.setPaint(GuiPaint().gradient(IconRGB(78, 196, 102), IconRGB(40, 150, 70)).stroke(1.0f, IconRGB(26, 100, 48)));
	nub.setPosition(28, 14);
	nub.setCorners(GuiCorners(1.0f));
	nub.setPaint(GuiPaint().color(IconRGB(40, 150, 70)));

	slot.setPosition(8, 34);
	slot.setCorners(GuiCorners(3.0f));
	slot.setPaint(GuiPaint().color(IconRGB(58, 68, 86)));
}

void GuiSaveFileIcon::setNoBattery(bool none)
{
	if(none == noBattery)
		return;
	noBattery = none;

	// an empty battery is a gray outline: nothing in it
	if(none)
	{
		battery.setPaint(GuiPaint().color(IconRGB(222, 228, 236)).stroke(1.5f, IconRGB(150, 158, 170)));
		nub.setPaint(GuiPaint().color(IconRGB(150, 158, 170)));
	}
	else
	{
		battery.setPaint(GuiPaint().gradient(IconRGB(78, 196, 102), IconRGB(40, 150, 70)).stroke(1.0f, IconRGB(26, 100, 48)));
		nub.setPaint(GuiPaint().color(IconRGB(40, 150, 70)));
	}
}

void GuiSaveFileIcon::draw()
{
	if(!this->isVisible())
		return;

	body.draw();
	label.draw();
	battery.draw();
	nub.draw();
	slot.draw();

	this->updateEffects();
}

/**
 * Constructor for the GuiSaveBrowser class.
 */
GuiSaveBrowser::GuiSaveBrowser(int w, int r)
{
	rows = (r < 1) ? 1 : (r > SAVELISTSIZE / COLS ? SAVELISTSIZE / COLS : r);
	width = w;
	height = rows * SLOT_PITCH_Y - SLOT_GAP_Y;
	selectable = true;

	listOffset = 0;
	selectedItem = 0;
	pinnedOn = false;
	pinnedLastOver = false;
	scrollable = false;
	thumbHeight = 0;
	thumbTravel = 0;
	dragPos = 0;
	dragging = false;
	focus = 0; // allow focus

	trigA = new GuiTrigger;
	trigA->setPrimaryTrigger();
	trigHeldA = new GuiTrigger;
	trigHeldA->setHeldTrigger(-1, INPUT_BTN_A);

	const GuiTheme& theme = GuiTheme::current();
	trackHeight = height - 2 * SB_TRACK_Y;

	// scrollbar: a thin track with a thumb, and round arrow buttons that gray at rest
	// and take the accent color when the pointer is on them
	trackShape = new GuiShape(SB_TRACK_WIDTH, trackHeight);
	trackShape->setParent(this);
	trackShape->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	trackShape->setPosition(-(SB_INSET + (SB_WIDTH - SB_TRACK_WIDTH) / 2), SB_TRACK_Y);
	trackShape->setCapsule();
	trackShape->setPaint(GuiPaint().color(theme.outline));

	// the thumb is a button so it can be grabbed; it takes the accent color when the pointer is on it and a
	// deeper one while held (as in GuiOptionBrowser and GuiGameBrowser)
	thumbShape = new GuiShape(SB_THUMB_WIDTH, trackHeight);
	thumbShape->setCapsule();
	thumbShape->setStyle(GuiShapeStyle(GuiPaint().color(theme.outlineStrong))
		.onFocus(GuiPaint().color(theme.accent)).onPress(GuiPaint().color(theme.accentDark)));
	thumbShape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	thumbBtn = new GuiButton(SB_WIDTH, trackHeight);
	thumbBtn->setParent(this);
	thumbBtn->setImage(thumbShape);
	thumbBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	thumbBtn->setPosition(-SB_INSET, SB_TRACK_Y);
	thumbBtn->setSelectable(false);
	thumbBtn->setClickable(false);
	thumbBtn->setHoldable(true);
	thumbBtn->setTrigger(trigHeldA);
	thumbBtn->setVisible(false);

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
		// the two lines are centered on the card as a block: one a little above the middle, one a little below
		saveTitle[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		saveTitle[i]->setPosition(80,-12);
		saveSubtitle[i] = new GuiText(nullptr, 14, theme.textMuted);
		saveSubtitle[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		saveSubtitle[i]->setPosition(80,13);

		// each slot is a card, taking the accent outline and glow when highlighted
		saveBgImg[i] = new GuiShape(SLOT_WIDTH, SLOT_HEIGHT);
		saveBgImg[i]->setCorners(GuiCorners(theme.radius));
		saveBgImg[i]->setStyle(SaveCardStyle());
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
		saveBtn[i]->setPosition(SLOT_PITCH_X*(i % COLS),SLOT_PITCH_Y*(i / COLS));
		saveBtn[i]->setTrigger(trigA);
		saveBtn[i]->setState(STATE::DISABLED);
		saveBtn[i]->setEffectGrow(GRID_GROW);
		saveBtn[i]->setVisible(false);
		saveBtnLastOver[i] = false;

		// the tag sits half over the card's top edge, near its right end
		tagShape[i] = new GuiShape(TAG_PLAYING_WIDTH, TAG_HEIGHT);
		tagShape[i]->setParent(saveBtn[i]);
		tagShape[i]->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
		tagShape[i]->setPosition(-12, -TAG_HEIGHT / 2 + 1);
		tagShape[i]->setCapsule();
		tagShape[i]->setPaint(GuiPaint().color(theme.accent));
		tagText[i] = new GuiText("Playing from", 12, theme.textOnAccent);
		tagText[i]->setParent(tagShape[i]);
		tagText[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
		tagDot[i] = new GuiShape(8, 8);
		tagDot[i]->setParent(tagShape[i]);
		tagDot[i]->setShape(SHAPE::ELLIPSE);
		tagDot[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		tagDot[i]->setPosition(10, 0);
		tagDot[i]->setPaint(GuiPaint().color(Rgba(110, 225, 140, 255)));
		tagShown[i] = -1;

		// the slot badge, on the screenshot's top-left corner: AUTO in a pill, or the slot's number in a circle
		const int badgeY = (SLOT_HEIGHT - PREVIEW_HEIGHT) / 2 + BADGE_INSET;
		badgeAutoShape[i] = new GuiShape(BADGE_AUTO_WIDTH, BADGE_HEIGHT);
		badgeAutoShape[i]->setParent(saveBtn[i]);
		badgeAutoShape[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		badgeAutoShape[i]->setPosition(PREVIEW_X + BADGE_INSET, badgeY);
		badgeAutoShape[i]->setCapsule();
		badgeAutoShape[i]->setPaint(GuiPaint().color(theme.accent).stroke(1.0f, Rgba(255, 255, 255, 200)));
		badgeAutoText[i] = new GuiText("AUTO", 11, theme.textOnAccent);
		badgeAutoText[i]->setParent(badgeAutoShape[i]);
		badgeAutoText[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
		badgeNumShape[i] = new GuiShape(BADGE_NUM_SIZE, BADGE_NUM_SIZE);
		badgeNumShape[i]->setParent(saveBtn[i]);
		badgeNumShape[i]->setShape(SHAPE::ELLIPSE);
		badgeNumShape[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		badgeNumShape[i]->setPosition(PREVIEW_X + BADGE_INSET, badgeY);
		badgeNumShape[i]->setPaint(GuiPaint().color(Rgba(0, 0, 0, 185)).stroke(1.0f, Rgba(255, 255, 255, 200)));
		badgeNumText[i] = new GuiText(nullptr, 12, Rgba(255, 255, 255, 255));
		badgeNumText[i]->setParent(badgeNumShape[i]);
		badgeNumText[i]->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
		badgeShown[i] = -99;

		// over a screenshot that is not changing
		dimShape[i] = new GuiShape(PREVIEW_WIDTH, PREVIEW_HEIGHT);
		dimShape[i]->setParent(saveBtn[i]);
		dimShape[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
		dimShape[i]->setPosition(PREVIEW_X, (SLOT_HEIGHT - PREVIEW_HEIGHT) / 2);
		dimShape[i]->setPaint(GuiPaint().color(Rgba(40, 44, 52, 130)));
		infoShown[i] = 0;
	}

	// the pinned card: as wide as the grid, under it, with the save-file icon and a label at its right end
	pinnedTitle = new GuiText(nullptr, 18, theme.text);
	pinnedTitle->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	pinnedTitle->setPosition(PINNED_TEXT_X, -11);
	pinnedSubtitle = new GuiText(nullptr, 14, theme.textMuted);
	pinnedSubtitle->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	pinnedSubtitle->setPosition(PINNED_TEXT_X, 12);

	pinnedBg = new GuiShape(SLOT_WIDE_WIDTH, PINNED_HEIGHT);
	pinnedBg->setCorners(GuiCorners(theme.radius));
	pinnedBg->setStyle(SaveCardStyle());
	pinnedIcon = new GuiSaveFileIcon();
	pinnedIcon->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	pinnedIcon->setPosition(PINNED_ICON_X, 0);

	pinnedBtn = new GuiButton(SLOT_WIDE_WIDTH, PINNED_HEIGHT);
	pinnedBtn->setParent(this);
	pinnedBtn->setLabel(pinnedTitle, 0);
	pinnedBtn->setLabel(pinnedSubtitle, 1);
	pinnedBtn->setImage(pinnedBg);
	pinnedBtn->setIcon(pinnedIcon);
	pinnedBtn->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	pinnedBtn->setPosition(0, height + PINNED_GAP);
	pinnedBtn->setTrigger(trigA);
	pinnedBtn->setState(STATE::DISABLED);
	pinnedBtn->setEffectGrow(PINNED_GROW);
	pinnedBtn->setVisible(false);

	chipShape = new GuiShape(CHIP_ON_WIDTH, CHIP_HEIGHT);
	chipShape->setParent(pinnedBtn);
	chipShape->setAlignment(ALIGN_H::RIGHT, ALIGN_V::MIDDLE);
	chipShape->setPosition(-CHIP_INSET, 0);
	chipShape->setCapsule();
	chipShape->setPaint(GuiPaint().color(theme.surface).stroke(1.0f, theme.outline));
	chipText = new GuiText(nullptr, 13, theme.textMuted);
	chipText->setParent(chipShape);
	chipText->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	chipDot = new GuiShape(8, 8);
	chipDot->setParent(chipShape);
	chipDot->setShape(SHAPE::ELLIPSE);
	chipDot->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
	chipDot->setPosition(12, 0);
	chipDot->setPaint(GuiPaint().color(theme.good));
	chipState = -1;
	pinnedInfoShown = -1;
}

/**
 * Destructor for the GuiSaveBrowser class.
 */
GuiSaveBrowser::~GuiSaveBrowser()
{
	delete arrowUpBtn;
	delete arrowDownBtn;

	delete trackShape;
	delete thumbBtn;
	delete thumbShape;
	delete arrowUpShape;
	delete arrowDownShape;
	delete arrowUpGlyph;
	delete arrowDownGlyph;

	delete chipText;
	delete chipDot;
	delete chipShape;
	delete pinnedBtn;
	delete pinnedTitle;
	delete pinnedSubtitle;
	delete pinnedBg;
	delete pinnedIcon;

	delete trigA;
	delete trigHeldA;

	for(int i=0; i<SAVELISTSIZE; i++)
	{
		delete dimShape[i];
		delete badgeNumText[i];
		delete badgeNumShape[i];
		delete badgeAutoText[i];
		delete badgeAutoShape[i];
		delete tagText[i];
		delete tagDot[i];
		delete tagShape[i];
		delete saveBtn[i];
		delete saveTitle[i];
		delete saveSubtitle[i];
		delete saveBgImg[i];
		delete savePreviewImg[i];
	}
}

void GuiSaveBrowser::setRows(int r)
{
	r = (r < 1) ? 1 : (r > SAVELISTSIZE / COLS ? SAVELISTSIZE / COLS : r);
	if(r == rows)
		return;

	rows = r;
	height = rows * SLOT_PITCH_Y - SLOT_GAP_Y;
	trackHeight = height - 2 * SB_TRACK_Y;
	trackShape->setSize(SB_TRACK_WIDTH, trackHeight);
	thumbBtn->setSize(SB_WIDTH, trackHeight);
	thumbShape->setSize(SB_THUMB_WIDTH, trackHeight);
	pinnedBtn->setPosition(0, height + PINNED_GAP);
}

void GuiSaveBrowser::setEntries(const std::vector<SaveEntry>& list, int select)
{
	entries = list;
	const int n = (int)entries.size();
	const int grid = gridCount();

	if(select < 0 || select >= n)
		select = 0;

	// the cursor rests on a card to choose, never on a note: the first one, if it was asked to be on one
	if(hasPinned() && !pinnedSelectable() && select == n - 1)
		select = 0;
	if(select < grid && isInfoAt(select))
	{
		int first = 0;
		while(first < grid && isInfoAt(first))
			first++;
		select = (first < grid) ? first : (pinnedSelectable() ? n - 1 : 0);
	}

	// the cursor starts on the pinned card when that is what was asked for (or all there is); the grid then
	// keeps its first card to come back to
	pinnedOn = pinnedSelectable() && (select == n - 1 || grid == 0);
	if(pinnedOn)
		select = 0;

	// scroll so the card with the cursor is on the page: the first page, or with it on the last row
	if(select < pageSize())
		listOffset = 0;
	else
		listOffset = ((select / COLS) - (rows - 1)) * COLS;
	selectedItem = select - listOffset;

	// the cards show what they are given on the next update()
	for(int i=0; i<SAVELISTSIZE; i++)
	{
		saveBtn[i]->resetState();
		saveBtn[i]->setVisible(false);
		saveBtn[i]->setState(STATE::DISABLED);
		savePreviewImg[i]->setImage(nullptr);
		saveBtnLastOver[i] = false;
		tagShown[i] = -1;
		badgeShown[i] = -99;
	}

	pinnedBtn->resetState();
	pinnedBtn->setVisible(false);
	pinnedBtn->setState(STATE::DISABLED);
	pinnedLastOver = false;
}

int GuiSaveBrowser::getSelectedPosition() const
{
	if(entries.empty())
		return -1;
	if(pinnedOn)
		return (int)entries.size() - 1;
	const int p = listOffset + selectedItem;
	return (p >= 0 && p < gridCount()) ? p : -1;
}

GuiPopover::Anchor GuiSaveBrowser::getEntryAnchor(int position)
{
	// the pinned card is wide, so a popover opens over its label rather than beside it
	if(hasPinned() && position == (int)entries.size() - 1)
		return GuiPopover::Anchor(chipShape->getLeft(), pinnedBtn->getTop(), chipShape->getWidth(), pinnedBtn->getHeight());

	const int i = position - listOffset;
	if(i < 0 || i >= pageSize() || position >= gridCount())
		return GuiPopover::Anchor();
	return GuiPopover::Anchor(*saveBtn[i], GuiPopover::Anchor::BESIDE);
}

const SaveEntry * GuiSaveBrowser::getEntry(int position) const
{
	return (position >= 0 && position < (int)entries.size()) ? &entries[(size_t)position] : nullptr;
}

//! The card to land on when the cursor comes back up from the pinned card: in the last row shown, in the column it left
int GuiSaveBrowser::lastRowItem() const
{
	const int shown = std::max(1, std::min(pageSize(), gridCount() - listOffset));
	const int row = (shown - 1) / COLS;
	const int col = std::min(selectedItem % COLS, shown - 1 - row * COLS);
	return row * COLS + col;
}

void GuiSaveBrowser::setFocus(int f)
{
	focus = f;

	for(int i=0; i<SAVELISTSIZE; i++)
		saveBtn[i]->resetState();
	pinnedBtn->resetState();

	if(f == 1)
	{
		if(pinnedOn)
			pinnedBtn->setState(STATE::SELECTED);
		else if(selectedItem >= 0)
			saveBtn[selectedItem]->setState(STATE::SELECTED);
	}
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
	pinnedBtn->resetState();
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

	if(found < 0 && pinnedBtn->getState() == STATE::CLICKED)
	{
		pinnedBtn->setState(STATE::SELECTED);
		pinnedOn = true;
		found = (int)entries.size() - 1;
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
		if(i < pageSize() && saveBtn[i]->isVisible() && pos < gridCount())
		{
			const SaveEntry & e = entries[(size_t)pos];

			if(e.dimPreview)
				dimShape[i]->draw();

			if(e.badge == SaveEntry::BADGE_AUTO || e.badge == SaveEntry::BADGE_AUTO_PAUSED)
			{
				badgeAutoShape[i]->draw();
				badgeAutoText[i]->draw();
			}
			else if(e.badge > 0)
			{
				badgeNumShape[i]->draw();
				badgeNumText[i]->draw();
			}

			// the tag goes over the card's edge, so last
			if(e.tag != SaveEntry::TAG_NONE)
			{
				tagShape[i]->draw();
				if(e.tag == SaveEntry::TAG_AUTOSAVE)
					tagDot[i]->draw();
				tagText[i]->draw();
			}
		}
	}

	if(hasPinned() && pinnedBtn->isVisible())
	{
		pinnedBtn->draw();
		chipShape->draw();
		if(chipState >= 1)
			chipDot->draw();
		chipText->draw();
	}

	// the scrollbar is there only when there is something to scroll to
	if(scrollable)
	{
		trackShape->draw();
		thumbBtn->draw();
		arrowUpBtn->draw();
		arrowDownBtn->draw();
	}

	this->updateEffects();
}

void GuiSaveBrowser::update(InputController * controller)
{
	if(state == STATE::DISABLED || !controller)
		return;

	int i;
	const int length = gridCount();
	bool toPinned = false;

	const InputPadData & livePad = controller->getPadData();
	const int currentChan = controller->getChannel();
	const bool channelActive = livePad.validPointer || livePad.buttons_d != 0 || livePad.buttons_h != 0;

	if(scrollable)
	{
		arrowUpBtn->update(controller);
		arrowDownBtn->update(controller);
		if(thumbBtn->isVisible())
			thumbBtn->update(controller);
	}

	// drag the scrollbar thumb: the grid follows the pointer, the thumb's center under it
	{
		const auto dragPad = controller->getPadData();
		dragging = scrollable && thumbBtn->isVisible() && thumbTravel > 0 &&
			thumbBtn->getState() == STATE::HELD &&
			thumbBtn->getStateChan() == controller->getChannel() &&
			dragPad.validPointer;

		if(dragging)
		{
			int pos = dragPad.cursor_y - (this->getTop() + SB_TRACK_Y + thumbHeight / 2);
			if(pos < 0) pos = 0;
			else if(pos > thumbTravel) pos = thumbTravel;
			dragPos = pos;

			// the row that leads the page for that thumb position
			const int totalRows = (length + COLS - 1) / COLS;
			listOffset = ((pos * (totalRows - rows) + thumbTravel / 2) / thumbTravel) * COLS;

			// the cursor stays on a card that is there
			if(!pinnedOn && listOffset + selectedItem >= length)
				selectedItem = std::max(0, length - listOffset - 1);
		}
	}

	// where the cursor was, to go back to when a move would put it on a note
	const int prevOffset = listOffset;
	const int prevSelected = selectedItem;
	const bool prevPinned = pinnedOn;

	// pad/joystick navigation
	if(!focus)
		goto endNavigation; // skip navigation

	if(selectedItem < 0) selectedItem = 0;

	if(pinnedOn)
	{
		// the way out of the pinned card is up, back to the grid
		if(controller->up() || arrowUpBtn->getState() == STATE::CLICKED)
		{
			pinnedOn = false;
			selectedItem = lastRowItem();
		}
	}
	else if(controller->right())
	{
		if(selectedItem == pageSize()-1)
		{
			if(listOffset + pageSize() < length)
			{
				// move list down by 1 row
				listOffset += COLS;
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
			if(listOffset - COLS >= 0)
			{
				// move list up by 1
				listOffset -= COLS;
				selectedItem = pageSize()-1;
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
		// only the pad goes on from the last row to the pinned card; the scrollbar's arrow just scrolls
		const bool viaPad = controller->down();

		if(selectedItem >= pageSize()-COLS)
		{
			if(listOffset + pageSize() + COLS - 1 < length)
			{
				listOffset += COLS;
			}
			else if(listOffset + pageSize() < length)
			{
				listOffset += COLS;

				if(selectedItem == pageSize()-1)
					selectedItem -= 1;
			}
			else if(viaPad && pinnedSelectable())
			{
				toPinned = true;
			}
		}
		else if(saveBtn[selectedItem+COLS]->isVisible())
		{
			selectedItem += COLS;
		}
		else if(viaPad && pinnedSelectable())
		{
			toPinned = true;
		}
	}
	else if(controller->up() || arrowUpBtn->getState() == STATE::CLICKED)
	{
		if(selectedItem < COLS)
		{
			if(listOffset - COLS >= 0)
			{
				// move list up by 1
				listOffset -= COLS;
			}
		}
		else
		{
			if(saveBtn[selectedItem-COLS]->isVisible())
			{
				selectedItem -= COLS;
			}
		}
	}

	if(toPinned)
		pinnedOn = true;

	// the cursor never rests on a note: a move onto one is no move
	if(!pinnedOn && isInfoAt(listOffset + selectedItem))
	{
		listOffset = prevOffset;
		selectedItem = prevSelected;
		pinnedOn = prevPinned;
	}

	endNavigation:

	if(arrowDownBtn->getState() == STATE::CLICKED)
		arrowDownBtn->resetState();

	if(arrowUpBtn->getState() == STATE::CLICKED)
		arrowUpBtn->resetState();

	for(i=0; i<SAVELISTSIZE; i++)
	{
		if(i < pageSize() && listOffset+i < length)
		{
			const SaveEntry & e = entries[(size_t)(listOffset+i)];
			const GuiTheme& theme = GuiTheme::current();

			if(e.info)
			{
				// a note: shown, with nothing to select or click
				saveBtn[i]->setVisible(true);
				saveBtn[i]->setState(STATE::DISABLED);
			}
			else if(saveBtn[i]->getState() == STATE::DISABLED || !saveBtn[i]->isVisible())
			{
				saveBtn[i]->setVisible(true);
				saveBtn[i]->setState(STATE::DEFAULT);
			}

			// a note is dark with white words; a card is white
			const int wantInfo = e.info ? 1 : 0;
			if(infoShown[i] != wantInfo)
			{
				infoShown[i] = wantInfo;
				saveBgImg[i]->setStyle(wantInfo ? SaveNoteStyle() : SaveCardStyle());
				saveTitle[i]->setColor(wantInfo ? Rgba(255, 255, 255, 255) : theme.text);
				saveSubtitle[i]->setColor(wantInfo ? Rgba(255, 255, 255, 255) : theme.textMuted);
			}

			// the slot badge: AUTO (live, or gray when paused) or the slot's number
			if(badgeShown[i] != e.badge)
			{
				badgeShown[i] = e.badge;
				if(e.badge == SaveEntry::BADGE_AUTO)
					badgeAutoShape[i]->setPaint(GuiPaint().color(theme.accent).stroke(1.0f, Rgba(255, 255, 255, 200)));
				else if(e.badge == SaveEntry::BADGE_AUTO_PAUSED)
					badgeAutoShape[i]->setPaint(GuiPaint().color(Rgba(112, 118, 126, 255)).stroke(1.0f, Rgba(255, 255, 255, 170)));
				else if(e.badge > 0)
				{
					char number[12];
					snprintf(number, sizeof(number), "%d", e.badge);
					badgeNumText[i]->setText(number);
				}
			}

			// the tag on the card's edge
			if(tagShown[i] != (int)e.tag)
			{
				tagShown[i] = (int)e.tag;
				int tagW = TAG_PLAYING_WIDTH;
				const char * words = "Playing from";
				PixelColor fill = theme.accent;
				int shift = 0;

				if(e.tag == SaveEntry::TAG_AUTOSAVE)
				{
					tagW = TAG_AUTO_WIDTH;
					words = "Autosave";
					shift = 6;	// the dot takes the left end
				}
				else if(e.tag == SaveEntry::TAG_PAUSED)
				{
					tagW = TAG_PAUSED_WIDTH;
					words = "Paused";
					fill = Rgba(112, 118, 126, 255);
				}
				tagShape[i]->setSize(tagW, TAG_HEIGHT);
				tagShape[i]->setPaint(GuiPaint().color(fill));
				tagText[i]->setText(words);
				tagText[i]->setPosition(shift, 0);
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

		// one card has the cursor: not one in the grid while it is on the pinned card
		if((pinnedOn || i != selectedItem) && saveBtn[i]->getState() == STATE::SELECTED)
			saveBtn[i]->resetState();
		else if(focus && !pinnedOn && i == selectedItem && saveBtn[i]->getState() == STATE::DEFAULT)
			saveBtn[selectedItem]->setState(STATE::SELECTED, channelActive ? currentChan : -1);	// -1: shows now, and any controller may click it
		else if(focus && channelActive && !pinnedOn && i == selectedItem && saveBtn[i]->getState() == STATE::SELECTED &&
			saveBtn[i]->getStateChan() != -1 && saveBtn[i]->getStateChan() != currentChan)
			saveBtn[i]->resetState();	// claimed by a channel that is not in use: let this one claim it on the next card pass

		auto pad = controller->getPadData();

		if(pad.validPointer)
		{
			if(!saveBtnLastOver[i] && saveBtn[i]->isInside(pad.cursor_x, pad.cursor_y))
				saveBtn[i]->resetState();
			saveBtnLastOver[i] = saveBtn[i]->isInside(pad.cursor_x, pad.cursor_y);
		}

		saveBtn[i]->update(controller);

		if(saveBtn[i]->getState() == STATE::SELECTED)
		{
			selectedItem = i;
			pinnedOn = false;
		}
	}

	if(hasPinned())
	{
		const SaveEntry & e = entries.back();

		if(e.info)
		{
			pinnedBtn->setVisible(true);
			pinnedBtn->setState(STATE::DISABLED);	// a note: nothing to select or click
		}
		else if(pinnedBtn->getState() == STATE::DISABLED || !pinnedBtn->isVisible())
		{
			pinnedBtn->setVisible(true);
			pinnedBtn->setState(STATE::DEFAULT);
		}

		// a note is dark with white words; a card is white
		const int wantInfo = e.info ? 1 : 0;
		if(pinnedInfoShown != wantInfo)
		{
			const GuiTheme& theme = GuiTheme::current();
			pinnedInfoShown = wantInfo;
			chipState = -1;
			pinnedBg->setStyle(wantInfo ? SaveNoteStyle() : SaveCardStyle());
			pinnedTitle->setColor(wantInfo ? Rgba(255, 255, 255, 255) : theme.text);
			pinnedSubtitle->setColor(wantInfo ? Rgba(255, 255, 255, 255) : theme.textMuted);
		}
		pinnedIcon->setNoBattery(e.noBattery);

		pinnedTitle->setText(e.title.c_str());
		pinnedSubtitle->setText(e.subtitle.c_str());
		chipText->setText(e.chip.c_str());
		chipShape->setVisible(!e.chip.empty());
		chipText->setVisible(!e.chip.empty());

		// on: a green dot and the accent color on a light tint (or white on a note); off: plain gray
		const int wantChip = e.chipOn ? (e.info ? 2 : 1) : 0;
		if(chipState != wantChip)
		{
			const GuiTheme& theme = GuiTheme::current();
			chipState = wantChip;
			chipShape->setSize(wantChip ? CHIP_ON_WIDTH : CHIP_OFF_WIDTH, CHIP_HEIGHT);
			if(wantChip == 1)
			{
				chipShape->setPaint(GuiPaint().color(theme.highlight).stroke(1.0f, theme.accentLight));
				chipText->setPosition(8, 0);
				chipText->setColor(theme.accent);
			}
			else if(wantChip == 2)
			{
				chipShape->setPaint(GuiPaint().color(Rgba(255, 255, 255, 26)).stroke(1.0f, Rgba(255, 255, 255, 150)));
				chipText->setPosition(8, 0);
				chipText->setColor(Rgba(255, 255, 255, 255));
			}
			else
			{
				chipShape->setPaint(GuiPaint().color(theme.surface).stroke(1.0f, theme.outline));
				chipText->setPosition(0, 0);
				chipText->setColor(theme.textMuted);
			}
		}

		if(!pinnedOn && pinnedBtn->getState() == STATE::SELECTED)
			pinnedBtn->resetState();
		else if(focus && pinnedOn && pinnedBtn->getState() == STATE::DEFAULT)
			pinnedBtn->setState(STATE::SELECTED, channelActive ? currentChan : -1);
		else if(focus && channelActive && pinnedOn && pinnedBtn->getState() == STATE::SELECTED &&
			pinnedBtn->getStateChan() != -1 && pinnedBtn->getStateChan() != currentChan)
			pinnedBtn->resetState();

		auto pad = controller->getPadData();

		if(pad.validPointer)
		{
			if(!pinnedLastOver && pinnedBtn->isInside(pad.cursor_x, pad.cursor_y))
				pinnedBtn->resetState();
			pinnedLastOver = pinnedBtn->isInside(pad.cursor_x, pad.cursor_y);
		}

		pinnedBtn->update(controller);

		// the pointer moved onto it
		if(pinnedBtn->getState() == STATE::SELECTED)
			pinnedOn = true;
	}

	updateThumb();

	if(updateCB)
		updateCB(this);
}

//! Sizes and places the scrollbar thumb for the rows currently shown, and shows the scrollbar only when some rows are off screen
void GuiSaveBrowser::updateThumb()
{
	const int totalRows = (gridCount() + COLS - 1) / COLS;
	int firstRow = listOffset / COLS;

	scrollable = totalRows > rows;
	if(!scrollable)
	{
		thumbBtn->setVisible(false);
		thumbTravel = 0;
		return;
	}
	if(firstRow < 0) firstRow = 0;
	if(firstRow > totalRows - rows) firstRow = totalRows - rows;

	thumbHeight = std::max(SB_THUMB_MIN_HEIGHT, trackHeight * rows / totalRows);
	thumbTravel = trackHeight - thumbHeight;

	// while dragged the thumb follows the pointer exactly; otherwise it sits at the list position
	int y = dragging ? dragPos : thumbTravel * firstRow / (totalRows - rows);
	if(y > thumbTravel) y = thumbTravel;

	thumbBtn->setVisible(true);
	thumbBtn->setSize(SB_WIDTH, thumbHeight);
	thumbShape->setSize(SB_THUMB_WIDTH, thumbHeight);
	thumbBtn->setMinY(0);
	thumbBtn->setMaxY(thumbTravel);
	thumbBtn->setPosition(-SB_INSET, SB_TRACK_Y + y);
}

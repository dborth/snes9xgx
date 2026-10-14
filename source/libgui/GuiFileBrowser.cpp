/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiFileBrowser.cpp
 ***************************************************************************/

#include "Gui.h"
#include "../filebrowser.h"

// Layout of the browser, in design pixels. The browser is one rounded card: the rows
// sit inset from its edge, with the scrollbar column down the right side.
#define BROWSER_PAD			6		// card edge to rows, and top/bottom of the scrollbar column
#define ROW_HEIGHT			27		// the highlight pill is 2 shorter, leaving a gap between rows
#define ROW_TEXT_Y			2		// text and icons sit this far below the row's middle: a font's
#define ROW_ICON_Y			2		// line box carries room for descenders and the icon art is
										// drawn high in its image, so they otherwise look raised
#define ROW_GAP				6		// rows to scrollbar column
#define SB_WIDTH			22		// scrollbar column (arrow buttons and thumb hit area)
#define SB_INSET			7		// column to card's right edge
#define SB_ARROW			22		// arrow button size
#define SB_TRACK_WIDTH		6
#define SB_TRACK_Y			(BROWSER_PAD + SB_ARROW + 6)
#define SB_THUMB_WIDTH		14
#define SB_THUMB_HEIGHT		38
#define ROW_TEXT_X			10
#define ROW_TEXT_X_ICON		34
#define ROW_TEXT_PAD		6		// room kept at the row's right end

//! Step size (in rows) for one scroll action while the on-screen scrollbar
//! arrow is held, based on how long it's been held continuously.
static int fastScrollStep(float heldDuration)
{
	if(heldDuration >= 2.5f)
		return 10;
	else if(heldDuration >= 1.0f)
		return 4;
	else
		return 1;
}

//! The selected-row tint a little deeper, for the moment the row is clicked
static PixelColor rowPressColor(PixelColor tint, PixelColor accent)
{
	PixelColor c = tint;
	c.r = (uint8_t)(tint.r + (accent.r - tint.r) * 0.18f + 0.5f);
	c.g = (uint8_t)(tint.g + (accent.g - tint.g) * 0.18f + 0.5f);
	c.b = (uint8_t)(tint.b + (accent.b - tint.b) * 0.18f + 0.5f);
	return c;
}

GuiFileBrowser::GuiFileBrowser(int w, int h)
	: previewDefaultImage(default_preview_png)
{
	width = w;
	height = h;
	numEntries = 0;
	selectedItem = 0;
	selectable = true;
	listChanged = true; // trigger an initial list update
	focus = 0; // allow focus

	const GuiTheme& theme = GuiTheme::current();
	const int rowWidth = w - BROWSER_PAD - ROW_GAP - SB_WIDTH - SB_INSET;
	const int rowTop = (h - FILE_PAGESIZE * ROW_HEIGHT) / 2;	// rows centred in the card
	const int trackHeight = h - 2 * SB_TRACK_Y;
	scrollTravel = trackHeight - SB_THUMB_HEIGHT;

	trigA = new GuiTrigger;
	trigA->setPrimaryTrigger();

	trigHeldA = new GuiTrigger;
	trigHeldA->setHeldTrigger(-1, INPUT_BTN_A);

	btnSoundOver = new GuiSound(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	btnSoundClick = new GuiSound(button_click_pcm, button_click_pcm_size, SOUND::PCM);

	// the card everything sits on
	cardShape = new GuiShape(w, h);
	cardShape->setParent(this);
	cardShape->setCorners(GuiCorners(theme.radius));
	cardShape->setStyle(theme.cardStyle());

	iconFolder = new GuiImageData(icon_folder_png);
	iconSD = new GuiImageData(icon_sd_png);
	iconUSB = new GuiImageData(icon_usb_png);
	iconDVD = new GuiImageData(icon_dvd_png);
	iconSMB = new GuiImageData(icon_smb_png);

	// scrollbar: a thin track, round arrow buttons and a pill thumb. They gray at rest
	// and take the accent color when the pointer is on them or they are held.
	const GuiPaint controlRest = GuiPaint().color(theme.surfaceRaised).stroke(1.5f, theme.outline);
	const GuiPaint controlLit = GuiPaint().color(theme.accent).stroke(1.5f, theme.accent);
	const GuiPaint controlHeld = GuiPaint().color(theme.accentDark).stroke(1.5f, theme.accentDark);
	const GuiShapeStyle controlStyle = GuiShapeStyle(controlRest).onFocus(controlLit).onPress(controlHeld);

	trackShape = new GuiShape(SB_TRACK_WIDTH, trackHeight);
	trackShape->setParent(this);
	trackShape->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	trackShape->setPosition(-(SB_INSET + (SB_WIDTH - SB_TRACK_WIDTH) / 2), SB_TRACK_Y);
	trackShape->setCapsule();
	trackShape->setPaint(GuiPaint().color(theme.outline));

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

	thumbShape = new GuiShape(SB_THUMB_WIDTH, SB_THUMB_HEIGHT);
	thumbShape->setCapsule();
	thumbShape->setStyle(GuiShapeStyle(GuiPaint().color(theme.outlineStrong))
		.onFocus(GuiPaint().color(theme.accent)).onPress(GuiPaint().color(theme.accentDark)));
	thumbShape->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	arrowUpBtn = new GuiButton(SB_WIDTH, SB_ARROW);
	arrowUpBtn->setParent(this);
	arrowUpBtn->setImage(arrowUpShape);
	arrowUpBtn->setIcon(arrowUpGlyph);
	arrowUpBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	arrowUpBtn->setPosition(-SB_INSET, BROWSER_PAD);
	arrowUpBtn->setSelectable(false);
	arrowUpBtn->setClickable(false);
	arrowUpBtn->setHoldable(true);
	arrowUpBtn->setTrigger(trigHeldA);
	arrowUpBtn->setSoundOver(btnSoundOver);
	arrowUpBtn->setSoundClick(btnSoundClick);

	arrowDownBtn = new GuiButton(SB_WIDTH, SB_ARROW);
	arrowDownBtn->setParent(this);
	arrowDownBtn->setImage(arrowDownShape);
	arrowDownBtn->setIcon(arrowDownGlyph);
	arrowDownBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	arrowDownBtn->setPosition(-SB_INSET, -BROWSER_PAD);
	arrowDownBtn->setSelectable(false);
	arrowDownBtn->setClickable(false);
	arrowDownBtn->setHoldable(true);
	arrowDownBtn->setTrigger(trigHeldA);
	arrowDownBtn->setSoundOver(btnSoundOver);
	arrowDownBtn->setSoundClick(btnSoundClick);

	scrollbarBoxBtn = new GuiButton(SB_WIDTH, SB_THUMB_HEIGHT);
	scrollbarBoxBtn->setParent(this);
	scrollbarBoxBtn->setImage(thumbShape);
	scrollbarBoxBtn->setAlignment(ALIGN_H::RIGHT, ALIGN_V::TOP);
	scrollbarBoxBtn->setPosition(-SB_INSET, SB_TRACK_Y);
	scrollbarBoxBtn->setMinY(0);
	scrollbarBoxBtn->setMaxY(scrollTravel);
	scrollbarBoxBtn->setSelectable(false);
	scrollbarBoxBtn->setClickable(false);
	scrollbarBoxBtn->setHoldable(true);
	scrollbarBoxBtn->setTrigger(trigHeldA);

	// The selected row is a rounded, tinted pill inset from the card edge, so it never
	// touches the card's corners. It fades in and out as the selection moves.
	const GuiPaint rowLit = GuiPaint().color(theme.highlight).stroke(1.5f, theme.accent);
	const GuiPaint rowHeld = GuiPaint().color(rowPressColor(theme.highlight, theme.accent)).stroke(1.5f, theme.accent);
	const GuiShapeStyle rowStyle = GuiShapeStyle(GuiPaint()).onFocus(rowLit).onPress(rowHeld);

	for(int i=0; i<FILE_PAGESIZE; ++i)
	{
		fileListText[i] = new GuiText(nullptr, 20, theme.text);
		fileListText[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		fileListText[i]->setPosition(ROW_TEXT_X,ROW_TEXT_Y);
		fileListText[i]->setMaxWidth(rowWidth - ROW_TEXT_X - ROW_TEXT_PAD);

		fileListBg[i] = new GuiShape(rowWidth, ROW_HEIGHT - 2);
		fileListBg[i]->setCorners(GuiCorners(9.0f));
		fileListBg[i]->setStyle(rowStyle);
		fileListBg[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
		fileListIcon[i] = nullptr;

		fileList[i] = new GuiButton(rowWidth, ROW_HEIGHT);
		fileList[i]->setParent(this);
		fileList[i]->setLabel(fileListText[i]);
		fileList[i]->setImage(fileListBg[i]);
		fileList[i]->setPosition(BROWSER_PAD, rowTop + ROW_HEIGHT*i);
		fileList[i]->setTrigger(trigA);
		fileList[i]->setSoundClick(btnSoundClick);
	}
}

GuiFileBrowser::~GuiFileBrowser()
{
	delete arrowUpBtn;
	delete arrowDownBtn;
	delete scrollbarBoxBtn;

	delete cardShape;
	delete trackShape;
	delete arrowUpShape;
	delete arrowDownShape;
	delete arrowUpGlyph;
	delete arrowDownGlyph;
	delete thumbShape;

	delete iconFolder;
	delete iconSD;
	delete iconUSB;
	delete iconDVD;
	delete iconSMB;

	delete btnSoundOver;
	delete btnSoundClick;
	delete trigHeldA;
	delete trigA;

	for(int i=0; i<FILE_PAGESIZE; i++)
	{
		delete fileListText[i];
		delete fileList[i];
		delete fileListBg[i];

		if(fileListIcon[i])
			delete fileListIcon[i];
	}
}

void GuiFileBrowser::setFocus(int f)
{
	focus = f;

	for(int i=0; i<FILE_PAGESIZE; i++)
		fileList[i]->resetState();

	if(f == 1)
		fileList[selectedItem]->setState(STATE::SELECTED);
}

void GuiFileBrowser::resetState()
{
	state = STATE::DEFAULT;
	stateChan = -1;
	selectedItem = 0;

	for(int i=0; i<FILE_PAGESIZE; i++)
	{
		fileList[i]->resetState();
	}

	// The listing itself just changed (new folder/device) - index N no
	// longer refers to the same file it used to, so any cached/in-flight
	// preview needs to go. What's on screen stays until the new selection resolves.
	if(previewCache)
	{
		previewCache->flush();
		previewRequestedIndex = -1;
	}
}

void GuiFileBrowser::setPreviewImage(GuiImage * target, GuiPreviewPathResolver resolver, void * resolverContext,
                                      int capacity, int prefetchRadius, int maxImageWidth, int maxImageHeight,
                                      GuiPreviewImageChangedCB changedCB, void * changedCBContext)
{
	if(!target)
	{
		// disable - free the cache and its background thread
		previewCache.reset();
		previewTarget = nullptr;
		previewLastImage = nullptr;
		previewResolver = nullptr;
		previewResolverContext = nullptr;
		previewChangedCB = nullptr;
		previewChangedCBContext = nullptr;
		previewRequestedIndex = -1;
		return;
	}

	previewCache.reset(new GuiImageAsyncCache(capacity, prefetchRadius, maxImageWidth, maxImageHeight));
	previewTarget = target;
	previewLastImage = nullptr;
	previewResolver = resolver;
	previewResolverContext = resolverContext;
	previewChangedCB = changedCB;
	previewChangedCBContext = changedCBContext;
	previewRequestedIndex = -1;
	showPreview(&previewDefaultImage, true);
}

void GuiFileBrowser::showPreview(GuiImageData * img, bool isDefault)
{
	if(img == previewLastImage)
		return;

	previewTarget->setImage(img);
	previewLastImage = img;
	previewCache->pin(isDefault ? nullptr : img);
	if(previewChangedCB)
		previewChangedCB(previewChangedCBContext, previewTarget, isDefault);
}

void GuiFileBrowser::refreshPreview()
{
	if(!previewCache)
		return;

	previewCache->flush();
	previewRequestedIndex = -1;
}

void GuiFileBrowser::triggerUpdate()
{
	int newIndex = browser.selIndex-browser.pageIndex;
	
	if(newIndex >= FILE_PAGESIZE)
		newIndex = FILE_PAGESIZE-1;
	else if(newIndex < 0)
		newIndex = 0;

	selectedItem = newIndex;
	listChanged = true;
}

/**
 * Draw the button on screen
 */
void GuiFileBrowser::draw()
{
	if(!this->isVisible())
		return;

	cardShape->draw();

	for(uint32_t i=0; i<FILE_PAGESIZE; ++i)
	{
		fileList[i]->draw();
	}

	trackShape->draw();
	arrowUpBtn->draw();
	arrowDownBtn->draw();
	scrollbarBoxBtn->draw();

	this->updateEffects();
}

void GuiFileBrowser::update(InputController * controller)
{
	if(state == STATE::DISABLED || !controller)
		return;

	int position = 0;
	int positionWiimote = 0;

	arrowUpBtn->update(controller);
	arrowDownBtn->update(controller);
	scrollbarBoxBtn->update(controller);

	auto pad = controller->getPadData();
	int currentChan = controller->getChannel();
	// A InputController with no live signal this frame (e.g. a persistent
	// but currently-disconnected controller slot) must not be able to claim
	// or evict a selection just by taking its turn through this loop -- only
	// a channel that's genuinely doing something (pointing or pressing) gets
	// a say in who owns the selected slot.
	bool channelActive = pad.validPointer || pad.buttons_d != 0 || pad.buttons_h != 0;

	// move the file listing to respond to wiimote cursor movement
	if(scrollbarBoxBtn->getState() == STATE::HELD &&
		scrollbarBoxBtn->getStateChan() == currentChan &&
		pad.validPointer &&
		browser.numEntries > FILE_PAGESIZE)
	{
		scrollbarBoxBtn->setPosition(-SB_INSET, 0);
		// put the thumb's center under the pointer (thumb travels from SB_TRACK_Y)
		positionWiimote = pad.cursor_y - (SB_TRACK_Y + SB_THUMB_HEIGHT / 2) - scrollbarBoxBtn->getTop();

		if(positionWiimote < scrollbarBoxBtn->getMinY())
			positionWiimote = scrollbarBoxBtn->getMinY();
		else if(positionWiimote > scrollbarBoxBtn->getMaxY())
			positionWiimote = scrollbarBoxBtn->getMaxY();

		browser.pageIndex = (positionWiimote * browser.numEntries)/(float)scrollTravel - selectedItem;

		if(browser.pageIndex <= 0)
		{
			browser.pageIndex = 0;
		}
		else if(browser.pageIndex+FILE_PAGESIZE >= browser.numEntries)
		{
			browser.pageIndex = browser.numEntries-FILE_PAGESIZE;
		}
		listChanged = true;
		focus = false;
	}

    // Evaluate simulated D-Pad actions via on-screen buttons
	bool simulateDown = arrowDownBtn->getState() == STATE::HELD && arrowDownBtn->getStateChan() == currentChan;
	bool simulateUp = arrowUpBtn->getState() == STATE::HELD && arrowUpBtn->getStateChan() == currentChan;

	if(simulateDown || simulateUp)
		arrowHoldDuration += platform->getVideo()->getDeltaTime();
	else
		arrowHoldDuration = 0.0f;

	if(simulateDown)
	{
		if(!this->isFocused())
			((GuiWindow *)this->getParent())->changeFocus(this);
	}
	else if(simulateUp)
	{
		if(!this->isFocused())
			((GuiWindow *)this->getParent())->changeFocus(this);
	}

	// pad/joystick navigation
	if(!focus)
	{
		goto endNavigation; // skip navigation
		listChanged = false;
	}

	if(controller->right())
	{
		if(browser.pageIndex < browser.numEntries && browser.numEntries > FILE_PAGESIZE)
		{
			browser.pageIndex += FILE_PAGESIZE;
			if(browser.pageIndex+FILE_PAGESIZE >= browser.numEntries)
				browser.pageIndex = browser.numEntries-FILE_PAGESIZE;
			listChanged = true;
		}
	}
	else if(controller->left())
	{
		if(browser.pageIndex > 0)
		{
			browser.pageIndex -= FILE_PAGESIZE;
			if(browser.pageIndex < 0)
				browser.pageIndex = 0;
			listChanged = true;
		}
	}
	else if(controller->down() || simulateDown)
	{
		// D-pad/analog holds always step by 1 row at a time (their own
		// repeat rate is InputController's acceleration ramp); only the
		// on-screen scrollbar arrow gets the extra fast-scroll ramp.
		int step = simulateDown ? fastScrollStep(arrowHoldDuration) : 1;
		int newAbsolute = browser.pageIndex + selectedItem + step;

		if(newAbsolute >= browser.numEntries)
			newAbsolute = browser.numEntries - 1;

		if(newAbsolute > browser.pageIndex + selectedItem)
		{
			if(newAbsolute >= browser.pageIndex + FILE_PAGESIZE)
			{
				// target isn't on the current page - scroll to it
				browser.pageIndex = newAbsolute - (FILE_PAGESIZE - 1);
				selectedItem = FILE_PAGESIZE - 1;
				listChanged = true;
			}
			else
			{
				fileList[selectedItem]->resetState();
				selectedItem = newAbsolute - browser.pageIndex;
			}
			fileList[selectedItem]->setState(STATE::SELECTED, currentChan);
		}
	}
	else if(controller->up() || simulateUp)
	{
		int step = simulateUp ? fastScrollStep(arrowHoldDuration) : 1;
		int newAbsolute = browser.pageIndex + selectedItem - step;

		if(newAbsolute < 0)
			newAbsolute = 0;

		if(newAbsolute < browser.pageIndex + selectedItem)
		{
			if(newAbsolute < browser.pageIndex)
			{
				// target isn't on the current page - scroll to it
				browser.pageIndex = newAbsolute;
				selectedItem = 0;
				listChanged = true;
			}
			else
			{
				fileList[selectedItem]->resetState();
				selectedItem = newAbsolute - browser.pageIndex;
			}
			fileList[selectedItem]->setState(STATE::SELECTED, currentChan);
		}
	}

	endNavigation:

	for(int i=0; i<FILE_PAGESIZE; ++i)
	{
		if(listChanged || numEntries != browser.numEntries)
		{
			if(browser.pageIndex+i < browser.numEntries)
			{
				if(fileList[i]->getState() == STATE::DISABLED)
					fileList[i]->setState(STATE::DEFAULT);

				fileList[i]->setVisible(true);

				fileListText[i]->setText(browserList[browser.pageIndex+i].displayname);

				if(fileListIcon[i])
				{
					delete fileListIcon[i];
					fileListIcon[i] = nullptr;
				}
				fileListText[i]->setPosition(ROW_TEXT_X,ROW_TEXT_Y);
				fileListText[i]->setMaxWidth(fileList[i]->getWidth() - ROW_TEXT_X - ROW_TEXT_PAD);

				switch(browserList[browser.pageIndex+i].icon)
				{
					case ICON_FOLDER:
						fileListIcon[i] = new GuiImage(iconFolder);
						break;
					case ICON_SD:
						fileListIcon[i] = new GuiImage(iconSD);
						break;
					case ICON_USB:
						fileListIcon[i] = new GuiImage(iconUSB);
						break;
					case ICON_DVD:
						fileListIcon[i] = new GuiImage(iconDVD);
						break;
					case ICON_SMB:
						fileListIcon[i] = new GuiImage(iconSMB);
						break;
				}
				fileList[i]->setIcon(fileListIcon[i]);
				if(fileListIcon[i] != nullptr)
				{
					fileListIcon[i]->setAlignment(ALIGN_H::LEFT, ALIGN_V::MIDDLE);
					fileListIcon[i]->setPosition(6,ROW_ICON_Y);
					fileListText[i]->setPosition(ROW_TEXT_X_ICON,ROW_TEXT_Y);
					fileListText[i]->setMaxWidth(fileList[i]->getWidth() - ROW_TEXT_X_ICON - ROW_TEXT_PAD);
				}
			}
			else
			{
				fileList[i]->setVisible(false);
				fileList[i]->setState(STATE::DISABLED);
			}
		}

		if(i != selectedItem && fileList[i]->getState() == STATE::SELECTED)
			fileList[i]->resetState();
		else if(focus && channelActive && i == selectedItem && fileList[i]->getState() == STATE::DEFAULT)
			fileList[selectedItem]->setState(STATE::SELECTED, currentChan);
		else if(focus && channelActive && i == selectedItem && fileList[i]->getState() == STATE::SELECTED &&
				fileList[i]->getStateChan() != -1 && fileList[i]->getStateChan() != currentChan)
			// Slot is already SELECTED but carries a stale channel from an earlier
			// selection (e.g. a reused slot after paging, or a preselected item that
			// was never actually hovered by the real channel yet). Without this,
			// currentChan == stateChan never holds and clicks are silently ignored
			// until the item happens to be navigated away from and back.
			fileList[i]->resetState();

		// Present a "no channel" (-1) identity to any item the cursor isn't
		// currently over. Without this, a stale stateChan left on a reused
		// list slot (e.g. after paging/navigating) can permanently block
		// clicks from the real channel until the item is re-hovered.
		if(pad.validPointer && !fileList[i]->isInside(pad.cursor_x, pad.cursor_y))
			controller->setChannel(-1);

		fileList[i]->update(controller);
		controller->setChannel(currentChan);

		if(fileList[i]->getState() == STATE::SELECTED)
		{
			selectedItem = i;
			browser.selIndex = browser.pageIndex + i;
		}

		if(selectedItem == i)
			fileListText[i]->setScroll(SCROLL::HORIZONTAL);
		else
			fileListText[i]->setScroll(SCROLL::NONE);
	}

	// update the location of the scroll box based on the position in the file list
	if(positionWiimote > 0)
	{
		position = positionWiimote; // follow wiimote cursor
		scrollbarBoxBtn->setPosition(-SB_INSET,position+SB_TRACK_Y);
	}
	else if(listChanged || numEntries != browser.numEntries)
	{
		if(float((browser.pageIndex<<1))/(float(FILE_PAGESIZE)) < 1.0)
		{
			position = 0;
		}
		else if(browser.pageIndex+FILE_PAGESIZE >= browser.numEntries)
		{
			position = scrollTravel;
		}
		else
		{
			position = scrollTravel * (browser.pageIndex + FILE_PAGESIZE/2) / (float)browser.numEntries;
		}
		scrollbarBoxBtn->setPosition(-SB_INSET,position+SB_TRACK_Y);
	}

	listChanged = false;
	numEntries = browser.numEntries;

	if(previewCache)
	{
		if(browser.selIndex != previewRequestedIndex)
		{
			previewRequestedIndex = browser.selIndex;
			previewSelHasPath = false;

			char path[GUI_IMAGE_CACHE_MAX_PATH];
			if(previewResolver && browser.selIndex >= 0 && browser.selIndex < browser.numEntries &&
			   previewResolver(previewResolverContext, browser.selIndex, path, sizeof(path)))
			{
				previewSelHasPath = true;
				previewCache->request(browser.selIndex, path);

				int radius = previewCache->getPrefetchRadius();
				for(int d = 1; d <= radius; d++)
				{
					char neighborPath[GUI_IMAGE_CACHE_MAX_PATH];

					if(browser.selIndex + d < browser.numEntries &&
					   previewResolver(previewResolverContext, browser.selIndex + d, neighborPath, sizeof(neighborPath)))
						previewCache->prefetch(browser.selIndex + d, neighborPath);

					if(browser.selIndex - d >= 0 &&
					   previewResolver(previewResolverContext, browser.selIndex - d, neighborPath, sizeof(neighborPath)))
						previewCache->prefetch(browser.selIndex - d, neighborPath);
				}
			}
		}

		bool landed = previewCache->update();

		if(previewTarget)
		{
			if(landed && previewLastImage && previewLastImage != &previewDefaultImage)
			{
				if(previewLastImage->getTexture())
				{
					previewTarget->setImage(previewLastImage);
					if(previewChangedCB)
						previewChangedCB(previewChangedCBContext, previewTarget, false);
				}
				else // that landing was a failed decode that wiped the slot: fall back, never blank
				{
					previewLastImage = nullptr;
					showPreview(&previewDefaultImage, true);
				}
			}

			if(!previewSelHasPath)
				showPreview(&previewDefaultImage, true);
			else if(previewCache->isResolved(browser.selIndex))
			{
				GuiImageData * img = previewCache->get(browser.selIndex);
				if(img)
					showPreview(img, false);
				else
					showPreview(&previewDefaultImage, true);
			}
		}
	}

	if(updateCB)
		updateCB(this);
}

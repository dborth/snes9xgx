/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui_button.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"
/**
 * Constructor for the GuiButton class.
 */
GuiButton::GuiButton(int w, int h)
{
	width = w;
	height = h;
	image = NULL;
	imageOver = NULL;
	icon = NULL;
	iconOver = NULL;
	label = NULL;
	labelOver = NULL;
	soundOver = NULL;
	soundClick = NULL;
	selectable = true;
	clickable = true;
}

/**
 * Destructor for the GuiButton class.
 */
GuiButton::~GuiButton()
{
}

void GuiButton::SetImage(GuiImage* img)
{
	image = img;
	img->SetParent(this);
}
void GuiButton::SetImageOver(GuiImage* img)
{
	imageOver = img;
	img->SetParent(this);
}
void GuiButton::SetIcon(GuiImage* img)
{
	icon = img;
	img->SetParent(this);
}
void GuiButton::SetIconOver(GuiImage* img)
{
	iconOver = img;
	img->SetParent(this);
}
void GuiButton::SetLabel(GuiText* txt)
{
	label = txt;
	txt->SetParent(this);
}
void GuiButton::SetLabelOver(GuiText* txt)
{
	labelOver = txt;
	txt->SetParent(this);
}
void GuiButton::SetSoundOver(GuiSound * snd)
{
	soundOver = snd;
}
void GuiButton::SetSoundClick(GuiSound * snd)
{
	soundClick = snd;
}

/**
 * Draw the button on screen
 */
void GuiButton::Draw()
{
	if(!this->IsVisible())
		return;

	// draw image
	if(state == STATE_SELECTED && imageOver)
		imageOver->Draw();
	else if(image)
		image->Draw();
	// draw icon
	if(state == STATE_SELECTED && iconOver)
		iconOver->Draw();
	else if(icon)
		icon->Draw();
	// draw text
	if(state == STATE_SELECTED && labelOver)
		labelOver->Draw();
	else if(label)
		label->Draw();
}

void GuiButton::Update(GuiTrigger * t)
{
	if(state == STATE_CLICKED || state == STATE_DISABLED)
		return;

	// cursor
	if(t->wpad.ir.valid)
	{
		if(this->IsInside(t->wpad.ir.x, t->wpad.ir.y))
		{
			if(state == STATE_DEFAULT) // we weren't on the button before!
			{
				state = STATE_SELECTED;
				rumbleCount[t->chan] = 4;

				if(soundOver)
					soundOver->Play();
			}
		}
		else if(state == STATE_SELECTED)
		{
			state = STATE_DEFAULT;
		}
	}

	// button triggers
	if(this->IsClickable())
	{
		if(state == STATE_SELECTED && (trigger->chan == -1 || trigger->chan == t->chan))
		{
			// higher 16 bits only (wiimote)
			s32 wm_btns = t->wpad.btns_d << 16;
			s32 wm_btns_trig = trigger->wpad.btns_d << 16;

			// lower 16 bits only (classic controller)
			s32 cc_btns = t->wpad.btns_d >> 16;
			s32 cc_btns_trig = trigger->wpad.btns_d >> 16;

			if(wm_btns == wm_btns_trig ||
				(cc_btns == cc_btns_trig && t->wpad.exp.type == EXP_CLASSIC) ||
				t->pad.button == trigger->pad.button)
			{
				state = STATE_CLICKED;

				if(soundClick)
					soundClick->Play();
			}
		}
	}
}

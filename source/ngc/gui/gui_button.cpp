/****************************************************************************
 * libwiigui
 *
 * Tantric 2009
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

	for(int i=0; i < 3; i++)
	{
		label[i] = NULL;
		labelOver[i] = NULL;
	}

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

	if(img)
		img->SetParent(this);
}
void GuiButton::SetImageOver(GuiImage* img)
{
	imageOver = img;

	if(img)
		img->SetParent(this);
}
void GuiButton::SetIcon(GuiImage* img)
{
	icon = img;

	if(img)
		img->SetParent(this);
}
void GuiButton::SetIconOver(GuiImage* img)
{
	iconOver = img;

	if(img)
		img->SetParent(this);
}
void GuiButton::SetLabel(GuiText* txt)
{
	label[0] = txt;

	if(txt)
		txt->SetParent(this);
}
void GuiButton::SetLabelOver(GuiText* txt)
{
	labelOver[0] = txt;

	if(txt)
		txt->SetParent(this);
}
void GuiButton::SetLabel(GuiText* txt, int n)
{
	label[n] = txt;

	if(txt)
		txt->SetParent(this);
}
void GuiButton::SetLabelOver(GuiText* txt, int n)
{
	labelOver[n] = txt;

	if(txt)
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
	if((state == STATE_SELECTED || state == STATE_HELD) && imageOver)
		imageOver->Draw();
	else if(image)
		image->Draw();
	// draw icon
	if((state == STATE_SELECTED || state == STATE_HELD) && iconOver)
		iconOver->Draw();
	else if(icon)
		icon->Draw();
	// draw text
	for(int i=0; i<3; i++)
	{
		if((state == STATE_SELECTED || state == STATE_HELD) && labelOver[i])
			labelOver[i]->Draw();
		else if(label[i])
			label[i]->Draw();
	}

	this->UpdateEffects();
}

void GuiButton::Update(GuiTrigger * t)
{
	if(state == STATE_CLICKED || state == STATE_DISABLED || !t)
		return;
	else if(parentElement && parentElement->GetState() == STATE_DISABLED)
		return;

	#ifdef HW_RVL
	// cursor
	if(t->wpad.ir.valid)
	{
		if(this->IsInside(t->wpad.ir.x, t->wpad.ir.y))
		{
			if(state == STATE_DEFAULT) // we weren't on the button before!
			{
				state = STATE_SELECTED;

				if(this->Rumble())
					rumbleRequest[t->chan] = 1;

				if(soundOver)
					soundOver->Play();

				if(effectsOver && !effects)
				{
					// initiate effects
					effects = effectsOver;
					effectAmount = effectAmountOver;
					effectTarget = effectTargetOver;
				}
			}
		}
		else
		{
			if(state == STATE_SELECTED)
				state = STATE_DEFAULT;

			if(effectTarget == effectTargetOver && effectAmount == effectAmountOver)
			{
				// initiate effects (in reverse)
				effects = effectsOver;
				effectAmount = -effectAmountOver;
				effectTarget = 100;
			}
		}
	}
	#endif

	// button triggers
	if(this->IsClickable())
	{
		for(int i=0; i<2; i++)
		{
			if(trigger[i] && (trigger[i]->chan == -1 || trigger[i]->chan == t->chan))
			{
				// higher 16 bits only (wiimote)
				s32 wm_btns = t->wpad.btns_d << 16;
				s32 wm_btns_trig = trigger[i]->wpad.btns_d << 16;

				// lower 16 bits only (classic controller)
				s32 cc_btns = t->wpad.btns_d >> 16;
				s32 cc_btns_trig = trigger[i]->wpad.btns_d >> 16;

				if(
					(t->wpad.btns_d > 0 &&
					wm_btns == wm_btns_trig ||
					(cc_btns == cc_btns_trig && t->wpad.exp.type == EXP_CLASSIC)) ||
					(t->pad.btns_d == trigger[i]->pad.btns_d && t->pad.btns_d > 0))
				{
					if(state == STATE_SELECTED)
					{
						state = STATE_CLICKED;

						if(soundClick)
							soundClick->Play();
					}
					else if(trigger[i]->type == TRIGGER_BUTTON_ONLY)
					{
						state = STATE_CLICKED;
					}
					else if(trigger[i]->type == TRIGGER_BUTTON_ONLY_IN_FOCUS &&
							parentElement->IsFocused())
					{
						state = STATE_CLICKED;
					}
				}
			}
		}
	}

	if(this->IsDraggable())
	{
		bool held = false;

		for(int i=0; i<2; i++)
		{
			if(trigger[i] && (trigger[i]->chan == -1 || trigger[i]->chan == t->chan))
			{
				// higher 16 bits only (wiimote)
				s32 wm_btns = t->wpad.btns_h << 16;
				s32 wm_btns_trig = trigger[i]->wpad.btns_h << 16;

				// lower 16 bits only (classic controller)
				s32 cc_btns = t->wpad.btns_h >> 16;
				s32 cc_btns_trig = trigger[i]->wpad.btns_h >> 16;

				if(
					(t->wpad.btns_h > 0 &&
					wm_btns == wm_btns_trig ||
					(cc_btns == cc_btns_trig && t->wpad.exp.type == EXP_CLASSIC)) ||
					(t->pad.btns_h == trigger[i]->pad.btns_h && t->pad.btns_h > 0))
				{
					if(trigger[i]->type == TRIGGER_HELD)
						held = true;
				}

				if(!held && state == STATE_HELD && stateChan == t->chan)
				{
					state = STATE_DEFAULT;
				}
				else if(held && state == STATE_SELECTED)
				{
					state = STATE_HELD;
					stateChan = t->chan; // record which controller is holding the button
				}
			}
		}
	}

	if(updateCB)
		updateCB(this);
}

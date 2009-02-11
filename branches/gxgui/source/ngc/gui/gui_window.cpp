/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui_window.cpp
 *
 * GUI class definitions
 ***************************************************************************/

#include "gui.h"

GuiWindow::GuiWindow(int w, int h)
{
	width = w;
	height = h;
}

GuiWindow::~GuiWindow()
{
}

void GuiWindow::Append(GuiElement* layer)
{
	if (layer == NULL)
		return;

	Remove(layer);
	_elements.push_back(layer);
	layer->SetParent(this);
}

void GuiWindow::Insert(GuiElement* layer, u32 index)
{
	if (layer == NULL || index > (_elements.size() - 1))
		return;

	Remove(layer);
	_elements.insert(_elements.begin()+index, layer);
	layer->SetParent(this);
}

void GuiWindow::Remove(GuiElement* layer)
{
	if (layer == NULL)
		return;

	for (u8 i = 0; i < _elements.size(); i++)
	{
		if(layer == _elements.at(i))
		{
			_elements.erase(_elements.begin()+i);
			break;
		}
	}
}

void GuiWindow::RemoveAll()
{
	_elements.clear();
}

GuiElement* GuiWindow::GetGuiElementAt(u32 index) const
{
	if (index >= _elements.size())
		return NULL;
	return _elements.at(index);
}

u32 GuiWindow::GetSize()
{
	return _elements.size();
}

void GuiWindow::Draw()
{
	if(_elements.size() == 0 || !this->IsVisible())
		return;

	for (u8 i = 0; i < _elements.size(); i++)
	{
		try	{ _elements.at(i)->Draw(); }
		catch (exception& e) { }
	}

	if(parentElement && state == STATE_DISABLED)
		GRRLIB_Rectangle(0,0,screenwidth,screenheight,0x646464DD,1);
}

void GuiWindow::ResetState()
{
	state = STATE_DEFAULT;

	for (u8 i = 0; i < _elements.size(); i++)
	{
		try { _elements.at(i)->ResetState(); }
		catch (exception& e) { }
	}
}

void GuiWindow::SetState(int s)
{
	if(parentElement) // we don't want to change the state of our main window
		state = s;

	for (u8 i = 0; i < _elements.size(); i++)
	{
		try { _elements.at(i)->SetState(s); }
		catch (exception& e) { }
	}
}

int GuiWindow::GetSelected()
{
	// find selected element
	int found = -1;
	for (u8 i = 0; i < _elements.size(); i++)
	{
		try
		{
			if(_elements.at(i)->GetState() == STATE_SELECTED)
			{
				found = i;
				break;
			}
		}
		catch (exception& e) { }
	}
	return found;
}

// set element to left/right as selected
// there's probably a more clever way to do this, but this way works
void GuiWindow::MoveSelectionHor(int dir)
{
	int found = -1;
	u16 left = 0;
	u16 top = 0;
	u8 i = 0;

	int selected = this->GetSelected();

	if(selected >= 0)
	{
		left = _elements.at(selected)->GetLeft();
		top = _elements.at(selected)->GetTop();
		_elements.at(selected)->ResetState();
	}

	// look for a button on the same row, to the left/right
	for (i = 0; i < _elements.size(); i++)
	{
		try
		{
			if(_elements.at(i)->IsSelectable())
			{
				if(_elements.at(i)->GetLeft()*dir > left*dir && _elements.at(i)->GetTop() == top)
				{
					if(found == -1)
						found = i;
					else if(_elements.at(i)->GetLeft()*dir < _elements.at(found)->GetLeft()*dir)
						found = i; // this is a better match
				}
			}
		}
		catch (exception& e) { }
	}
	if(found >= 0)
		goto matchfound;

	// match still not found, let's try the first button in the next row
	for (i = 0; i < _elements.size(); i++)
	{
		try
		{
			if(_elements.at(i)->IsSelectable())
			{
				if(_elements.at(i)->GetTop()*dir > top*dir)
				{
					if(found == -1)
						found = i;
					else if(_elements.at(i)->GetTop()*dir < _elements.at(found)->GetTop()*dir)
						found = i; // this is a better match
					else if(_elements.at(i)->GetLeft()*dir < _elements.at(found)->GetLeft()*dir)
						found = i; // this is a better match
				}
			}
		}
		catch (exception& e) { }
	}
	if(found >= 0)
		goto matchfound;

	// match still not found, let's go to the first button in the first row
	for (i = 0; i < _elements.size(); i++)
	{
		try
		{
			if(_elements.at(i)->IsSelectable())
			{
				if(_elements.at(i)->GetTop()*dir <= top*dir)
				{
					if(found == -1)
						found = i;
					else if(_elements.at(i)->GetTop()*dir < _elements.at(found)->GetTop()*dir)
						found = i; // this is a better match
					else if(_elements.at(i)->GetLeft()*dir < _elements.at(found)->GetLeft()*dir)
						found = i; // this is a better match
				}
			}
		}
		catch (exception& e) { }
	}

	// match found
	matchfound:
	if(found >= 0)
		_elements.at(found)->SetState(STATE_SELECTED);
}

void GuiWindow::MoveSelectionVert(int dir)
{
	int found = -1;
	u16 left = 0;
	u16 top = 0;
	u8 i = 0;

	int selected = this->GetSelected();

	if(selected >= 0)
	{
		left = _elements.at(selected)->GetLeft();
		top = _elements.at(selected)->GetTop();
		_elements.at(selected)->ResetState();
	}

	// look for a button above/below, with the least horizontal difference
	for (i = 0; i < _elements.size(); i++)
	{
		try
		{
			if(_elements.at(i)->IsSelectable())
			{
				if(_elements.at(i)->GetTop()*dir > top*dir)
				{
					if(found == -1)
						found = i;
					else if(_elements.at(i)->GetTop()*dir < _elements.at(found)->GetTop()*dir)
						found = i; // this is a better match
					else if(abs(_elements.at(i)->GetLeft() - left) <
							abs(_elements.at(found)->GetLeft() - left))
						found = i;
				}
			}
		}
		catch (exception& e) { }
	}
	if(found >= 0)
		goto matchfound;

	// match still not found, let's go to the first/last row
	for (i = 0; i < _elements.size(); i++)
	{
		try
		{
			if(_elements.at(i)->IsSelectable())
			{
				if(_elements.at(i)->GetTop()*dir <= top*dir)
				{
					if(found == -1)
						found = i;
					else if(_elements.at(i)->GetTop()*dir < _elements.at(found)->GetTop()*dir)
						found = i; // this is a better match
					else if(_elements.at(i)->GetLeft()*dir < _elements.at(found)->GetLeft()*dir)
						found = i; // this is a better match
				}
			}
		}
		catch (exception& e) { }
	}

	// match found
	matchfound:
	if(found >= 0)
		_elements.at(found)->SetState(STATE_SELECTED);
}

void GuiWindow::Update(GuiTrigger * t)
{
	if(_elements.size() == 0 ||	(state == STATE_DISABLED && parentElement))
		return;

	for (u8 i = 0; i < _elements.size(); i++)
	{
		try	{ _elements.at(i)->Update(t); }
		catch (exception& e) { }
	}

	// pad/joystick navigation
	if(t->wpad.btns_d & (WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT)
		|| t->pad.button & PAD_BUTTON_RIGHT)
		this->MoveSelectionHor(1);
	else if(t->wpad.btns_d & (WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT)
		|| t->pad.button & PAD_BUTTON_LEFT)
		this->MoveSelectionHor(-1);
	else if(t->wpad.btns_d & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)
		|| t->pad.button & PAD_BUTTON_DOWN)
		this->MoveSelectionVert(1);
	else if(t->wpad.btns_d & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)
		|| t->pad.button & PAD_BUTTON_UP)
		this->MoveSelectionVert(-1);
}

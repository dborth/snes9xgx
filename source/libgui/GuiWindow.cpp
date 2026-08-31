/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiWindow.cpp
 ***************************************************************************/

#include "Gui.h"

GuiWindow::GuiWindow()
{
	width = 0;
	height = 0;
	focus = 0; // allow focus
}

GuiWindow::GuiWindow(int w, int h)
{
	width = w;
	height = h;
	focus = 0; // allow focus
}

GuiWindow::~GuiWindow()
{
	// Orphan all children if the window is destroyed first to prevent dangling pointer
	for (GuiElement* e : _elements) {
		e->setParent(nullptr);
	}
}

void GuiWindow::append(GuiElement* e)
{
	if (e == nullptr)
		return;

	remove(e);
	_elements.push_back(e);
	e->setParent(this);
}

void GuiWindow::appendWithAutoRemove(GuiElement* e)
{
    if (e == nullptr) return;
    append(e);
    e->setRemoveOnDestroy(true);
}

void GuiWindow::insert(GuiElement* e, uint32_t index)
{
	if (e == nullptr || index > (_elements.size() - 1))
		return;

	remove(e);
	_elements.insert(_elements.begin()+index, e);
	e->setParent(this);
}

void GuiWindow::remove(GuiElement* e)
{
	if (e == nullptr)
		return;

	uint32_t elemSize = _elements.size();
	for (uint32_t i = 0; i < elemSize; ++i)
	{
		if(e == _elements.at(i))
		{
			e->setParent(nullptr);
			_elements.erase(_elements.begin()+i);
			break;
		}
	}
}

void GuiWindow::removeAll()
{
	uint32_t elemSize = _elements.size();
	for (uint32_t i = 0; i < elemSize; ++i)
	{
		_elements.at(i)->setParent(nullptr);
	}
	_elements.clear();
}

bool GuiWindow::find(GuiElement* e)
{
	if (e == nullptr)
		return false;

	uint32_t elemSize = _elements.size();
	for (uint32_t i = 0; i < elemSize; ++i)
		if(e == _elements.at(i))
			return true;
	return false;
}

GuiElement* GuiWindow::getGuiElementAt(uint32_t index) const
{
	if (index >= _elements.size())
		return nullptr;
	return _elements.at(index);
}

uint32_t GuiWindow::getSize()
{
	return _elements.size();
}

void GuiWindow::draw()
{
	if(_elements.size() == 0 || !this->isVisible())
		return;

	uint32_t elemSize = _elements.size();
	for (uint32_t i = 0; i < elemSize; ++i)
	{
		_elements.at(i)->draw();
	}

	this->updateEffects();

	if(parentElement && state == STATE::DISABLED)
		platform->getVideo()->getImageRenderer()->drawRectangle(0,0,platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight(), (PixelColor){0xbe, 0xca, 0xd5, 0x70});
}

void GuiWindow::resetState()
{
	if(state != STATE::DISABLED)
		state = STATE::DEFAULT;

	uint32_t elemSize = _elements.size();
	for (uint32_t i = 0; i < elemSize; ++i)
	{
		_elements.at(i)->resetState();
	}
}

void GuiWindow::setState(STATE s, int c)
{
	state = s;

	uint32_t elemSize = _elements.size();
	for (uint32_t i = 0; i < elemSize; ++i)
	{
		_elements.at(i)->setState(s, c);
	}
}

void GuiWindow::setVisible(bool v)
{
	visible = v;

	uint32_t elemSize = _elements.size();
	for (uint32_t i = 0; i < elemSize; ++i)
	{
		_elements.at(i)->setVisible(v);
	}
}

void GuiWindow::setFocus(int f)
{
	focus = f;

	if(f == 1)
		this->moveSelectionVert(1);
	else
		this->resetState();
}

void GuiWindow::changeFocus(GuiElement* e)
{
	if(parentElement)
		return; // this is only intended for the main window

	uint32_t elemSize = _elements.size();
	for (uint32_t i = 0; i < elemSize; ++i)
	{
		if(e == _elements.at(i))
			_elements.at(i)->setFocus(1);
		else if(_elements.at(i)->isFocused() == 1)
			_elements.at(i)->setFocus(0);
	}
}

void GuiWindow::toggleFocus(GuiInputController * controller)
{
	if(parentElement)
		return; // this is only intended for the main window

	int found = -1;
	int newfocus = -1;
	int i;

	int elemSize = _elements.size();

	// look for currently in focus element
	for (i = 0; i < elemSize; ++i)
	{
		if(_elements.at(i)->isFocused() == 1)
		{
			found = i;
			break;
		}
	}

	// element with focus not found, try to give focus
	if(found == -1)
	{
		for (i = 0; i < elemSize; ++i)
		{
			if(_elements.at(i)->isFocused() == 0 && _elements.at(i)->getState() != STATE::DISABLED) // focus is possible (but not set)
			{
				_elements.at(i)->setFocus(1); // give this element focus
				break;
			}
		}
	}

	// change focus
	else if(controller->isSecondaryPressed())
	{
		for (i = found; i < elemSize; ++i)
		{
			if(_elements.at(i)->isFocused() == 0 && _elements.at(i)->getState() != STATE::DISABLED) // focus is possible (but not set)
			{
				newfocus = i;
				_elements.at(i)->setFocus(1); // give this element focus
				_elements.at(found)->setFocus(0); // disable focus on other element
				break;
			}
		}

		if(newfocus == -1)
		{
			for (i = 0; i < found; ++i)
			{
				if(_elements.at(i)->isFocused() == 0 && _elements.at(i)->getState() != STATE::DISABLED) // focus is possible (but not set)
				{
					_elements.at(i)->setFocus(1); // give this element focus
					_elements.at(found)->setFocus(0); // disable focus on other element
					break;
				}
			}
		}
	}
}

int GuiWindow::getSelected()
{
	// find selected element
	int found = -1;
	uint32_t elemSize = _elements.size();
	for (uint32_t i = 0; i < elemSize; ++i)
	{
		if(_elements.at(i)->getState() == STATE::SELECTED)
		{
			found = int(i);
			break;
		}
	}
	return found;
}

// set element to left/right as selected
// there's probably a more clever way to do this, but this way works
void GuiWindow::moveSelectionHor(int dir)
{
	int found = -1;
	uint16_t left = 0;
	uint16_t top = 0;
	uint32_t i;
	uint32_t elemSize = _elements.size();

	int selected = this->getSelected();

	if(selected >= 0)
	{
		left = _elements.at(selected)->getLeft();
		top = _elements.at(selected)->getTop();
	}

	
	// look for a button on the same row, to the left/right
	for (i = 0; i < elemSize; ++i)
	{
		if(_elements.at(i)->isSelectable())
		{
			if(_elements.at(i)->getLeft()*dir > left*dir && _elements.at(i)->getTop() == top)
			{
				if(found == -1)
					found = int(i);
				else if(_elements.at(i)->getLeft()*dir < _elements.at(found)->getLeft()*dir)
					found = int(i); // this is a better match
			}
		}
	}
	if(found >= 0)
		goto matchfound;

	// match still not found, let's try the first button in the next row
	for (i = 0; i < elemSize; ++i)
	{
		if(_elements.at(i)->isSelectable())
		{
			if(_elements.at(i)->getTop()*dir > top*dir)
			{
				if(found == -1)
					found = i;
				else if(_elements.at(i)->getTop()*dir < _elements.at(found)->getTop()*dir)
					found = i; // this is a better match
				else if(_elements.at(i)->getTop()*dir == _elements.at(found)->getTop()*dir
					&&
					_elements.at(i)->getLeft()*dir < _elements.at(found)->getLeft()*dir)
					found = i; // this is a better match
			}
		}
	}

	// match found
	matchfound:
	if(found >= 0)
	{
		_elements.at(found)->setState(STATE::SELECTED);
		if(selected >= 0)
			_elements.at(selected)->resetState();
	}
}

void GuiWindow::moveSelectionVert(int dir)
{
	int found = -1;
	uint16_t left = 0;
	uint16_t top = 0;

	int selected = this->getSelected();

	if(selected >= 0)
	{
		left = _elements.at(selected)->getLeft();
		top = _elements.at(selected)->getTop();
	}

	// look for a button above/below, with the least horizontal difference
	uint32_t elemSize = _elements.size();
	for (uint32_t i = 0; i < elemSize; ++i)
	{
		if(_elements.at(i)->isSelectable())
		{
			if(_elements.at(i)->getTop()*dir > top*dir)
			{
				if(found == -1)
					found = i;
				else if(_elements.at(i)->getTop()*dir < _elements.at(found)->getTop()*dir)
					found = i; // this is a better match
				else if(_elements.at(i)->getTop()*dir == _elements.at(found)->getTop()*dir
						&&
						abs(_elements.at(i)->getLeft() - left) <
						abs(_elements.at(found)->getLeft() - left))
					found = i;
			}
		}
	}
	if(found >= 0)
		goto matchfound;

	// match found
	matchfound:
	if(found >= 0)
	{
		_elements.at(found)->setState(STATE::SELECTED);
		if(selected >= 0)
			_elements.at(selected)->resetState();
	}
}

void GuiWindow::resetText()
{
	uint32_t elemSize = _elements.size();
	for (uint32_t i = 0; i < elemSize; i++)
	{
		_elements.at(i)->resetText();
	}
}

void GuiWindow::update(GuiInputController * controller)
{
	if(_elements.size() == 0 || (state == STATE::DISABLED && parentElement))
		return;

	uint32_t elemSize = _elements.size();
	for (uint32_t i = 0; i < elemSize; ++i)
	{
		_elements.at(i)->update(controller);
	}

	this->toggleFocus(controller);

	if(focus) // only send actions to this window if it's in focus
	{
		// pad/joystick navigation
		if(controller->right())
			this->moveSelectionHor(1);
		else if(controller->left())
			this->moveSelectionHor(-1);
		else if(controller->down())
			this->moveSelectionVert(1);
		else if(controller->up())
			this->moveSelectionVert(-1);
	}

	if(updateCB)
		updateCB(this);
}

/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiLayout.cpp
 ***************************************************************************/

#include "Gui.h"
#include <cmath>

static int roundi(float v)
{
	return (int)floorf(v + 0.5f);
}

GuiLayout::GuiLayout() :
	spacingX(0.0f), spacingY(0.0f), justify(LAYOUT_JUSTIFY::START), alignItems(LAYOUT_ALIGN::START),
	userFixedW(0), userFixedH(0), assignedW(0), assignedH(0), naturalW(0), naturalH(0), dirty(true), settled(false)
{
}

GuiLayout& GuiLayout::add(GuiElement* e, const GuiItem& item)
{
	if(!e)
		return *this;

	this->append(e); // calls remove(e) first, which drops any earlier entry for it
	Entry en;
	en.e = e;
	en.item = item;
	entries.push_back(en);
	dirty = true;
	return *this;
}

GuiLayout& GuiLayout::addGap(float size)
{
	Entry en;
	en.gapSize = size < 0.0f ? 0.0f : size;
	entries.push_back(en);
	dirty = true;
	return *this;
}

GuiLayout& GuiLayout::addSpace(float weight)
{
	Entry en;
	en.item.grow(weight);
	entries.push_back(en);
	dirty = true;
	return *this;
}

GuiLayout& GuiLayout::setPadding(float all)
{
	return setPadding(all, all, all, all);
}

GuiLayout& GuiLayout::setPadding(float horizontal, float vertical)
{
	return setPadding(horizontal, vertical, horizontal, vertical);
}

GuiLayout& GuiLayout::setPadding(float left, float top, float right, float bottom)
{
	padding = GuiInsets(left, top, right, bottom);
	dirty = true;
	return *this;
}

GuiLayout& GuiLayout::setSpacing(float s)
{
	return setSpacing(s, s);
}

GuiLayout& GuiLayout::setSpacing(float x, float y)
{
	spacingX = x;
	spacingY = y;
	dirty = true;
	return *this;
}

GuiLayout& GuiLayout::setJustify(LAYOUT_JUSTIFY j)
{
	justify = j;
	dirty = true;
	return *this;
}

GuiLayout& GuiLayout::setAlignItems(LAYOUT_ALIGN a)
{
	alignItems = a;
	dirty = true;
	return *this;
}

GuiLayout& GuiLayout::setFixedSize(int w, int h)
{
	userFixedW = w < 0 ? 0 : w;
	userFixedH = h < 0 ? 0 : h;
	dirty = true;
	return *this;
}

void GuiLayout::invalidate()
{
	dirty = true;
}

void GuiLayout::clear()
{
	this->removeAll();
	entries.clear();
	dirty = true;
}

void GuiLayout::remove(GuiElement* e)
{
	GuiWindow::remove(e);
	for(size_t i = 0; i < entries.size(); i++)
	{
		if(entries[i].e == e)
		{
			entries.erase(entries.begin() + i);
			break;
		}
	}
	dirty = true;
}

//Entries follow the window's children: pick up children added with append() directly,
//and drop entries whose child was removed some other way
void GuiLayout::syncEntries()
{
	// Nearly always the entries are exactly the children, in order (gaps aside): confirm that in one
	// pass and skip the searches below, which compare every entry with every child
	size_t next = 0;
	bool inSync = true;
	for(const Entry& en : entries)
	{
		if(!en.e)
			continue;
		if(next >= _elements.size() || _elements[next] != en.e)
		{
			inSync = false;
			break;
		}
		next++;
	}
	if(inSync && next == _elements.size())
		return;

	for(size_t i = 0; i < entries.size();)
	{
		if(entries[i].e && !this->find(entries[i].e))
			entries.erase(entries.begin() + i), dirty = true;
		else
			i++;
	}
	for(GuiElement* child : _elements)
	{
		bool known = false;
		for(const Entry& en : entries)
			if(en.e == child) { known = true; break; }
		if(!known)
		{
			Entry en;
			en.e = child;
			entries.push_back(en);
			dirty = true;
		}
	}
}

void GuiLayout::layout()
{
	settled = true;

	// nested layouts settle first, so their sizes are right when measured below
	for(Entry& en : entries)
		if(en.e)
			en.e->layout();
	this->syncEntries();

	std::vector<Entry*>& visible = visibleScratch;
	visible.clear();
	bool changed = dirty;
	for(Entry& en : entries)
	{
		const bool shown = !en.e || en.e->isVisible();
		if(shown != en.lastVisible)
		{
			changed = true;
			en.lastVisible = shown;
		}
		if(!shown)
			continue;

		if(en.e)
		{
			int w, h;
			en.e->getNaturalSize(w, h);
			if(en.haveAssigned && w == en.assignedW && h == en.assignedH)
			{
				// still the size layout gave it: its natural size is what it was before
				w = en.baseW;
				h = en.baseH;
			}
			else
			{
				en.baseW = w;
				en.baseH = h;
				en.haveAssigned = false;
			}
			if(w != en.lastW || h != en.lastH)
			{
				changed = true;
				en.lastW = w;
				en.lastH = h;
			}
			en.natW = w;
			en.natH = h;
		}
		visible.push_back(&en);
	}
	if(!changed)
		return;
	dirty = false;

	GuiLayoutResult fit = this->solve(visible, (float)userFixedW, (float)userFixedH);
	naturalW = roundi(fit.width);
	naturalH = roundi(fit.height);

	GuiLayoutResult res = fit;
	if(assignedW > 0 || assignedH > 0)
		res = this->solve(visible, (float)(assignedW > 0 ? assignedW : userFixedW), (float)(assignedH > 0 ? assignedH : userFixedH));
	width = roundi(res.width);
	height = roundi(res.height);

	for(size_t i = 0; i < visible.size() && i < res.rects.size(); i++)
	{
		Entry& en = *visible[i];
		if(!en.e)
			continue;

		const GuiRect& r = res.rects[i];
		const int w = roundi(r.w), h = roundi(r.h);
		if(w != en.natW || h != en.natH)
		{
			en.e->setLayoutSize(w, h);
			en.assignedW = w;
			en.assignedH = h;
			en.haveAssigned = true;
			en.e->layout(); // a nested layout re-flows to its new size right away
		}
		else if(en.haveAssigned)
		{
			en.e->setLayoutSize(en.baseW, en.baseH);
			en.haveAssigned = false;
			en.e->layout();
		}
		en.e->setLayoutPosition(roundi(r.x), roundi(r.y));
	}
}

// A layout inside another layout is settled by its parent's layout() on the way down, which comes
// just before the parent updates or draws it. Repeating that here would re-measure everything below
// it once more for every level of nesting.
void GuiLayout::draw()
{
	if(!settled)
		this->layout();
	GuiWindow::draw();
	settled = false;
}

void GuiLayout::update(InputController * c)
{
	if(!settled)
		this->layout();
	// A hidden layout is not drawn, so it must not take input either: its buttons would still answer the pointer
	// (and any press) from where they sit under whatever is showing in their place.
	if(this->isVisible())
		GuiWindow::update(c);
	settled = false;
}

void GuiLayout::getNaturalSize(int& w, int& h)
{
	w = naturalW;
	h = naturalH;
}

void GuiLayout::setLayoutSize(int w, int h)
{
	assignedW = w < 0 ? 0 : w;
	assignedH = h < 0 ? 0 : h;
	width = assignedW;
	height = assignedH;
	dirty = true;
}

void GuiLayout::collectNavigable(std::vector<GuiElement*>& out)
{
	if(!this->isVisible())
		return;	// nothing in a hidden layout can take the D-pad's selection
	for(GuiElement* child : _elements)
		child->collectNavigable(out);
}

GuiRow::GuiRow(int w, int h)
{
	setFixedSize(w, h);
}

GuiLayoutResult GuiRow::solve(const std::vector<Entry*>& visible, float fixedW, float fixedH)
{
	GuiStackSpec spec;
	spec.vertical = false;
	spec.padding = padding;
	spec.spacing = spacingX;
	spec.fixedW = fixedW;
	spec.fixedH = fixedH;
	spec.justify = justify;
	spec.alignItems = alignItems;

	std::vector<GuiStackItem> items(visible.size());
	for(size_t i = 0; i < visible.size(); i++)
	{
		const Entry& en = *visible[i];
		if(en.e) { items[i].w = (float)en.natW; items[i].h = (float)en.natH; }
		else items[i].w = en.gapSize;
		items[i].grow = en.item.growWeight;
		items[i].useContainerAlign = !en.item.hasAlignV;
		items[i].align = en.item.alignV;
	}
	return GuiSolveStack(spec, items);
}

GuiColumn::GuiColumn(int w, int h)
{
	setFixedSize(w, h);
}

GuiLayoutResult GuiColumn::solve(const std::vector<Entry*>& visible, float fixedW, float fixedH)
{
	GuiStackSpec spec;
	spec.vertical = true;
	spec.padding = padding;
	spec.spacing = spacingY;
	spec.fixedW = fixedW;
	spec.fixedH = fixedH;
	spec.justify = justify;
	spec.alignItems = alignItems;

	std::vector<GuiStackItem> items(visible.size());
	for(size_t i = 0; i < visible.size(); i++)
	{
		const Entry& en = *visible[i];
		if(en.e) { items[i].w = (float)en.natW; items[i].h = (float)en.natH; }
		else items[i].h = en.gapSize;
		items[i].grow = en.item.growWeight;
		items[i].useContainerAlign = !en.item.hasAlignH;
		items[i].align = en.item.alignH;
	}
	return GuiSolveStack(spec, items);
}

GuiGrid::GuiGrid(int cols, int w, int h) : columns(cols < 1 ? 1 : cols)
{
	setFixedSize(w, h);
}

GuiLayoutResult GuiGrid::solve(const std::vector<Entry*>& visible, float fixedW, float fixedH)
{
	GuiGridSpec spec;
	spec.columns = columns;
	spec.padding = padding;
	spec.spacingX = spacingX;
	spec.spacingY = spacingY;
	spec.fixedW = fixedW;
	spec.fixedH = fixedH;

	std::vector<GuiGridItem> items(visible.size());
	for(size_t i = 0; i < visible.size(); i++)
	{
		const Entry& en = *visible[i];
		if(en.e) { items[i].w = (float)en.natW; items[i].h = (float)en.natH; }
		else items[i].w = items[i].h = en.gapSize; // a gap holds a cell open
		if(en.item.hasAlignH) items[i].alignH = en.item.alignH;
		if(en.item.hasAlignV) items[i].alignV = en.item.alignV;
	}
	return GuiSolveGrid(spec, items);
}

GuiOverlay::GuiOverlay(int w, int h)
{
	setFixedSize(w, h);
}

GuiLayoutResult GuiOverlay::solve(const std::vector<Entry*>& visible, float fixedW, float fixedH)
{
	GuiOverlaySpec spec;
	spec.padding = padding;
	spec.fixedW = fixedW;
	spec.fixedH = fixedH;

	std::vector<GuiOverlayItem> items;
	for(size_t i = 0; i < visible.size(); i++)
	{
		const Entry& en = *visible[i];
		GuiOverlayItem it;
		it.w = (float)en.natW;
		it.h = (float)en.natH;
		it.background = en.item.isBackground;
		if(en.item.hasAlignH) it.alignH = en.item.alignH;
		if(en.item.hasAlignV) it.alignV = en.item.alignV;
		items.push_back(it);
	}
	return GuiSolveOverlay(spec, items);
}

/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiLayoutSolver.cpp
 ***************************************************************************/

#include "GuiLayoutSolver.h"
#include <algorithm>
#include <cmath>

namespace {

float placeAcross(LAYOUT_ALIGN a, float start, float space, float size)
{
	switch(a)
	{
		case LAYOUT_ALIGN::CENTRE: return start + (space - size) * 0.5f;
		case LAYOUT_ALIGN::END:    return start + space - size;
		default:                   return start;
	}
}

} // namespace

GuiLayoutResult GuiSolveStack(const GuiStackSpec& spec, const std::vector<GuiStackItem>& items)
{
	GuiLayoutResult out;
	const size_t n = items.size();
	out.rects.assign(n, GuiRect{ 0, 0, 0, 0 });

	const bool v = spec.vertical;
	const float padMainStart = v ? spec.padding.top : spec.padding.left;
	const float padMainEnd = v ? spec.padding.bottom : spec.padding.right;
	const float padCrossStart = v ? spec.padding.left : spec.padding.top;
	const float padCrossEnd = v ? spec.padding.right : spec.padding.bottom;
	const float fixedMain = v ? spec.fixedH : spec.fixedW;
	const float fixedCross = v ? spec.fixedW : spec.fixedH;

	float sumMain = 0.0f, maxCross = 0.0f, growSum = 0.0f;
	for(size_t i = 0; i < n; i++)
	{
		sumMain += v ? items[i].h : items[i].w;
		maxCross = std::max(maxCross, v ? items[i].w : items[i].h);
		growSum += std::max(items[i].grow, 0.0f);
	}
	if(n > 1)
		sumMain += spec.spacing * (float)(n - 1);

	const float totalMain = fixedMain > 0.0f ? fixedMain : sumMain + padMainStart + padMainEnd;
	const float totalCross = fixedCross > 0.0f ? fixedCross : maxCross + padCrossStart + padCrossEnd;
	out.width = v ? totalCross : totalMain;
	out.height = v ? totalMain : totalCross;

	const float innerCross = std::max(0.0f, totalCross - padCrossStart - padCrossEnd);
	float freeMain = totalMain - padMainStart - padMainEnd - sumMain;

	float offset = 0.0f, extraGap = 0.0f;
	std::vector<float> mainSize(n);
	for(size_t i = 0; i < n; i++)
		mainSize[i] = v ? items[i].h : items[i].w;

	if(freeMain > 0.0f)
	{
		if(growSum > 0.0f)
		{
			for(size_t i = 0; i < n; i++)
				mainSize[i] += freeMain * std::max(items[i].grow, 0.0f) / growSum;
		}
		else
		{
			switch(spec.justify)
			{
				case LAYOUT_JUSTIFY::CENTRE: offset = freeMain * 0.5f; break;
				case LAYOUT_JUSTIFY::END: offset = freeMain; break;
				case LAYOUT_JUSTIFY::SPACE_BETWEEN: if(n > 1) extraGap = freeMain / (float)(n - 1); break;
				default: break;
			}
		}
	}

	float pos = padMainStart + offset;
	for(size_t i = 0; i < n; i++)
	{
		const LAYOUT_ALIGN align = items[i].useContainerAlign ? spec.alignItems : items[i].align;
		float cross = v ? items[i].w : items[i].h;
		if(align == LAYOUT_ALIGN::STRETCH)
			cross = innerCross;
		const float crossPos = placeAcross(align, padCrossStart, innerCross, cross);

		GuiRect r;
		if(v) { r.x = crossPos; r.y = pos; r.w = cross; r.h = mainSize[i]; }
		else  { r.x = pos; r.y = crossPos; r.w = mainSize[i]; r.h = cross; }
		out.rects[i] = r;
		pos += mainSize[i] + spec.spacing + extraGap;
	}
	return out;
}

GuiLayoutResult GuiSolveGrid(const GuiGridSpec& spec, const std::vector<GuiGridItem>& items)
{
	GuiLayoutResult out;
	const int cols = std::max(1, spec.columns);
	const size_t n = items.size();
	out.rects.assign(n, GuiRect{ 0, 0, 0, 0 });
	const int rows = n == 0 ? 0 : (int)((n + cols - 1) / cols);

	std::vector<float> colW((size_t)cols, 0.0f), rowH((size_t)std::max(rows, 0), 0.0f);
	for(size_t i = 0; i < n; i++)
	{
		const size_t c = i % cols, r = i / cols;
		colW[c] = std::max(colW[c], items[i].w);
		rowH[r] = std::max(rowH[r], items[i].h);
	}

	const float padH = spec.padding.left + spec.padding.right;
	const float padV = spec.padding.top + spec.padding.bottom;
	if(spec.fixedW > 0.0f)
	{
		const float each = std::max(0.0f, (spec.fixedW - padH - spec.spacingX * (float)(cols - 1)) / (float)cols);
		for(int c = 0; c < cols; c++)
			colW[(size_t)c] = each;
	}

	float sumW = 0.0f, sumH = 0.0f;
	for(int c = 0; c < cols; c++)
		sumW += colW[(size_t)c];
	for(int r = 0; r < rows; r++)
		sumH += rowH[(size_t)r];
	sumW += spec.spacingX * (float)(cols - 1);
	if(rows > 1)
		sumH += spec.spacingY * (float)(rows - 1);

	out.width = spec.fixedW > 0.0f ? spec.fixedW : sumW + padH;
	out.height = spec.fixedH > 0.0f ? spec.fixedH : sumH + padV;

	std::vector<float> colX((size_t)cols), rowY((size_t)std::max(rows, 0));
	float x = spec.padding.left;
	for(int c = 0; c < cols; c++)
	{
		colX[(size_t)c] = x;
		x += colW[(size_t)c] + spec.spacingX;
	}
	float y = spec.padding.top;
	for(int r = 0; r < rows; r++)
	{
		rowY[(size_t)r] = y;
		y += rowH[(size_t)r] + spec.spacingY;
	}

	for(size_t i = 0; i < n; i++)
	{
		const size_t c = i % cols, r = i / cols;
		float w = items[i].alignH == LAYOUT_ALIGN::STRETCH ? colW[c] : items[i].w;
		float h = items[i].alignV == LAYOUT_ALIGN::STRETCH ? rowH[r] : items[i].h;
		GuiRect rect;
		rect.x = placeAcross(items[i].alignH, colX[c], colW[c], w);
		rect.y = placeAcross(items[i].alignV, rowY[r], rowH[r], h);
		rect.w = w;
		rect.h = h;
		out.rects[i] = rect;
	}
	return out;
}

GuiLayoutResult GuiSolveOverlay(const GuiOverlaySpec& spec, const std::vector<GuiOverlayItem>& items)
{
	GuiLayoutResult out;
	const size_t n = items.size();
	out.rects.assign(n, GuiRect{ 0, 0, 0, 0 });

	float maxW = 0.0f, maxH = 0.0f;
	for(size_t i = 0; i < n; i++)
	{
		if(items[i].background)
			continue;
		maxW = std::max(maxW, items[i].w);
		maxH = std::max(maxH, items[i].h);
	}
	const float padH = spec.padding.left + spec.padding.right;
	const float padV = spec.padding.top + spec.padding.bottom;
	out.width = spec.fixedW > 0.0f ? spec.fixedW : maxW + padH;
	out.height = spec.fixedH > 0.0f ? spec.fixedH : maxH + padV;

	const float innerW = std::max(0.0f, out.width - padH);
	const float innerH = std::max(0.0f, out.height - padV);
	for(size_t i = 0; i < n; i++)
	{
		if(items[i].background)
		{
			out.rects[i] = GuiRect{ 0, 0, out.width, out.height };
			continue;
		}
		const float w = items[i].alignH == LAYOUT_ALIGN::STRETCH ? innerW : items[i].w;
		const float h = items[i].alignV == LAYOUT_ALIGN::STRETCH ? innerH : items[i].h;
		out.rects[i] = GuiRect{ placeAcross(items[i].alignH, spec.padding.left, innerW, w), placeAcross(items[i].alignV, spec.padding.top, innerH, h), w, h };
	}
	return out;
}

namespace {

struct Cand {
	int index;
	float cx, cy;
};

void centreOf(const GuiRect& r, float& cx, float& cy)
{
	cx = r.x + r.w * 0.5f;
	cy = r.y + r.h * 0.5f;
}

//Length by which [a0,a1] and [b0,b1] overlap, or minus the gap between them
float overlapOrGap(float a0, float a1, float b0, float b1)
{
	return std::min(a1, b1) - std::max(a0, b0);
}

} // namespace

int GuiNavigate(const std::vector<GuiNavItem>& items, int current, NAV_DIR dir)
{
	const int n = (int)items.size();
	const bool horizontal = dir == NAV_DIR::LEFT || dir == NAV_DIR::RIGHT;
	const float sign = (dir == NAV_DIR::RIGHT || dir == NAV_DIR::DOWN) ? 1.0f : -1.0f;

	// nothing selected: the first selectable item in reading order
	if(current < 0 || current >= n)
	{
		int best = -1;
		float bx = 0, by = 0;
		for(int i = 0; i < n; i++)
		{
			if(!items[i].selectable)
				continue;
			float cx, cy;
			centreOf(items[i].r, cx, cy);
			if(best < 0 || cy < by - 1.0f || (fabsf(cy - by) <= 1.0f && cx < bx))
			{
				best = i;
				bx = cx;
				by = cy;
			}
		}
		return best;
	}

	float ccx, ccy;
	centreOf(items[current].r, ccx, ccy);
	const GuiRect& cur = items[current].r;

	// nearest item in the direction, preferring those that line up with the current one
	int best = -1;
	float bestScore = 0.0f;
	for(int i = 0; i < n; i++)
	{
		if(i == current || !items[i].selectable)
			continue;
		float cx, cy;
		centreOf(items[i].r, cx, cy);
		const float along = horizontal ? (cx - ccx) * sign : (cy - ccy) * sign;
		if(along <= 1.0f)
			continue;
		const GuiRect& r = items[i].r;
		const float ov = horizontal ? overlapOrGap(cur.y, cur.y + cur.h, r.y, r.y + r.h) : overlapOrGap(cur.x, cur.x + cur.w, r.x, r.x + r.w);
		const float across = ov > 0.0f ? 0.0f : -ov;
		const float offAxis = fabsf(horizontal ? cy - ccy : cx - ccx);
		const float score = along + 4.0f * across + 0.01f * offAxis;
		if(best < 0 || score < bestScore)
		{
			best = i;
			bestScore = score;
		}
	}
	if(best >= 0 || !horizontal)
		return best;

	// off the end of a row: continue on the next row (right) or the previous row (left)
	for(int i = 0; i < n; i++)
	{
		if(i == current || !items[i].selectable)
			continue;
		float cx, cy;
		centreOf(items[i].r, cx, cy);
		const bool otherRow = sign > 0 ? (cy > cur.y + cur.h) : (cy < cur.y);
		if(!otherRow)
			continue;
		if(best < 0)
		{
			best = i;
			continue;
		}
		float bx, by;
		centreOf(items[best].r, bx, by);
		const bool closerRow = sign > 0 ? (cy < by - 1.0f) : (cy > by + 1.0f);
		const bool sameRow = fabsf(cy - by) <= 1.0f;
		const bool betterInRow = sign > 0 ? (cx < bx) : (cx > bx);
		if(closerRow || (sameRow && betterInRow))
			best = i;
	}
	return best;
}

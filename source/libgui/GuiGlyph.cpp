/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiGlyph.cpp
 ***************************************************************************/

#include "Gui.h"

GuiGlyph::GuiGlyph(GLYPH k, const char * letter) : kind(k), text(nullptr), meshFeather(-1.0f)
{
	float w, h;
	GuiGlyph::size(kind, w, h);
	width = (int)w;
	height = (int)h;

	const bool takesLetter = kind == GLYPH::BUTTON || kind == GLYPH::BUMPER || kind == GLYPH::STICK;
	if(takesLetter && letter && *letter)
	{
		const GuiTheme& theme = GuiTheme::current();
		const int px = (int)strlen(letter) > 1 ? theme.glyphFontSizeSmall : theme.glyphFontSize;
		text = new GuiText(letter, px, theme.textInverse);
		text->setParent(this);
		text->setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
		// capitals and digits have no descenders, so the font's line box centers them high; this puts the same
		// padding above the letter as below it
		// (the bumper pill is shorter than the round glyphs, so its single letter takes one less)
		text->setNudgeY(kind == GLYPH::BUMPER && px == theme.glyphFontSize ? 1 : 2);
	}
}

GuiGlyph::~GuiGlyph()
{
	delete text;
}

void GuiGlyph::draw()
{
	if(!this->isVisible())
		return;

	const int a = this->getAlpha();
	if(a > 0)
	{
		VideoDriver * video = platform->getVideo();
		const float uiScale = video->getUIScale();
		const float feather = uiScale > 0.0f ? 1.0f / uiScale : 1.0f;
		if(feather != meshFeather)
		{
			mesh = GuiMesh(feather);
			GuiGlyph::build(mesh, kind, (float)width, (float)height);
			meshFeather = feather;
		}

		const float ox = (float)this->getLeft(), oy = (float)this->getTop();
		const std::vector<MeshVertex>& src = mesh.vertices();
		scratch.resize(src.size());
		for(size_t i = 0; i < src.size(); i++)
		{
			scratch[i] = src[i];
			scratch[i].x += ox;
			scratch[i].y += oy;
			if(a < 255)
				scratch[i].c.a = (uint8_t)((src[i].c.a * a + 127) / 255);
		}
		if(!scratch.empty())
			video->getImageRenderer()->drawMesh(scratch.data(), scratch.size());

		if(text)
			text->draw();
	}

	this->updateEffects();
}


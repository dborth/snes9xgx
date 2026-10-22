/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiUi.cpp
 ***************************************************************************/

#include "Gui.h"
#include <cstdarg>
#include <cstdio>

std::string GuiFormat(const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	va_list copy;
	va_copy(copy, args);
	const int n = vsnprintf(nullptr, 0, fmt, copy);
	va_end(copy);

	std::string out;
	if(n > 0)
	{
		out.resize((size_t)n + 1);
		vsnprintf(&out[0], out.size(), fmt, args);
		out.resize((size_t)n);
	}
	va_end(args);
	return out;
}

// An empty string is no text at all: GuiText skips drawing and the translator is not asked for "".
static const char * textOrNull(const char * t)
{
	return (t && *t) ? t : nullptr;
}

// Shared by the widgets that wrap a GuiButton: report focus when the button becomes selected, run the
// click callback, and keep the button selected afterwards so the D-pad does not lose its place.
static void pollButton(GuiButton& b, bool& wasSelected, const UiCallback& click, const UiCallback& focus)
{
	const STATE s = b.getState();
	const bool selected = s == STATE::SELECTED || s == STATE::HELD;
	if(selected && !wasSelected && focus)
		focus();
	wasSelected = selected;

	if(s == STATE::CLICKED && click)
	{
		const int chan = b.getStateChan();
		click();
		b.resetState();
		b.setState(STATE::SELECTED, chan);
		wasSelected = true;
	}
}

// ---- UiScope

class UiScope::Runner : public GuiElement
{
	public:
		Runner(UiScope& s) : scope(s) {}
		void update(InputController *) override { scope.runBindings(); }
		void draw() override {}
	private:
		UiScope& scope;
};

UiScope::UiScope() : trig(&primary), soundOver(GuiSound::getDefaultOver()), soundClick(GuiSound::getDefaultClick()), runner(nullptr)
{
	primary.setPrimaryTrigger();
}

UiScope::~UiScope()
{
	bindings.clear(); // nothing runs against elements that are about to go
	for(size_t i = holders.size(); i > 0; i--)
		delete holders[i - 1];
}

void UiScope::bind(UiCallback fn)
{
	if(!fn)
		return;
	fn(); // evaluate once now, so a first layout already sees the values
	bindings.push_back(fn);
}

void UiScope::visibleWhen(GuiElement& e, std::function<bool()> fn)
{
	GuiElement * target = &e;
	bind([target, fn]() {
		const bool v = fn();
		if(v != target->isVisible())
			target->setVisible(v);
	});
}

void UiScope::enabledWhen(GuiElement& e, std::function<bool()> fn)
{
	GuiElement * target = &e;
	bind([target, fn]() {
		const bool enabled = fn();
		const bool off = target->getState() == STATE::DISABLED;
		if(enabled && off)
			target->setState(STATE::DEFAULT);
		else if(!enabled && !off)
			target->setState(STATE::DISABLED);
	});
}

void UiScope::attachTo(GuiWindow& window)
{
	if(!runner)
		runner = &make<Runner>(*this);
	runner->setVisible(false);
	window.append(runner);
}

void UiScope::runBindings()
{
	for(size_t i = 0; i < bindings.size(); i++)
		bindings[i]();
}

// ---- GuiLabel

GuiLabel::GuiLabel(UiScope& scope, const char * t) :
	GuiText(textOrNull(t), 20, GuiTheme::current().text), ui(scope)
{
}

GuiLabel& GuiLabel::fontSize(int px)
{
	setFontSize(px);
	return *this;
}

GuiLabel& GuiLabel::textColor(PixelColor c)
{
	setColor(c);
	return *this;
}

GuiLabel& GuiLabel::muted()
{
	return textColor(GuiTheme::current().textMuted);
}

GuiLabel& GuiLabel::accent()
{
	return textColor(GuiTheme::current().accent);
}

GuiLabel& GuiLabel::bind(std::function<std::string()> fn)
{
	GuiLabel * self = this;
	std::string last = "\x01"; // matches no real text, so the first evaluation always sets it
	ui.bind([self, fn, last]() mutable {
		const std::string s = fn();
		if(s != last)
		{
			self->setText(textOrNull(s.c_str()));
			last = s;
		}
	});
	return *this;
}

GuiLabel& GuiLabel::visibleWhen(std::function<bool()> fn)
{
	ui.visibleWhen(*this, fn);
	return *this;
}

// ---- GuiChip

GuiChip::GuiChip(UiScope& scope, const char * t) :
	ui(scope), backdrop(1, 1), label(scope, t)
{
	const GuiTheme& theme = GuiTheme::current();

	backdrop.setCapsule();
	backdrop.setStyle(theme.chipStyle());
	label.fontSize(16);

	setPadding(10, 3);
	GuiLayout::add(&backdrop, GuiItem().background());
	GuiLayout::add(&label);
}

GuiChip& GuiChip::bind(std::function<std::string()> fn)
{
	label.bind(fn);
	return *this;
}

GuiChip& GuiChip::visibleWhen(std::function<bool()> fn)
{
	ui.visibleWhen(*this, fn);
	return *this;
}

// ---- GuiTextButton

GuiTextButton::GuiTextButton(UiScope& scope, const char * t) :
	GuiButton(150, 44), ui(scope), shape(150, 44), text(scope, t), textLit(scope, t), wasSelected(false)
{
	const GuiTheme& theme = GuiTheme::current();

	shape.setCapsule();
	shape.setStyle(theme.buttonStyle());
	textLit.textColor(theme.textOnAccent);
	text.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
	textLit.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);

	setImage(&shape);
	setLabel(&text);
	setLabelOver(&textLit);
	setTrigger(scope.trigger());
	setSoundOver(scope.getSoundOver());
	setSoundClick(scope.getSoundClick());
	setEffectGrow();
}

GuiTextButton& GuiTextButton::size(int w, int h)
{
	width = w;
	height = h;
	shape.setLayoutSize(w, h);
	return *this;
}

GuiTextButton& GuiTextButton::fontSize(int px)
{
	text.fontSize(px);
	textLit.fontSize(px);
	return *this;
}

void GuiTextButton::setLayoutSize(int w, int h)
{
	GuiButton::setLayoutSize(w, h);
	shape.setLayoutSize(w, h);
}

GuiTextButton& GuiTextButton::onClick(UiCallback fn)
{
	clickCb = fn;
	return *this;
}

GuiTextButton& GuiTextButton::onFocus(UiCallback fn)
{
	focusCb = fn;
	return *this;
}

GuiTextButton& GuiTextButton::style(const GuiShapeStyle& s)
{
	shape.setStyle(s);
	return *this;
}

GuiTextButton& GuiTextButton::rounded(float radius)
{
	shape.setCorners(GuiCorners(radius));
	return *this;
}

GuiTextButton& GuiTextButton::disabled(bool d)
{
	if(d)
		setState(STATE::DISABLED);
	else if(state == STATE::DISABLED)
		setState(STATE::DEFAULT);
	return *this;
}

void GuiTextButton::setText(const char * t)
{
	text.setText(textOrNull(t));
	textLit.setText(textOrNull(t));
}

GuiTextButton& GuiTextButton::bind(std::function<std::string()> fn)
{
	text.bind(fn);
	textLit.bind(fn);
	return *this;
}

GuiTextButton& GuiTextButton::visibleWhen(std::function<bool()> fn)
{
	ui.visibleWhen(*this, fn);
	return *this;
}

GuiTextButton& GuiTextButton::enabledWhen(std::function<bool()> fn)
{
	ui.enabledWhen(*this, fn);
	return *this;
}

void GuiTextButton::update(InputController * c)
{
	GuiButton::update(c);
	pollButton(*this, wasSelected, clickCb, focusCb);
}

// ---- GuiCard

GuiCardSurface::GuiCardSurface() : GuiButton(1, 1), shape(1, 1)
{
	setImage(&shape);
}

void GuiCardSurface::setLayoutSize(int w, int h)
{
	GuiButton::setLayoutSize(w, h);
	shape.setLayoutSize(w, h);
}

GuiCard::GuiCard(UiScope& scope) : ui(scope), wasSelected(false)
{
	const GuiTheme& theme = GuiTheme::current();

	surface.shape.setCorners(GuiCorners(theme.radius));
	surface.shape.setStyle(theme.cardStyle());
	surface.setTrigger(scope.trigger());
	surface.setSoundOver(scope.getSoundOver());
	surface.setSoundClick(scope.getSoundClick());

	setPadding(theme.padding);
	GuiLayout::add(&surface, GuiItem().background());
}

GuiCard& GuiCard::onClick(UiCallback fn)
{
	clickCb = fn;
	return *this;
}

GuiCard& GuiCard::onFocus(UiCallback fn)
{
	focusCb = fn;
	return *this;
}

GuiCard& GuiCard::style(const GuiShapeStyle& s)
{
	surface.shape.setStyle(s);
	return *this;
}

GuiCard& GuiCard::corners(const GuiCorners& c)
{
	surface.shape.setCorners(c);
	return *this;
}

GuiCard& GuiCard::selectable(bool s)
{
	surface.setSelectable(s);
	// an inert surface ignores the pointer too, so a plain panel never lights up
	if(!s)
		surface.setState(STATE::DISABLED);
	else if(surface.getState() == STATE::DISABLED)
		surface.setState(STATE::DEFAULT);
	return *this;
}

GuiCard& GuiCard::disabled(bool d)
{
	// only the surface is disabled: a disabled GuiWindow would dim the whole screen
	const bool off = surface.getState() == STATE::DISABLED;
	if(d && !off)
		surface.setState(STATE::DISABLED);
	else if(!d && off)
		surface.setState(STATE::DEFAULT);
	return *this;
}

GuiCard& GuiCard::visibleWhen(std::function<bool()> fn)
{
	ui.visibleWhen(*this, fn);
	return *this;
}

GuiCard& GuiCard::enabledWhen(std::function<bool()> fn)
{
	GuiCard * self = this;
	ui.bind([self, fn]() { self->disabled(!fn()); });
	return *this;
}

void GuiCard::update(InputController * c)
{
	GuiOverlay::update(c);
	pollButton(surface, wasSelected, clickCb, focusCb);
}

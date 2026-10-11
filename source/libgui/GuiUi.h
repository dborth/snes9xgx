/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiUi.h
 *
 * The builder layer. A UiScope creates and owns a page's elements, so a screen
 * is built by describing it: no `new`, no `delete`, no helper structs holding
 * the pieces. Widgets here (GuiLabel, GuiChip, GuiTextButton, GuiCard) take
 * their look from GuiTheme, draw with no image assets, and report clicks and
 * focus through callbacks. Text, visibility and enabled state can be bound to
 * a function that is re-evaluated every frame.
 *
 *   UiScope ui;
 *   ui.column().gap(8)
 *     .add(ui.label("Controllers").fontSize(28))
 *     .add(ui.card().onClick([&]{ OpenPlayer(1); })
 *            .add(ui.label().bind([&]{ return DeviceName(1); })));
 *   ui.attachTo(page);
 *
 * Everything the scope made is destroyed with it, children before parents
 * or the other way round; the elements unhook themselves.
 ***************************************************************************/
#pragma once

//!A callback with no arguments.
//!\ingroup grp_builder
typedef std::function<void()> UiCallback;

//!Formats like printf and returns a std::string, for bound text:
//!`ui.label().bind([&]{ return GuiFormat("battery %d%%", pct); })`
//!\ingroup grp_builder
#if defined(__GNUC__)
std::string GuiFormat(const char * fmt, ...) __attribute__((format(printf, 1, 2)));
#else
std::string GuiFormat(const char * fmt, ...);
#endif

class UiScope;

//!A line of text styled from the theme. Setters chain, and `bind()` keeps the text up to date.
//!\ingroup grp_builder
class GuiLabel : public GuiText
{
	public:
		GuiLabel(UiScope& scope, const char * text = "");
		//!Sets the font size
		GuiLabel& fontSize(int px);
		//!Sets the text color
		GuiLabel& textColor(PixelColor c);
		//!Uses the theme's quiet text color
		GuiLabel& muted();
		//!Uses the theme's accent color
		GuiLabel& accent();
		//!Keeps the text equal to what `fn` returns, checked every frame (the text is only
		//!rebuilt when the string actually changes)
		GuiLabel& bind(std::function<std::string()> fn);
		//!Shows the label only while `fn` returns true. Hidden elements take no room in a layout.
		GuiLabel& visibleWhen(std::function<bool()> fn);
	private:
		UiScope& ui;
};

//!A small rounded label background with text on it.
//!\ingroup grp_builder
class GuiChip : public GuiOverlay
{
	public:
		GuiChip(UiScope& scope, const char * text = "");
		//!Sets the text
		void setText(const char * t) { label.setText(t); }
		//!Keeps the text equal to what `fn` returns, checked every frame
		GuiChip& bind(std::function<std::string()> fn);
		GuiChip& visibleWhen(std::function<bool()> fn);
	private:
		UiScope& ui;
		GuiShape backdrop;
		GuiLabel label;
};

//!A button with no image assets: a capsule shape that lights up when focused,
//!with a centred label. Hover, D-pad focus, pressed and disabled looks come from the theme.
//!A button with an `onClick` callback handles its own clicks; without one, poll
//!`getState() == STATE::CLICKED` as with any GuiButton.
//!\ingroup grp_builder
class GuiTextButton : public GuiButton
{
	public:
		GuiTextButton(UiScope& scope, const char * text);
		//!Sets the size; the label stays centred
		GuiTextButton& size(int w, int h);
		//!Called when the button is clicked. The button stays selected afterwards.
		GuiTextButton& onClick(UiCallback fn);
		//!Called when the button becomes selected (by the pointer or the D-pad)
		GuiTextButton& onFocus(UiCallback fn);
		//!Replaces the look (default: GuiTheme::buttonStyle())
		GuiTextButton& style(const GuiShapeStyle& s);
		//!Squarer corners instead of a capsule
		GuiTextButton& rounded(float radius);
		//!Disables or enables the button (a disabled button cannot be selected or clicked)
		GuiTextButton& disabled(bool d = true);
		//!Sets the text
		void setText(const char * t);
		//!Keeps the text equal to what `fn` returns, checked every frame
		GuiTextButton& bind(std::function<std::string()> fn);
		//!Shows the button only while `fn` returns true
		GuiTextButton& visibleWhen(std::function<bool()> fn);
		//!Enables the button only while `fn` returns true
		GuiTextButton& enabledWhen(std::function<bool()> fn);

		void update(InputController * c) override;
		void setLayoutSize(int w, int h) override;
	private:
		UiScope& ui;
		GuiShape shape;
		GuiLabel text;
		GuiLabel textLit;
		UiCallback clickCb;
		UiCallback focusCb;
		bool wasSelected;
};

//!The clickable, focusable surface behind a GuiCard (a button with a shape for an image).
//!\ingroup grp_builder
class GuiCardSurface : public GuiButton
{
	public:
		GuiCardSurface();
		void setLayoutSize(int w, int h) override;
		GuiShape shape;
};

//!A container with a styled backdrop that can take the D-pad's selection and be clicked,
//!for tiles such as a player card. Add content as to any overlay. The backdrop restyles itself
//!on focus, press and disable (default: GuiTheme::cardStyle()). Call the card's own methods
//!(`size`, `pad`, `onClick`, ...) before the generic layout ones, which return a GuiLayout&.
//!\ingroup grp_builder
class GuiCard : public GuiOverlay
{
	public:
		GuiCard(UiScope& scope);
		//!Called when the card is clicked. The card stays selected afterwards.
		GuiCard& onClick(UiCallback fn);
		//!Called when the card becomes selected
		GuiCard& onFocus(UiCallback fn);
		//!Replaces the backdrop's look
		GuiCard& style(const GuiShapeStyle& s);
		//!Squarer or rounder corners (default: the theme's radius)
		GuiCard& corners(const GuiCorners& c);
		//!Whether the D-pad and pointer can select the card (false for a plain panel)
		GuiCard& selectable(bool s);
		//!Disables or enables the card
		GuiCard& disabled(bool d = true);
		//!Shows the card only while `fn` returns true
		GuiCard& visibleWhen(std::function<bool()> fn);
		//!Enables the card only while `fn` returns true
		GuiCard& enabledWhen(std::function<bool()> fn);
		//!Same as the GuiLayout setters, but returning the card so calls can continue
		GuiCard& size(int w, int h) { setFixedSize(w, h); return *this; }
		GuiCard& pad(float all) { setPadding(all); return *this; }
		GuiCard& pad(float horizontal, float vertical) { setPadding(horizontal, vertical); return *this; }
		GuiCard& add(GuiElement& e, const GuiItem& item = GuiItem()) { GuiLayout::add(&e, item); return *this; }
		GuiCard& add(GuiElement * e, const GuiItem& item = GuiItem()) { GuiLayout::add(e, item); return *this; }

		void update(InputController * c) override;
	private:
		UiScope& ui;
		GuiCardSurface surface;
		UiCallback clickCb;
		UiCallback focusCb;
		bool wasSelected;
};

//!Creates and owns the elements of one page, runs their bindings, and supplies the
//!defaults (trigger, sounds, theme) the widgets use. Create it on the page's stack
//!before the window it will be attached to.
//!\ingroup grp_builder
class UiScope
{
	public:
		UiScope();
		~UiScope();
		UiScope(const UiScope&) = delete;
		UiScope& operator=(const UiScope&) = delete;

		//!Creates a GuiElement (or subclass) owned by the scope
		//!\param args Constructor arguments
		template<class T, class... Args>
		T& make(Args&&... args)
		{
			Held<T> * h = new Held<T>(std::forward<Args>(args)...);
			holders.push_back(h);
			h->value.setRemoveOnDestroy(true); // unhooks from its parent if destroyed first
			return h->value;
		}
		//!Creates any other object (a GuiImageData, a GuiSound, ...) that should live as long as the scope
		template<class T, class... Args>
		T& own(Args&&... args)
		{
			Held<T> * h = new Held<T>(std::forward<Args>(args)...);
			holders.push_back(h);
			return h->value;
		}

		GuiRow& row() { return make<GuiRow>(); }
		GuiColumn& column() { return make<GuiColumn>(); }
		GuiGrid& grid(int columns) { return make<GuiGrid>(columns); }
		GuiOverlay& overlay() { return make<GuiOverlay>(); }
		GuiLabel& label(const char * text = "") { return make<GuiLabel>(*this, text); }
		GuiChip& chip(const char * text = "") { return make<GuiChip>(*this, text); }
		GuiTextButton& button(const char * text) { return make<GuiTextButton>(*this, text); }
		GuiCard& card() { return make<GuiCard>(*this); }
		GuiGlyph& glyph(GLYPH kind, const char * letter = "") { return make<GuiGlyph>(kind, letter); }

		//!Runs `fn` once per frame, before drawing
		void bind(UiCallback fn);
		//!Shows `e` only while `fn` returns true
		void visibleWhen(GuiElement& e, std::function<bool()> fn);
		//!Disables `e` while `fn` returns false (for buttons and cards)
		void enabledWhen(GuiElement& e, std::function<bool()> fn);

		//!The theme widgets read (GuiTheme::current())
		const GuiTheme& theme() const { return GuiTheme::current(); }
		//!Sets the trigger buttons and cards click with (default: the primary trigger, A)
		void setTrigger(GuiTrigger * t) { trig = t ? t : &primary; }
		GuiTrigger * trigger() { return trig; }
		//!Sets the sounds buttons and cards play; null for none. Applies to widgets made afterwards.
		void setSounds(GuiSound * over, GuiSound * click) { soundOver = over; soundClick = click; }
		GuiSound * getSoundOver() { return soundOver; }
		GuiSound * getSoundClick() { return soundClick; }

		//!Adds the scope's (invisible) binding runner to a window, so bindings run as it updates.
		//!Call once, on the window the page's elements are in.
		void attachTo(GuiWindow& window);
		//!Runs the bindings once now (done every frame once attached)
		void runBindings();

	private:
		struct Holder { virtual ~Holder() {} };
		template<class T>
		struct Held : Holder
		{
			template<class... A> Held(A&&... a) : value(std::forward<A>(a)...) {}
			T value;
		};
		class Runner;

		std::vector<Holder*> holders;
		std::vector<UiCallback> bindings;
		GuiTrigger primary;
		GuiTrigger * trig;
		GuiSound * soundOver;
		GuiSound * soundClick;
		Runner * runner;
};

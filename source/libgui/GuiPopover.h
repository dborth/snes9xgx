/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiPopover.h
 *
 * A question asked in place: a panel that opens beside the control that asked
 * it, over the screen it came from.
 ***************************************************************************/
#pragma once

//!A panel that opens beside the control that asked, over the screen it came from, with a tail that points at the
//!control. It is a full-screen overlay: it places the panel (place()), draws the tail, and decides when the popover
//!is dismissed. It closes on B from any controller, or on a click from any controller whose pointer is outside the
//!panel; a click on the panel is the panel's own. What is on the panel, and how it looks, is up to the caller.
//!
//!A popover does not run itself. Add it on top of the screen it came from (disabling that screen so it dims and
//!stops listening), keep updating the window, and watch isDismissed(). To keep it open after all (a B that only
//!stopped something inside it), call reopen().
//!
//!Derive from it, say where it opens and what to keep clear in the constructor, build the panel in `ui`, and hand
//!it over with setPanel(), which places the panel and attaches `ui`:
//!\code
//!class Ask : public GuiPopover
//!{
//!	public:
//!		Ask(GuiElement& button) : GuiPopover(Anchor(button), Insets(60, 60, 20))
//!		{
//!			setPanel(ui.card().selectable(false).size(280, 160).add(ui.label("Sure?")));
//!		}
//!};
//!\endcode
//!The placement arithmetic (place(), tailAt(), maxHeight()) is static and needs no popover, so it can be tested on a
//!host or used to draw a preview.
//!\ingroup grp_layout
class GuiPopover : public GuiOverlay
{
	public:
		//!The control a popover opens beside, in canvas pixels.
		struct Anchor
		{
			//!Which way the panel lines up: with the anchor's nearer edge (AUTO), or against a side margin to leave the other side of the screen clear
			enum Edge { AUTO, LEFT_MARGIN, RIGHT_MARGIN };
			int x, y, w, h;
			bool valid;		//!< false for a popover with no anchor: it is centered
			Edge edge;
			//!No anchor: the popover is centered
			Anchor() : x(0), y(0), w(0), h(0), valid(false), edge(AUTO) {}
			//!The popover opens beside the rectangle `ax`,`ay`,`aw`,`ah`
			Anchor(int ax, int ay, int aw, int ah, Edge e = AUTO) : x(ax), y(ay), w(aw), h(ah), valid(true), edge(e) {}
			//!The popover opens beside `control`, where it is now
			explicit Anchor(GuiElement& control, Edge e = AUTO) : x(control.getLeft()), y(control.getTop()), w(control.getWidth()), h(control.getHeight()), valid(true), edge(e) {}
		};

		//!What a popover keeps clear at the screen's edges: the bars at the top and bottom, and the margin at both sides.
		//!The default is none: the whole screen is available.
		struct Insets
		{
			int top, bottom, side;
			Insets(int t = 0, int b = 0, int s = 0) : top(t), bottom(b), side(s) {}
		};

		//!Where the panel's top-left corner goes.
		struct Position { int x, y; };

		//!Which way the tail points.
		enum class TAIL { UP, DOWN, LEFT, RIGHT };

		//!The little tail on the panel: `x`,`y` is its top-left corner, `w`,`h` its size, `direction` the way it points (toward the anchor).
		struct Tail { bool shown; TAIL direction; int x, y, w, h; };

		//!A popover as large as the screen the platform reports, opening beside `anchor` (centered with none) and
		//!keeping `insets` clear
		explicit GuiPopover(const Anchor& anchor = Anchor(), const Insets& insets = Insets());
		//!The same over a screen of `screenWidth` x `screenHeight`
		GuiPopover(const Anchor& anchor, const Insets& insets, int screenWidth, int screenHeight);

		//!Puts the built `panel` (a card owned by `ui`, fixed or fit to its content: it is laid out here to measure it) beside the anchor, points the tail at it in
		//!`tailColor` (the panel's face), and attaches `ui`. Call once, after the panel is built. The default tail
		//!color is the theme's surfaceRaised.
		void setPanel(GuiCard& panel, PixelColor tailColor);
		void setPanel(GuiCard& panel);

		//!The most height a panel can have here: the band between the bars. Use it to size a panel before building it.
		int maxPanelHeight() const { return maxHeight(screenH, insets); }
		//!Where the panel went (its top-left corner, in canvas pixels). Valid after setPanel().
		const Position& position() const { return placed; }
		//!The tail drawn on the panel; `shown` is false with no anchor or when the panel covers it. Valid after setPanel().
		const Tail& tail() const { return placedTail; }

		//!Dismisses the popover as B does (a close button on the panel calls this)
		void dismiss();
		//!Whether B, a click outside the panel or dismiss() has asked for it to close
		bool isDismissed() const { return dismissed; }
		//!The controller channel that dismissed it, or -1 for dismiss() or while it is open
		int dismissedBy() const { return by; }
		//!Keeps it open after all: clears the dismissal
		void reopen() { dismissed = false; by = -1; }
		//!Updates the panel first (a click on it is its own), then watches for the dismissal
		void update(InputController * c) override;

		//!The most height a popover can have: the band between the bars.
		static int maxHeight(int screenHeight, const Insets& insets);
		//!Places a panel of `w` x `h` beside its anchor: below it when that fits above the bottom bar, else above it,
		//!and failing both inside the band between the bars. A panel whose anchor is in a bar comes straight out of it,
		//!so the bars bind only an anchor that is in the band. Horizontally it lines up with the anchor's right edge when
		//!the anchor is on the right half of the screen and its left edge otherwise (or against a side margin, per the
		//!anchor's edge), and always stays inside the side margins. With no anchor it is centered.
		static Position place(const Anchor& anchor, int w, int h, int screenWidth, int screenHeight, const Insets& insets);
		//!Where the tail goes for a panel of `panelW` x `panelH` at `at`: on the side facing the anchor, level with the
		//!anchor's middle but kept clear of the panel's corners. Not shown when the panel covers the anchor or there is none.
		static Tail tailAt(const Anchor& anchor, const Position& at, int panelW, int panelH);

	protected:
		UiScope ui;	//!< Owns the panel and everything else the popover is made of; build the panel with it
	private:
		Anchor anchor;
		Insets insets;
		int screenW, screenH;
		GuiTrigger * cancel;	// B
		GuiTrigger * click;		// the primary press, for a click outside the panel
		GuiElement * panel;
		Position placed;
		Tail placedTail;
		bool dismissed;
		int by;
};

/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiLayout.h
 *
 * Layout containers: GuiRow, GuiColumn, GuiGrid and GuiOverlay. They are
 * GuiWindows that position and size their children from rules instead of
 * pixel offsets. Drawing, input, focus and triggers are the ordinary GuiWindow
 * ones; a container only decides where its children go. The D-pad moves the
 * selection through nested containers by on-screen position.
 *
 * Sizes are in design-canvas pixels. A container's width or height of 0 means
 * "fit the content" on that axis. Layout re-runs by itself when a child's size,
 * visibility or the container's rules change, so a label whose text changes
 * simply reflows.
 ***************************************************************************/
#pragma once

//!Per-child options passed to GuiLayout::add(). Setters chain:
//!`row.add(&label, GuiItem().grow(1))`
//!\ingroup grp_layout
struct GuiItem {
	float growWeight;		/*!< Share of leftover space along a stack's main axis (0 = none). */
	bool hasAlignH;			/*!< Horizontal alignment was set. */
	bool hasAlignV;			/*!< Vertical alignment was set. */
	LAYOUT_ALIGN alignH;
	LAYOUT_ALIGN alignV;
	bool isBackground;		/*!< Overlay only: fills the whole box (see background()). */

	GuiItem() : growWeight(0), hasAlignH(false), hasAlignV(false),
		alignH(LAYOUT_ALIGN::START), alignV(LAYOUT_ALIGN::START), isBackground(false) {}

	//!Takes a share of the space left over along a row's width or a column's height.
	//!Items with weights 1 and 2 split it one third / two thirds.
	GuiItem& grow(float weight = 1.0f) { growWeight = weight; return *this; }
	//!Fills the available space across a stack, or in a grid cell or overlay
	GuiItem& stretch() { return at(LAYOUT_ALIGN::STRETCH, LAYOUT_ALIGN::STRETCH); }
	//!Top / left
	GuiItem& alignStart() { return at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START); }
	//!Centered
	GuiItem& alignCentre() { return at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE); }
	//!Bottom / right
	GuiItem& alignEnd() { return at(LAYOUT_ALIGN::END, LAYOUT_ALIGN::END); }
	//!Alignment on both axes. A row uses the vertical one, a column the horizontal one;
	//!a grid cell and an overlay use both.
	GuiItem& at(LAYOUT_ALIGN h, LAYOUT_ALIGN v) { alignH = h; alignV = v; hasAlignH = hasAlignV = true; return *this; }
	//!Overlay only: this child is a backdrop. It fills the whole box, ignores the padding,
	//!and does not count toward the size the box fits to. Add it first so it is drawn first.
	GuiItem& background() { isBackground = true; return *this; }
};

//!Base of the layout containers. Not used directly; see GuiRow, GuiColumn, GuiGrid, GuiOverlay.
//!\ingroup grp_layout
class GuiLayout : public GuiWindow
{
	public:
		//!Adds a child. Children are laid out in the order they are added.
		//!\param e The element
		//!\param item Options for this child
		GuiLayout& add(GuiElement* e, const GuiItem& item = GuiItem());
		//!Adds a child by reference, so a builder can pass it straight in:
		//!`row.add(ui.label("Hi"))`
		GuiLayout& add(GuiElement& e, const GuiItem& item = GuiItem()) { return add(&e, item); }
		//!Adds a fixed gap along a row's width or a column's height
		GuiLayout& addGap(float size);
		//!Adds flexible empty space that takes a share of what is left over,
		//!for example to push one child to each end of a row
		GuiLayout& addSpace(float weight = 1.0f);
		//!Sets the space between the container's edge and its children
		GuiLayout& setPadding(float all);
		GuiLayout& setPadding(float horizontal, float vertical);
		GuiLayout& setPadding(float left, float top, float right, float bottom);
		//!Sets the gap between neighbouring children (a grid uses both numbers)
		GuiLayout& setSpacing(float s);
		GuiLayout& setSpacing(float x, float y);
		//!Sets how leftover space along the main axis is spent when no child grows
		GuiLayout& setJustify(LAYOUT_JUSTIFY j);
		//!Sets the default alignment of children across the main axis
		GuiLayout& setAlignItems(LAYOUT_ALIGN a);
		//!Fixes the container's size (0 on an axis = fit the content)
		GuiLayout& setFixedSize(int w, int h);
		//!Short names for the setters above, for reading a layout top to bottom:
		//!`ui.row().size(568, 0).gap(8).align(LAYOUT_ALIGN::CENTRE).add(...)`
		GuiLayout& size(int w, int h) { return setFixedSize(w, h); }
		GuiLayout& gap(float s) { return setSpacing(s); }
		GuiLayout& gap(float x, float y) { return setSpacing(x, y); }
		GuiLayout& pad(float all) { return setPadding(all); }
		GuiLayout& pad(float horizontal, float vertical) { return setPadding(horizontal, vertical); }
		GuiLayout& pad(float left, float top, float right, float bottom) { return setPadding(left, top, right, bottom); }
		GuiLayout& align(LAYOUT_ALIGN a) { return setAlignItems(a); }
		GuiLayout& justifyContent(LAYOUT_JUSTIFY j) { return setJustify(j); }
		//!Forces layout to run again
		void invalidate();
		//!Removes every child
		void clear();

		void remove(GuiElement* e) override;
		void layout() override;
		void draw() override;
		void update(InputController * c) override;
		void getNaturalSize(int& w, int& h) override;
		void setLayoutSize(int w, int h) override;
		void collectNavigable(std::vector<GuiElement*>& out) override;

	protected:
		//!A child (or gap) and what layout knows about it
		struct Entry {
			GuiElement * e;			//!< The child; null for a gap or flexible space
			GuiItem item;
			float gapSize;			//!< Fixed size of a gap
			int natW, natH;			//!< Natural size, as measured this pass
			int baseW, baseH;		//!< Natural size before layout resized it
			int assignedW, assignedH;//!< Size layout last gave it
			bool haveAssigned;
			int lastW, lastH;		//!< Natural size at the last layout, to spot changes
			bool lastVisible;
			Entry() : e(nullptr), gapSize(0), natW(0), natH(0), baseW(0), baseH(0), assignedW(0), assignedH(0),
				haveAssigned(false), lastW(-1), lastH(-1), lastVisible(true) {}
		};

		GuiLayout();
		//!Works out the container size and each visible entry's rectangle (what a subclass does)
		//!\param visible The entries to place, in order
		//!\param fixedW Container width (0 = fit)
		//!\param fixedH Container height (0 = fit)
		virtual GuiLayoutResult solve(const std::vector<Entry*>& visible, float fixedW, float fixedH) = 0;

		GuiInsets padding;
		float spacingX;
		float spacingY;
		LAYOUT_JUSTIFY justify;
		LAYOUT_ALIGN alignItems;

	private:
		void syncEntries();

		std::vector<Entry> entries;
		std::vector<Entry*> visibleScratch;	//!< layout() works in this, so a pass allocates nothing once it has grown
		int userFixedW, userFixedH;
		int assignedW, assignedH;
		int naturalW, naturalH;
		bool dirty;
		bool settled;						//!< layout() has run since this was last updated or drawn, so doing it again is wasted work
};

//!Lays children out left to right.
//!\ingroup grp_layout
class GuiRow : public GuiLayout
{
	public:
		//!\param w Width (0 = fit the content)
		//!\param h Height (0 = fit the content)
		GuiRow(int w = 0, int h = 0);
	protected:
		GuiLayoutResult solve(const std::vector<Entry*>& visible, float fixedW, float fixedH) override;
};

//!Lays children out top to bottom.
//!\ingroup grp_layout
class GuiColumn : public GuiLayout
{
	public:
		//!\param w Width (0 = fit the content)
		//!\param h Height (0 = fit the content)
		GuiColumn(int w = 0, int h = 0);
	protected:
		GuiLayoutResult solve(const std::vector<Entry*>& visible, float fixedW, float fixedH) override;
};

//!Lays children out in rows of a set number of columns.
//!\ingroup grp_layout
class GuiGrid : public GuiLayout
{
	public:
		//!\param columns Number of columns
		//!\param w Width (0 = fit; a fixed width makes the columns equal)
		//!\param h Height (0 = fit)
		GuiGrid(int columns, int w = 0, int h = 0);
	protected:
		GuiLayoutResult solve(const std::vector<Entry*>& visible, float fixedW, float fixedH) override;
	private:
		int columns;
};

//!Stacks children in one box, each aligned inside it (centered by default).
//!Typical use: a backdrop shape behind some content, or a badge on a corner.
//!\ingroup grp_layout
class GuiOverlay : public GuiLayout
{
	public:
		//!\param w Width (0 = fit the largest child)
		//!\param h Height (0 = fit the largest child)
		GuiOverlay(int w = 0, int h = 0);
	protected:
		GuiLayoutResult solve(const std::vector<Entry*>& visible, float fixedW, float fixedH) override;
};

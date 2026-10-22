/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * controllerview.h
 *
 * Drawing a controller from a ControllerLayout: the geometry for each state
 * (BuildController*), and ControllerView, the element that shows it and what
 * is happening on it.
 ***************************************************************************/
#pragma once

#include "libgui/Gui.h"
#include "libgui/GuiPaint.h"
#include "libgui/GuiTheme.h"
#include "controllerlayout.h"

// ---- the drawing: how a controller and its controls look in each state (pure geometry: no elements, no platform)

//!The look a control is drawn in.
enum class CTL_STATE {
	NORMAL,
	LIT,				//!< Pressed
	HIGHLIGHT,			//!< Being edited / pointed at
	LIT_HIGHLIGHT
};

//!Builds the resting drawing of a controller (silhouette, then every decorative control) in local
//!coordinates, with the layout multiplied by `scale`. The silhouette's outline is made by drawing each
//!piece's outline first and the fills after, so overlapping pieces merge into one shape.
void BuildControllerBody(GuiMesh& mesh, const ControllerLayout& layout, float scale);

//!Builds one control in one state (an analog stick's cap is separate, see BuildControllerStickCap).
void BuildControllerSpot(GuiMesh& mesh, const ControlSpot& spot, CTL_STATE state, float scale);

//!Builds an analog stick's cap, displaced by (x, y) (each -1 to 1) from the middle of its ring.
void BuildControllerStickCap(GuiMesh& mesh, const ControlSpot& spot, float x, float y, bool lit, float scale);

//!Builds the whole controller as thin lines only, no fills and no theme: the silhouette and every control
//!(a stick with its cap at rest) stroked centered on the outline in one color.
//!\param width Line width in drawing units (not multiplied by `scale`)
void BuildControllerOutline(GuiMesh& mesh, const ControllerLayout& layout, float scale, float width, PixelColor color);

// ---- the element

//!Draws a controller from a ControllerLayout and shows what is happening on it: controls light up
//!while pressed, a chosen control is highlighted (optionally with a line from a list row to it), and
//!sticks follow their position. It is a picture, not a button: selection lives in the list beside it,
//!and the list tells the view what to highlight.
class ControllerView : public GuiElement
{
	public:
		//!\param layout The controller to draw (must outlive the view)
		//!\param scale Multiplies the layout's units
		ControllerView(const ControllerLayout& layout, float scale = 1.0f);
		~ControllerView();

		//!Changes the controller drawn
		void setLayout(const ControllerLayout& layout);
		//!Changes the size
		void setScale(float scale);
		//!Highlights one control (0 = none)
		void setHighlight(int id);
		//!Highlights several controls (a D-pad, a group of face buttons)
		void setHighlight(const std::vector<int>& ids);
		//!Gets the highlighted control ids
		const std::vector<int>& getHighlight() const { return highlighted; }
		//!Draws a line from a point (design-canvas coordinates, such as a list row's edge) to the first
		//!highlighted control, or removes it
		void setLeader(bool on, float fromX = 0.0f, float fromY = 0.0f);
		//!Chooses which controls carry a text label (the default is all that have one). Small controls
		//!such as the shoulder buttons are too small for legible text at the sizes a list page draws.
		void setLabelFilter(std::function<bool(int)> fn) { labelFilter = fn; rebuild(); }
		//!Sets which controls are drawn as pressed; asked every frame (null for none)
		void setLit(std::function<bool(int)> fn) { lit = fn; }
		//!Sets an analog stick's position (-1 to 1 each)
		void setStick(int id, float x, float y);
		//!Gets the center of a control in design-canvas coordinates (false if the layout has no such control)
		bool getSpotCentre(int id, float& x, float& y) const;

		void draw() override;
	private:
		//!A mesh's ready-to-draw form in the video backend (see ImageRenderer::createMesh()), made once
		//!the mesh has been drawn a second time and kept until the mesh is rebuilt
		struct Baked {
			void * handle;
			bool noHandle;			//!< The backend can't make one; don't ask again
			uint32_t uses;
			Baked() : handle(nullptr), noHandle(false), uses(0) {}
		};

		struct SpotCache {
			GuiMesh mesh[4];
			bool built[4];
			Baked baked[4];
		};

		void rebuild();
		void clearLabels();
		void emit(const GuiMesh& m);
		//!Draws a mesh that doesn't change, from its backend copy when it has one. Order against the
		//!meshes emit() collects is kept by sending those first.
		void drawStatic(const GuiMesh& m, Baked& baked);
		void flushScratch();
		void freeBaked(Baked& b);
		void freeAllBaked();

		const ControllerLayout * layout;
		float scale;
		std::vector<int> highlighted;
		bool leader;
		float leaderX, leaderY;
		std::function<bool(int)> lit;
		std::function<bool(int)> labelFilter;
		std::vector<float> stickX, stickY;		//!< Per spot (only used by sticks)
		GuiMesh body;
		Baked bodyBaked;
		std::vector<SpotCache> spots;
		std::vector<GuiText *> labels;			//!< Per spot, null where there is no label
		std::vector<bool> labelLit;
		float meshFeather;
		bool dirty;
		std::vector<MeshVertex> scratch;
		float originX, originY;
		int alpha;
};

/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiMeshCache.h
 *
 * Platform-agnostic cache of shape meshes. Shapes that look the same (the
 * keys of an on-screen keyboard, the rows of a list) share one mesh instead
 * of tessellating and holding a copy each, and a mesh that keeps being drawn
 * is handed to the video backend (ImageRenderer::createMesh()) to keep in its
 * own ready-to-draw form, so drawing it again costs almost nothing on the CPU.
 ***************************************************************************/
#pragma once

//!Everything that decides what a shape's triangles are. Two shapes with equal
//!keys can share one mesh.
//!\ingroup grp_shapes
struct GuiMeshKey {
	SHAPE kind;				/*!< Rectangle or ellipse. */
	int width;				/*!< Width in design-canvas pixels. */
	int height;				/*!< Height in design-canvas pixels. */
	GuiCorners corners;		/*!< Corner radii of a rectangle. */
	GuiPaint paint;			/*!< Fill, outline and shadow. */
	float feather;			/*!< Anti-aliasing fringe width, see GuiMesh. */
	float tolerance;		/*!< Curve tolerance, see GuiMesh. */

	bool operator==(const GuiMeshKey& o) const;
};

//!One cached mesh. Owned by GuiMeshCache; hold it with GuiMeshCache::acquire()
//!and give it back with GuiMeshCache::release().
//!\ingroup grp_shapes
class GuiSharedMesh
{
	public:
		//!Gets the triangle list, in the shape's own coordinates (0,0 is its top-left)
		const std::vector<MeshVertex>& vertices() const { return mesh.vertices(); }
	private:
		friend class GuiMeshCache;
		explicit GuiSharedMesh(const GuiMeshKey& k);

		GuiMeshKey key;
		GuiMesh mesh;
		void * handle;			//!< Backend's ready-to-draw form, or nullptr
		bool noHandle;			//!< The backend can't (or failed to) make one; don't ask again
		uint32_t uses;			//!< Times drawn
		int refs;				//!< Shapes holding it
		uint32_t idleStamp;		//!< When the last holder let go, for eviction order
};

//!Static, since there is one video backend. Safe to call from any thread
//!except draw(), which must be called from the thread that draws.
//!\ingroup grp_shapes
class GuiMeshCache
{
	public:
		//!Finds or builds the mesh for a key and takes a reference to it. Pair with release().
		static GuiSharedMesh * acquire(const GuiMeshKey& key);
		//!Gives back a reference from acquire(). The mesh stays cached, ready for the next
		//!shape that wants it, until the cache needs the room.
		static void release(GuiSharedMesh * mesh);
		//!Draws a mesh with its (0,0) at (x, y), stretched by scaleX/scaleY about that point.
		//!\param alpha 255 for opaque; anything less draws through the slower path that
		//!rewrites each vertex's alpha
		//!\param scratch Working space for that path, kept by the caller so it isn't reallocated per frame
		static void draw(GuiSharedMesh * mesh, float x, float y, float scaleX, float scaleY, uint8_t alpha, std::vector<MeshVertex>& scratch);
		//!Frees every mesh nothing is holding, including the backend's copies (call when
		//!leaving the menu to give the memory back to whatever runs next)
		static void flush();
};

/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * Mesh.h
 *
 * Plain color and vertex types shared by the video interfaces and the
 * platform-agnostic shape tessellator (GuiGeometry). This header includes
 * nothing platform specific (no FreeType, no GX/GX2), so it can be built on
 * a host machine for tests.
 ***************************************************************************/
#pragma once
#include <stdint.h>

//!8-bit-per-channel RGBA color used by the video interfaces.
//!\ingroup grp_pal
typedef struct {
	uint8_t r;			/*!< Red color component. */
	uint8_t g;			/*!< Green color component. */
	uint8_t b;			/*!< Blue color component. */
	uint8_t a;			/*!< Alpha component. If a function does not use the alpha value, it is safely ignored. */
} PixelColor;

//!A single 2D vertex with its own color, in design-canvas coordinates.
//!Meshes are lists of these, three per triangle. The per-vertex color lets one
//!draw call carry gradients and soft (feathered) edges.
//!\ingroup grp_pal
struct MeshVertex {
	float x;			/*!< X position (design-canvas pixels). */
	float y;			/*!< Y position (design-canvas pixels). */
	PixelColor c;		/*!< Straight (non-premultiplied) RGBA color. */
};

#ifndef __GUI_H__
#define __GUI_H__

#include <gccore.h>


struct sGui {
	u8 texmem[640 * 480 * 4] ATTRIBUTE_ALIGN (32);	// rgba8 - working draw area
	int currmenu;
	int prevmenu;
	u32 fontcolour;
	int screenheight;
};

extern struct sGui Gui;

// Prototypes
void gui_alphasetup ();
void gui_makebg ();
void Make_Texture_RGBA8 (void * dst_tex, void * src_data, int width, int height);
void gui_drawbox (int x1, int y1, int width, int height, int r, int g, int b, int a);
void gui_DrawText (int x, int y, char *text);
void gui_setfontcolour (int r, int g, int b, int a);
void gui_DrawLine (int x1, int y1, int x2, int y2, int r, int g, int b, int a);
void gui_clearscreen ();
void gui_draw ();
void gui_showscreen ();

void gui_savescreen ();

// external functions
extern void setfontsize (int pixelsize);


#endif


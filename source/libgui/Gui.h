/*!\mainpage libgui Documentation
 *
 * \section Introduction
 * libgui is a GUI library originally created for the Wii/GameCube, created to 
 * help structure the design of a complicated GUI interface, and to enable an 
 * author to create a sophisticated, feature-rich GUI. It was originally conceived 
 * and written after I started to design a GUI for Snes9x GX, and found libwiisprite 
 * and GRRLIB inadequate for the purpose. It uses GX for drawing, and makes use
 * of PNGU for displaying images and FreeTypeGX for text. It was designed to
 * be flexible and is easy to modify - don't be afraid to change the way it
 * works or expand it to suit your GUI's purposes! If you do, and you think
 * your changes might benefit others, please share them so they might be
 * added to the project!
 *
 * \section Quickstart
 * Start from the supplied template example. For more advanced uses, see the
 * source code for Snes9x GX, FCE Ultra GX, and Visual Boy Advance GX.

 * \section Contact
 * If you have any suggestions for the library or documentation, or want to
 * contribute, please visit the libgui website:
 * http://code.google.com/p/libgui/

 * \section Credits
 * This library was wholly designed and written by Tantric. Thanks to the
 * authors of PNGU and FreeTypeGX, of which this library makes use. Thanks
 * also to the authors of GRRLIB and libwiisprite for laying the foundations.
 *
*/

#ifndef LIBGUI_H
#define LIBGUI_H

#include <gccore.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <exception>
#include <wchar.h>
#include <math.h>
#ifndef NO_SOUND
#include <asndlib.h>
#endif
#include <wiiuse/wpad.h>

#include "snes9xgx.h"
#include "video.h"
#include "filelist.h"
#include "fileop.h"
#include "input.h"
#include "../utils/pngu.h"
#include "../utils/FreeTypeGX.h"
#include "../utils/oggplayer.h"

extern FreeTypeGX *fontSystem[];

enum class ALIGN_V {
	TOP,
	BOTTOM,
	MIDDLE
};

enum class ALIGN_H {
	LEFT,
	RIGHT,
	CENTRE
};

enum class STATE {
	DEFAULT,
	SELECTED,
	CLICKED,
	HELD,
	DISABLED
};

enum class SCROLL {
	NONE,
	HORIZONTAL
};

#include "GuiTrigger.h"
#include "GuiElement.h"
#include "GuiWindow.h"
#include "GuiText.h"
#include "GuiSound.h"
#include "GuiImageData.h"
#include "GuiImage.h"
#include "GuiTooltip.h"
#include "GuiButton.h"
#include "GuiFileBrowser.h"
#include "GuiKeyboard.h"
#include "GuiOptionBrowser.h"
#include "GuiSaveBrowser.h"

#endif

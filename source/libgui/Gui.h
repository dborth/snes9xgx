/*!
 * \file Gui.h
 * \brief Umbrella header for the platform-agnostic libgui core.
 *
 * libgui - a GUI library for GameCube, Wii, and Wii U homebrew.
 * https://github.com/dborth/libgui
 *
 * Include this one header to get every core UI class (GuiElement, GuiWindow,
 * GuiButton, GuiImage, GuiText, GuiSound, GuiFileBrowser, GuiKeyboard,
 * GuiOptionBrowser, GuiSaveBrowser, ...), the alignment/state/scroll enums,
 * and the platform driver interfaces they are built on.
 *
 * Everything reachable from this header is platform-agnostic. All hardware
 * access goes through the abstract driver interfaces in source/drivers/
 * (Platform, VideoDriver, AudioDriver, InputDriver, FileSystemDriver,
 * ThreadDriver). Application code should only ever need to include
 * this header plus whichever concrete Platform header it instantiates.
 *
 * See README.md, CHANGELOG.md, and the API documentation in the repository
 * for more information.
 */

#pragma once

#include <cstdlib>
#include <cstring>
#include <vector>
#include <exception>
#include <cwchar>
#include <cmath>

#include "../filelist.h"
#include "../drivers/Platform.h"
#include "../drivers/InputData.h"
#include "../drivers/InputController.h"

//!Vertical alignment of a GuiElement relative to its parent.
//!\ingroup grp_core
enum class ALIGN_V {
	TOP,
	BOTTOM,
	MIDDLE
};

//!Horizontal alignment of a GuiElement relative to its parent.
//!\ingroup grp_core
enum class ALIGN_H {
	LEFT,
	RIGHT,
	CENTRE
};

//!Interaction state of a GuiElement (default, selected, clicked, held, disabled).
//!\ingroup grp_core
enum class STATE {
	DEFAULT,
	SELECTED,
	CLICKED,
	HELD,
	DISABLED
};

//!Scrolling mode of a GuiText (none or horizontal).
//!\ingroup grp_core
enum class SCROLL {
	NONE,
	HORIZONTAL
};

#include "GuiTrigger.h"
#include "GuiElement.h"
#include "GuiWindow.h"
#include "GuiTextRenderer.h"
#include "GuiTextTranslator.h"
#include "GuiText.h"
#include "GuiSound.h"
#include "GuiImageData.h"
#include "GuiImageDataCache.h"
#include "GuiImage.h"
#include "GuiButton.h"
#include "GuiFileBrowser.h"
#include "GuiKeyboard.h"
#include "GuiOptionBrowser.h"
#include "GuiSaveBrowser.h"

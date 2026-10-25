/*!
 * \file GuiWidgets.h
 * \brief Umbrella header for this application's widgets.
 *
 * Widgets are built from the core classes in libgui/ (GuiElement, GuiButton,
 * GuiLayout, ...) and, unlike the core, may depend on application data and
 * assets (the file list, the save list, filelist.h). They live in source/widgets/ and
 * are owned by the application: they may differ between applications.
 *
 * Include this after (or instead of) libgui/Gui.h wherever a widget is used.
 */

#pragma once

#include "../libgui/Gui.h"

#include "GuiImageAsyncCache.h"
#include "GuiGameBrowser.h"
#include "GuiKeyboard.h"
#include "GuiOptionBrowser.h"
#include "GuiSaveBrowser.h"

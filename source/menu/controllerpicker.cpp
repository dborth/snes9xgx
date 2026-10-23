/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * controllerpicker.cpp
 ***************************************************************************/
#include <string>
#include <vector>

#include "libgui/Gui.h"
#include "controllerpicker.h"
#include "menupopover.h"
#include "menu.h"
#include "snes9xgx.h"

namespace {

std::string Tr(const char * s)
{
	return textTranslator ? std::string(textTranslator->getText(s)) : std::string(s);
}

// The controllers preferences.cpp accepts (CTRL_SCOPE..CTRL_PAD4), gamepads first, then the light guns, then the mice
struct Kind {
	int ctrl;
	const char * detail;
	bool divider;	// a rule after it
};

const Kind kinds[] = {
	{ CTRL_PAD2,				"Two controllers, for one or two players.",	false },
	{ CTRL_PAD4,				"Multitap: up to four players.",			true },
	{ CTRL_SCOPE,				"Super Scope light gun.",					false },
	{ CTRL_JUST,				"Justifier light gun.",						true },
	{ CTRL_MOUSE,				"SNES Mouse in controller port 1.",			false },
	{ CTRL_MOUSE_PORT2,			"SNES Mouse in controller port 2.",			false },
	{ CTRL_MOUSE_BOTH_PORTS,	"A SNES Mouse in each controller port.",	false },
};

const size_t kindCount = sizeof(kinds) / sizeof(kinds[0]);

} // namespace

std::string ControllerChipValue()
{
	return Tr(ctrlName[EmuSettings.controller]);
}

bool ChooseController(const GuiPopover::Anchor& anchor)
{
	std::vector<PickerItem> items;
	for(size_t i = 0; i < kindCount; i++)
	{
		PickerItem it;
		it.name = Tr(ctrlName[kinds[i].ctrl]);
		MenuPopover_Wrap(it.lines, Tr(kinds[i].detail), 44);
		it.current = kinds[i].ctrl == EmuSettings.controller;
		it.divider = kinds[i].divider;
		items.push_back(it);
	}

	const int chosen = MenuPopover_Pick(Tr("Which controller is being emulated?").c_str(), items, anchor);
	if(chosen < 0 || (size_t)chosen >= kindCount)
		return false;

	EmuSettings.controller = kinds[chosen].ctrl;
	return true;
}

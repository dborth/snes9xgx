/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2022
 *
 * cheatmgr.cpp
 *
 * Cheat handling
 ***************************************************************************/


#include "snes9x/port.h"
#include "snes9x/cheats.h"

#include "snes9xgx.h"
#include "fileop.h"
#include "filebrowser.h"
#include "bml.h"

#define MAX_CHEATS      150

extern SCheatData Cheat;

/****************************************************************************
 * LoadCheatFile
 *
 * Loads cheat file from save buffer
 * Custom version of S9xLoadCheatFile()
 ***************************************************************************/

static bool LoadCheatFile (int length)
{
	uint8 data [28];
	int offset = 0;

	while (offset < length) {
		if(Cheat.g.size() >= MAX_CHEATS || (length - offset) < 28)
			break;

		memcpy (data, savebuffer+offset, 28);
		offset += 28;

		SCheat c;
		char name[21];
		char cheat[10];
		c.byte = data[1];
		c.address = data[2] | (data[3] << 8) |  (data[4] << 16);
		memcpy (name, &data[8], 20);
		name[20] = 0;

		snprintf (cheat, 10, "%x=%x", c.address, c.byte);
		S9xAddCheatGroup (name, cheat);
	}
	return true;
}

int S9xCheatIsDuplicate (const char *name, const char *code)
{
    unsigned int i;

    for (i = 0; i < Cheat.g.size(); i++)
    {
        if (!strcmp (name, Cheat.g[i].name))
        {
            char *code_string = S9xCheatGroupToText (i);
            char *validated   = S9xCheatValidate (code);

            if (validated && !strcmp (code_string, validated))
            {
                free (code_string);
                free (validated);
                return TRUE;
            }

            free (code_string);
            free (validated);
        }
    }

    return FALSE;
}

static void S9xLoadCheatsFromBMLNode (bml_node *n)
{
    unsigned int i;

    for (i = 0; i < n->child.size (); i++)
    {
        if (!strcasecmp (n->child[i].name.c_str(), "cheat"))
        {
            const char *desc = NULL;
            const char *code = NULL;
            bool8 enabled = false;

            bml_node *c = &n->child[i];
            bml_node *tmp = NULL;

            tmp = c->find_subnode("name");
            if (!tmp)
                desc = (char *) "";
            else
                desc = tmp->data.c_str();

            tmp = c->find_subnode("code");
            if (tmp)
                code = tmp->data.c_str();

            if (c->find_subnode("enable"))
                enabled = true;

            if (code && !S9xCheatIsDuplicate (desc, code))
            {
                int index = S9xAddCheatGroup (desc, code);

                if (enabled)
                    S9xEnableCheatGroup (index);
            }
        }
    }

    return;
}

void ToggleCheat(uint32 num) {
	if(Cheat.g[num].enabled) {
		S9xDisableCheatGroup(num);
	}
	else {
		S9xEnableCheatGroup(num);
	}

	for(int i=0; i < Cheat.g.size(); i++) {
		if(Cheat.g[i].enabled) {
			Cheat.enabled = TRUE;
			return;
		}
	}
	Cheat.enabled = FALSE;
}

/****************************************************************************
 * SetupCheats
 *
 * Erases any prexisting cheats, loads cheats from a cheat file
 * Called when a ROM is first loaded
 ***************************************************************************/
void
WiiSetupCheats()
{
	char filepath[1024];
	int offset = 0;

	if(!MakeFilePath(filepath, FILE_CHEAT))
		return;

	AllocSaveBuffer();

	offset = LoadFile(filepath, SILENT);

	// load cheat file if present
	if(offset > 0)
	{
		bml_node bml;
		if (!bml.parse_file(filepath))
		{
			LoadCheatFile (offset);
		}

		bml_node *n = bml.find_subnode("cheat");
		if (n)
		{
			S9xLoadCheatsFromBMLNode (&bml);
		}

		if (!n)
		{
			LoadCheatFile (offset);
		}
	}
		

	FreeSaveBuffer ();
}

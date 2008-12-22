/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Tantric August 2008
 *
 * smbload.cpp
 *
 * SMB support routines
 ****************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <network.h>
#include <smb.h>
#include <zlib.h>
#include <errno.h>

#include "snes9xGX.h"
#include "fileop.h"
#include "unzip.h"
#include "video.h"
#include "menudraw.h"
#include "filesel.h"

bool networkInit = false;
bool autoNetworkInit = true;
bool networkShareInit = false;

/****************************************************************************
 * InitializeNetwork
 * Initializes the Wii/GameCube network interface
 ****************************************************************************/

void InitializeNetwork(bool silent)
{
	// stop if we're already initialized, or if auto-init has failed before
	// in which case, manual initialization is required
	if(networkInit || !autoNetworkInit)
		return;

	if(!silent)
		ShowAction ("Initializing network...");

	char ip[16];
	s32 initResult = if_config(ip, NULL, NULL, true);

	if(initResult == 0)
	{
		networkInit = true;
	}
	else
	{
		// do not automatically attempt a reconnection
		autoNetworkInit = false;

		if(!silent)
		{
			char msg[150];
			sprintf(msg, "Unable to initialize network (Error #: %i)", initResult);
			WaitPrompt(msg);
		}
	}
}

void CloseShare()
{
	if(networkShareInit)
		smbClose();
	networkShareInit = false;
}

/****************************************************************************
 * Mount SMB Share
 ****************************************************************************/

bool
ConnectShare (bool silent)
{
	// Crashes or stalls system in GameCube mode - so disable
	#ifndef HW_RVL
	return false;
	#endif

	int chkU = (strlen(GCSettings.smbuser) > 0) ? 0:1;
	int chkP = (strlen(GCSettings.smbpwd) > 0) ? 0:1;
	int chkS = (strlen(GCSettings.smbshare) > 0) ? 0:1;
	int chkI = (strlen(GCSettings.smbip) > 0) ? 0:1;

	// check that all parameters have been set
	if(chkU + chkP + chkS + chkI > 0)
	{
		if(!silent)
		{
			char msg[50];
			char msg2[100];
			if(chkU + chkP + chkS + chkI > 1) // more than one thing is wrong
				sprintf(msg, "Check settings.xml.");
			else if(chkU)
				sprintf(msg, "Username is blank.");
			else if(chkP)
				sprintf(msg, "Password is blank.");
			else if(chkS)
				sprintf(msg, "Share name is blank.");
			else if(chkI)
				sprintf(msg, "Share IP is blank.");

			sprintf(msg2, "Invalid network settings - %s", msg);
			WaitPrompt(msg2);
		}
		return false;
	}

	if(!networkInit)
		InitializeNetwork(silent);

	if(networkInit)
	{
		if(unmountRequired[METHOD_SMB])
			CloseShare();

		if(!networkShareInit)
		{
			if(!silent)
				ShowAction ("Connecting to network share...");

			if(smbInit(GCSettings.smbuser, GCSettings.smbpwd,
					GCSettings.smbshare, GCSettings.smbip))
			{
				networkShareInit = true;
			}
		}

		if(!networkShareInit && !silent)
			WaitPrompt ("Failed to connect to network share.");
	}

	return networkShareInit;
}

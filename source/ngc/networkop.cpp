/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric December 2008
 *
 * networkop.cpp
 *
 * Network and SMB support routines
 ****************************************************************************/

#ifdef HW_RVL

#include <network.h>
#include <smb.h>
#include <mxml.h>

#include "unzip.h"
#include "miniunz.h"
#include "snes9xGX.h"
#include "menu.h"
#include "fileop.h"
#include "http.h"

static bool inNetworkInit = false;
static bool networkInit = false;
static bool autoNetworkInit = true;
static bool networkShareInit = false;
static bool updateChecked = false; // true if checked for app update
static char updateURL[128]; // URL of app update
bool updateFound = false; // true if an app update was found

/****************************************************************************
 * UpdateCheck
 * Checks for an update for the application
 ***************************************************************************/

void UpdateCheck()
{
	// we can only check for the update if we have internet + SD
	if(!updateChecked && networkInit && isMounted[METHOD_SD])
	{
		static char url[128];
		int retval;

		updateChecked = true;

		snprintf(url, 128, "http://snes9x-gx.googlecode.com/svn/trunk/update.xml");

		u8 * tmpbuffer = (u8 *)memalign(32,32768);
		memset(tmpbuffer, 0, 32768);
		retval = http_request(url, NULL, tmpbuffer, 32768);
		memset(url, 0, 128);

		if (retval)
		{
			mxml_node_t *xml;
			mxml_node_t *item;

			xml = mxmlLoadString(NULL, (char *)tmpbuffer, MXML_TEXT_CALLBACK);

			if(xml)
			{
				// check settings version
				item = mxmlFindElement(xml, xml, "app", "version", NULL, MXML_DESCEND);
				if(item) // a version entry exists
				{
					const char * version = mxmlElementGetAttr(item, "version");

					if(version && strlen(version) == 5)
					{
						int verMajor = version[0] - '0';
						int verMinor = version[2] - '0';
						int verPoint = version[4] - '0';
						int curMajor = APPVERSION[0] - '0';
						int curMinor = APPVERSION[2] - '0';
						int curPoint = APPVERSION[4] - '0';

						// check that the versioning is valid and is a newer version
						if((verMajor >= 0 && verMajor <= 9 &&
							verMinor >= 0 && verMinor <= 9 &&
							verPoint >= 0 && verPoint <= 9) &&
							(verMajor > curMajor ||
							(verMajor == curMajor && verMinor > curMinor) ||
							(verMajor == curMajor && verMinor == curMinor && verPoint > curPoint)))
						{
							item = mxmlFindElement(xml, xml, "file", NULL, NULL, MXML_DESCEND);
							if(item)
							{
								const char * tmp = mxmlElementGetAttr(item, "url");
								if(tmp)
								{
									snprintf(updateURL, 128, "%s", tmp);
									updateFound = true;
								}
							}
						}
					}
					else // temporary, for compatibility
					{
						int versionnum = atoi(version);
						if(versionnum > 12) // 012 (4.0.2)
						{
							item = mxmlFindElement(xml, xml, "file", NULL, NULL, MXML_DESCEND);
							if(item)
							{
								const char * tmp = mxmlElementGetAttr(item, "url");
								if(tmp)
								{
									snprintf(updateURL, 128, "%s", tmp);
									updateFound = true;
								}
							}
						}
					}
				}
				mxmlDelete(xml);
			}
		}
		free(tmpbuffer);
	}
}

static bool unzipArchive(char * zipfilepath, char * unzipfolderpath)
{
	unzFile uf = unzOpen(zipfilepath);
	if (uf==NULL)
		return false;

	if(chdir(unzipfolderpath)) // can't access dir
	{
		makedir(unzipfolderpath); // attempt to make dir
		if(chdir(unzipfolderpath)) // still can't access dir
			return false;
	}

	extractZip(uf,0,1,0);

	unzCloseCurrentFile(uf);
	return true;
}

bool DownloadUpdate()
{
	bool result = false;
	if(strlen(updateURL) > 0)
	{
		// stop checking if devices were removed/inserted
		// since we're saving a file
		LWP_SuspendThread (devicethread);

		FILE * hfile;
		char updateFile[50];
		sprintf(updateFile, "sd:/%s Update.zip", APPNAME);
		hfile = fopen (updateFile, "wb");

		if (hfile > 0)
		{
			int retval;
			retval = http_request(updateURL, hfile, NULL, (1024*1024*5));
			fclose (hfile);
		}

		bool unzipResult = unzipArchive(updateFile, (char *)"sd:/");
		remove(updateFile); // delete update file

		if(unzipResult)
		{
			result = true;
			InfoPrompt("Update successful!");
		}
		else
		{
			result = false;
			ErrorPrompt("Update failed!");
		}

		updateFound = false; // updating is finished (successful or not!)

		// go back to checking if devices were inserted/removed
		LWP_ResumeThread (devicethread);
	}
	return result;
}

/****************************************************************************
 * InitializeNetwork
 * Initializes the Wii/GameCube network interface
 ***************************************************************************/

void InitializeNetwork(bool silent)
{
	// stop if we're already initialized, or if auto-init has failed before
	// in which case, manual initialization is required
	if(networkInit || !autoNetworkInit)
		return;

	if(!silent)
		ShowAction ("Initializing network...");

	while(inNetworkInit) // a network init is already in progress!
		usleep(50);

	if(networkInit) // check again if the network was inited
		return;

	inNetworkInit = true;

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
			ErrorPrompt(msg);
		}
	}
	if(!silent)
		CancelAction();
	inNetworkInit = false;
}

void CloseShare()
{
	if(networkShareInit)
		smbClose("smb");
	networkShareInit = false;
	networkInit = false; // trigger a network reinit
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
			ErrorPrompt(msg2);
		}
		return false;
	}

	if(unmountRequired[METHOD_SMB])
		CloseShare();

	if(!networkInit)
		InitializeNetwork(silent);

	if(networkInit)
	{
		if(!networkShareInit)
		{
			if(!silent)
				ShowAction ("Connecting to network share...");

			if(smbInit(GCSettings.smbuser, GCSettings.smbpwd,
					GCSettings.smbshare, GCSettings.smbip))
			{
				networkShareInit = true;
			}
			if(!silent)
				CancelAction();
		}

		if(!networkShareInit && !silent)
			ErrorPrompt("Failed to connect to network share.");
	}

	return networkShareInit;
}

#endif

/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Wii/Gamecube Port
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
#include <string.h>
#include <ogcsys.h>
#include <network.h>
#include <smb.h>
#include <zlib.h>
#include <errno.h>

#include "memmap.h"

#include "unzip.h"
#include "video.h"
#include "menudraw.h"
#include "dvd.h"
#include "filesel.h"
#include "smbop.h"
#include "Snes9xGx.h"

SMBCONN smbconn;

extern char currentdir[MAXPATHLEN];

char output[16384];
extern unsigned char savebuffer[];
#define ZIPCHUNK 16384

/****************************************************************************
 * InitializeNetwork
 * Initializes the Wii/GameCube network interface
 ****************************************************************************/

bool InitializeNetwork(bool silent)
{
	ShowAction ((char*) "Initializing network...");
	s32 result;

	while ((result = net_init()) == -EAGAIN);

	if (result >= 0)
	{
		char myIP[16];

		if (if_config(myIP, NULL, NULL, true) < 0)
		{
			WaitPrompt((char*) "Error reading IP address.");
			return false;
		}
		else
		{
			if(!silent)
			{
				char msg[100];
				sprintf(msg, "Network initialized. IP address: %s", myIP);
				WaitPrompt(msg);
			}
			return true;
		}
	}

	WaitPrompt((char*) "Unable to initialize network.");
	return false;
}

/****************************************************************************
 * Mount SMB Share
 ****************************************************************************/
bool
ConnectShare ()
{
	bool networkInit = false;
	bool networkShareInit = false;

	networkInit = InitializeNetwork(SILENT);

	if(networkInit)
	{
		ShowAction ((char*) "Connecting to network share...");

		if(SMB_Connect(&smbconn, GCSettings.smbuser, GCSettings.smbpwd,
		GCSettings.smbgcid, GCSettings.smbsvid, GCSettings.smbshare, GCSettings.smbip) == SMB_SUCCESS)
			networkShareInit = true;

		if(!networkShareInit)
			WaitPrompt ((char*) "Failed to connect to network share.");
	}

	return networkShareInit;
}

/****************************************************************************
 * parseSMBDirectory
 *
 * Load the directory and put in the filelist array
 *****************************************************************************/
int
parseSMBdirectory ()
{
	int filecount = 0;
	char searchpath[1024];

	SMBDIRENTRY smbdir;

	sprintf(searchpath, "%s/*", currentdir);

	// fix path - replace all '/' with '\'
	for(uint i=0; i < strlen(searchpath); i++)
		if(searchpath[i] == '/')
			searchpath[i] = '\\';

	ShowAction((char*) "Loading...");

	if (SMB_FindFirst
	(searchpath, SMB_SRCH_READONLY | SMB_SRCH_DIRECTORY, &smbdir, smbconn) != SMB_SUCCESS)
	{
		char msg[200];
		sprintf(msg, "Could not open %s", currentdir);
		WaitPrompt (msg);

		// if we can't open the dir, open root dir
		currentdir[0] = '\0';
		sprintf(searchpath,"*");

		if (SMB_FindFirst
		(searchpath, SMB_SRCH_READONLY | SMB_SRCH_DIRECTORY, &smbdir, smbconn) != SMB_SUCCESS)
			return 0;
	}

	// index files/folders
	do
	{
		if(strcmp(smbdir.name,".") != 0 &&
		!(strcmp(currentdir,"/") == 0 && strcmp(smbdir.name,"..") == 0))
		{
			memset (&filelist[filecount], 0, sizeof (FILEENTRIES));
			filelist[filecount].length = smbdir.size_low;
			smbdir.name[MAXJOLIET] = 0;

			if(smbdir.attributes == SMB_SRCH_DIRECTORY)
				filelist[filecount].flags = 1; // flag this as a dir
			else
				filelist[filecount].flags = 0;

			// Update display name
			memcpy (&filelist[filecount].displayname, smbdir.name, MAXDISPLAY);
			filelist[filecount].displayname[MAXDISPLAY] = 0;

			strcpy (filelist[filecount].filename, smbdir.name);
			filecount++;
		}
	} while (SMB_FindNext (&smbdir, smbconn) == SMB_SUCCESS);

	// close directory
	SMB_FindClose (smbconn);

	// Sort the file list
	qsort(filelist, filecount, sizeof(FILEENTRIES), FileSortCallback);

	return filecount;
}

/****************************************************************************
 * Load SMB file
 ****************************************************************************/
int
LoadSMBFile (char *filename, int length)
{
	int offset = 0;
	int bytesread = 0;
	char filepath[1024];
	SMBFILE smbfile;
	unsigned char *rbuffer;
	char zipbuffer[16384];
	int pass = 0;
	int zip = 0;
	PKZIPHEADER pkzip;
	z_stream zs;
	int res, outbytes = 0;
	rbuffer = (unsigned char *) Memory.ROM;
	int have = 0;

	if(strcmp(currentdir,"/") == 0)
		sprintf(filepath, "/%s", filename);
	else
		sprintf(filepath, "%s/%s", currentdir, filename);

	// fix path - replace all '/' with '\'
	for(uint i=0; i < strlen(filepath); i++)
		if(filepath[i] == '/')
			filepath[i] = '\\';

	ShowAction((char *)"Loading...");

	// Open the file for reading
	smbfile =
	SMB_OpenFile (filepath, SMB_OPEN_READING, SMB_OF_OPEN, smbconn);
	if (smbfile)
	{
		while (offset < length)
		{
			// Don't read past end of file
			if (offset + bytesread > length)
				bytesread = length - offset;
			else
				bytesread = 16384;

			SMB_ReadFile (zipbuffer, bytesread, offset, smbfile);

			if (pass == 0)
			{
				// Is this a Zip file ?
				zip = IsZipFile (zipbuffer);
				if (zip)
				{
					memcpy (&pkzip, zipbuffer, sizeof (PKZIPHEADER));
					pkzip.uncompressedSize = FLIP32 (pkzip.uncompressedSize);
					memset (&zs, 0, sizeof (zs));
					zs.zalloc = Z_NULL;
					zs.zfree = Z_NULL;
					zs.opaque = Z_NULL;
					zs.avail_in = 0;
					zs.next_in = Z_NULL;
					res = inflateInit2 (&zs, -MAX_WBITS);

					if (res != Z_OK)
					{
						SMB_CloseFile (smbfile);
						return 0;
					}

					zs.avail_in =
					16384 - (sizeof (PKZIPHEADER) +
					FLIP16 (pkzip.filenameLength) +
					FLIP16 (pkzip.extraDataLength));
					zs.next_in =
					(Bytef *) zipbuffer + (sizeof (PKZIPHEADER) +
					FLIP16 (pkzip.filenameLength) +
					FLIP16 (pkzip.extraDataLength));
				}
			}

			if (zip)
			{
				if (pass)
				{
					zs.avail_in = bytesread;
					zs.next_in = (Bytef *) zipbuffer;
				}

				do
				{
					zs.avail_out = ZIPCHUNK;
					zs.next_out = (Bytef *) output;

					res = inflate (&zs, Z_NO_FLUSH);

					have = ZIPCHUNK - zs.avail_out;

					if (have)
					{
						memcpy (rbuffer + outbytes, output, have);
						outbytes += have;
					}
				} while (zs.avail_out == 0);
			}
			else
			{
				memcpy (rbuffer + offset, zipbuffer, bytesread);
			}
			offset += bytesread;

			pass++;
		}

		if (zip)
		{
			inflateEnd (&zs);
			offset = outbytes;
		}

		SMB_CloseFile (smbfile);

		return offset;
	}
	else
	{
		WaitPrompt((char*) "SMB Reading Failed!");
		return 0;
	}
}


/****************************************************************************
 * Write savebuffer to SMB file
 ****************************************************************************/
int
SaveBufferToSMB (char *filepath, int datasize, bool8 silent)
{
	SMBFILE smbfile;
	int dsize = datasize;
	int wrote = 0;
	int offset = 0;

	// fix path - replace all '/' with '\'
	for(uint i=0; i < strlen(filepath); i++)
		if(filepath[i] == '/')
			filepath[i] = '\\';

	smbfile =
	SMB_OpenFile (filepath, SMB_OPEN_WRITING | SMB_DENY_NONE,
	SMB_OF_CREATE | SMB_OF_TRUNCATE, smbconn);

	if (smbfile)
	{
		while (dsize > 0)
		{
			if (dsize > 1024)
				wrote =
					SMB_WriteFile ((char *) savebuffer + offset, 1024, offset, smbfile);
			else
				wrote =
					SMB_WriteFile ((char *) savebuffer + offset, dsize, offset, smbfile);

			offset += wrote;
			dsize -= wrote;
		}

		SMB_CloseFile (smbfile);

		return offset;
	}
	else
	{
		char msg[100];
		sprintf(msg, "Couldn't save SMB: %s", filepath);
		WaitPrompt (msg);
	}

	return 0;
}

/****************************************************************************
 * Load savebuffer from SMB file
 ****************************************************************************/
int
LoadBufferFromSMB (char *filepath, bool8 silent)
{
	SMBFILE smbfile;
	int ret;
	int offset = 0;

	// fix path - replace all '/' with '\'
	for(uint i=0; i < strlen(filepath); i++)
		if(filepath[i] == '/')
			filepath[i] = '\\';

	smbfile =
	SMB_OpenFile (filepath, SMB_OPEN_READING, SMB_OF_OPEN, smbconn);

	if (!smbfile)
	{
		if (!silent)
		{
			char msg[100];
			sprintf(msg, "Couldn't open SMB: %s", filepath);
			WaitPrompt (msg);
		}
		return 0;
	}

	memset (savebuffer, 0, 0x22000);

	while ((ret =
	SMB_ReadFile ((char *) savebuffer + offset, 1024, offset,
	smbfile)) > 0)
		offset += ret;

	SMB_CloseFile (smbfile);

	return offset;
}

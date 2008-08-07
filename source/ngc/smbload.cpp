/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube Port
 * softdev  July 2006
 * crunchy2 May 2007
 *
 * smbload.cpp
 *
 * Load ROMS from a Network share.
 ****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <network.h>
extern "C"
{
#include "smb.h"
}
#include "unzip.h"
#include "memmap.h"
#include "video.h"
#include "menudraw.h"
#include "dvd.h"
#include "filesel.h"
#include "sram.h"
#include "preferences.h"
#include "smbload.h"
#include "Snes9xGx.h"
#include <zlib.h>

static int connected = 0;
static int netinited = 0;

char output[16384];
unsigned int SMBTimer = 0;
extern unsigned char savebuffer[];

#define ZIPCHUNK 16384
#define SMBTIMEOUT ( 3600 )	/*** Some implementations timeout in 10 minutes ***/
SMBINFO smbinfo =
  { GC_IP, GW_IP, MASK, SMB_IP,
   SMB_USER, SMB_PWD, SMB_GCID, SMB_SVID, SMB_SHARE
};

extern FILEENTRIES filelist[MAXFILES];

/****************************************************************************
 * Mount SMB Share
 ****************************************************************************/
void
ConnectSMB ()
{
    int ret;

    if (SMBTimer > SMBTIMEOUT)
    {
        connected = 0;
        SMBTimer = 0;
    }

    if (connected == 0)
    {

        if (netinited == 0)
        {
            ShowAction ((char*) "Setting up network interface ...");
            ret = if_config (smbinfo.gcip, smbinfo.gwip, smbinfo.mask, 0);
            netinited = 1;
        }

        ShowAction ((char*) "Connecting to share ...");
        SMB_Destroy ();

        if (SMB_Init (smbinfo.smbuser, smbinfo.smbpwd,
            smbinfo.smbgcid, smbinfo.smbsvid, smbinfo.smbshare,
            smbinfo.smbip) != SMB_SUCCESS)
        {
            WaitPrompt((char*) "Failed to connect to SMB share");
            connected = 0;
            return;
        }
    }

    connected = 1;

}

/****************************************************************************
 * parseSMBDirectory
 *
 * Load the share directory and put in the filelist array
 *****************************************************************************/
int
parseSMBDirectory ()
{
  char searchpath[1024];
  int filecount = 0;
  SMBDIRENTRY smbdir;

  ConnectSMB ();

  strcpy (searchpath, GCSettings.LoadFolder);
  strcat (searchpath, "\\*.*");

  if (SMB_FindFirst
      (searchpath, SMB_SRCH_READONLY | SMB_SRCH_SYSTEM | SMB_SRCH_HIDDEN,
       &smbdir) != SMB_SUCCESS)
    {
      return 0;
    }

  do
    {
      memset (&filelist[filecount], 0, sizeof (FILEENTRIES));
      filelist[filecount].length = smbdir.size_low;
      smbdir.name[MAXJOLIET] = 0;

      /*** Update display name ***/
      memcpy (&filelist[filecount].displayname, smbdir.name, MAXDISPLAY);
      filelist[filecount].displayname[MAXDISPLAY] = 0;

      strcpy (filelist[filecount].filename, smbdir.name);
      filecount++;

    }
  while (SMB_FindNext (&smbdir) == SMB_SUCCESS);

  SMB_FindClose ();

  return filecount;
}

/****************************************************************************
 * Load SMB file
 ****************************************************************************/
int
LoadSMBFile (char *filename, int length)
{
  char buffer[128];
  int offset = 0;
  int bytesread = 0;
  int total = 0;
  char filepath[1024];
  SMBFILE smbfile;
  char *rbuffer;
  char zipbuffer[16384];
  int pass = 0;
  int zip = 0;
  PKZIPHEADER pkzip;
  z_stream zs;
  int res, outbytes;

  strcpy (filepath, GCSettings.LoadFolder);
  strcat (filepath, "\\");
  strcat (filepath, filename);
  rbuffer = (char *) Memory.ROM;
  outbytes = 0;
  int have = 0;

  ConnectSMB ();

  if ( connected )
  {

        /*** Open the file for reading ***/
      smbfile =
        SMB_Open (filepath, SMB_OPEN_READING | SMB_DENY_NONE, SMB_OF_OPEN);
      if (smbfile)
        {
          while (total < length)
        {
          bytesread = SMB_Read (zipbuffer, 16384, offset, smbfile);

          if (pass == 0)
            {
                    /*** Is this a Zip file ? ***/
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
                  SMB_Close (smbfile);
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
            }
              while (zs.avail_out == 0);
            }
          else
            memcpy (rbuffer + offset, zipbuffer, bytesread);

          total += bytesread;
          offset += bytesread;

          if (!zip)
            {
              sprintf (buffer, "Read %d of %d bytes", total, length);
              ShowProgress (buffer, total, length);
            }
          else
            {
              sprintf (buffer, "Unzipped %d of %d", outbytes,
                   pkzip.uncompressedSize);
              ShowProgress (buffer, outbytes, pkzip.uncompressedSize);
            }
          //ShowAction (buffer);

          pass++;

        }

          if (zip)
        {
          inflateEnd (&zs);
          total = outbytes;
        }

          SMB_Close (smbfile);

          return total;
        }
      else
        {
          WaitPrompt((char*) "SMB Reading Failed!");
          //while (1);
          return 0;
        }
    }

    return 0;
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

    ConnectSMB ();

    if ( connected )
    {
        smbfile =
            SMB_Open (filepath, SMB_OPEN_WRITING | SMB_DENY_NONE,
            SMB_OF_CREATE | SMB_OF_TRUNCATE);

        if (smbfile)
        {
            while (dsize > 0)
            {
                if (dsize > 1024)
                    wrote =
                        SMB_Write ((char *) savebuffer + offset, 1024, offset, smbfile);
                else
                    wrote =
                        SMB_Write ((char *) savebuffer + offset, dsize, offset, smbfile);

                offset += wrote;
                dsize -= wrote;
            }

            SMB_Close (smbfile);
            SMBTimer = 0;

            return offset;
        }
        else
        {
            char msg[100];
            sprintf(msg, "Couldn't save SMB:%s", filepath);
            WaitPrompt (msg);
        }
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

    ConnectSMB ();

    if ( connected )
    {
        smbfile =
            SMB_Open (filepath, SMB_OPEN_READING | SMB_DENY_NONE, SMB_OF_OPEN);

        if (!smbfile)
        {
            if (!silent)
            {
                char msg[100];
                sprintf(msg, "Couldn't open SMB:%s", filepath);
                WaitPrompt (msg);
            }
            return 0;
        }

        memset (savebuffer, 0, 0x22000);

        while ((ret =
                SMB_Read ((char *) savebuffer + offset, 1024, offset,
                smbfile)) > 0)
            offset += ret;

        SMB_Close (smbfile);

        return offset;
    }

    return 0;
}

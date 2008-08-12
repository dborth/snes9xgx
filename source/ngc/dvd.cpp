/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Gamecube DVD
 *
 * softdev July 2006
 * svpe & crunchy2 June 2007
 ****************************************************************************/
#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dvd.h>

#include "menudraw.h"
#include "snes9xGx.h"
#include "unzip.h"

extern int offset;
extern int selection;

/** DVD I/O Address base **/
volatile unsigned long *dvd = (volatile unsigned long *) 0xCC006000;

 /** Due to lack of memory, we'll use this little 2k keyhole for all DVD operations **/
unsigned char DVDreadbuffer[2048] ATTRIBUTE_ALIGN (32);
unsigned char dvdbuffer[2048];


/**
  * dvd_driveid
  *
  * Gets and returns the dvd driveid
**/
static unsigned char *inquiry=(unsigned char *)0x80000004;

int dvd_driveid()
{
    dvd[0] = 0x2e;
    dvd[1] = 0;
    dvd[2] = 0x12000000;
    dvd[3] = 0;
    dvd[4] = 0x20;
    dvd[5] = 0x80000000;
    dvd[6] = 0x20;
    dvd[7] = 3;

    while( dvd[7] & 1 )
        ;
    DCFlushRange((void *)0x80000000, 32);

    return (int)inquiry[2];
}

 /**
  * dvd_read
  *
  * The only DVD function we need - you gotta luv gc-linux self-boots!
  */
int
dvd_read (void *dst, unsigned int len, u64 offset)
{

  unsigned char *buffer = (unsigned char *) (unsigned int) DVDreadbuffer;

  if (len > 2048)
    return 1;				/*** We only allow 2k reads **/

  DCInvalidateRange ((void *) buffer, len);

  if(offset < 0x57057C00 || (isWii == true && offset < 0x118244F00LL)) // don't read past the end of the DVD
    {
      offset >>= 2;
      dvd[0] = 0x2E;
      dvd[1] = 0;
      dvd[2] = 0xA8000000;
      dvd[3] = (u32)offset;
      dvd[4] = len;
      dvd[5] = (unsigned long) buffer;
      dvd[6] = len;
      dvd[7] = 3;			/*** Enable reading with DMA ***/
      while (dvd[7] & 1);
      memcpy (dst, buffer, len);
    }
  else				// Let's not read past end of DVD
    return 0;

  if (dvd[0] & 0x4)		/* Ensure it has completed */
    return 0;

  return 1;

}

/** Minimal ISO Directory Definition **/
#define RECLEN 0		/* Record length */
#define EXTENT 6		/* Extent */
#define FILE_LENGTH 14		/* File length (BIG ENDIAN) */
#define FILE_FLAGS 25		/* File flags */
#define FILENAME_LENGTH 32	/* Filename length */
#define FILENAME 33		/* ASCIIZ filename */

/** Minimal Primary Volume Descriptor **/
#define PVDROOT 0x9c
static int IsJoliet = 0;
u64 rootdir = 0;
int rootdirlength = 0;

/** Global file entry table **/
FILEENTRIES filelist[MAXFILES];

/**
 * Primary Volume Descriptor
 *
 * The PVD should reside between sector 16 and 31.
 * This is for single session DVD only.
 */
int
getpvd ()
{
  int sector = 16;
  u32 rootdir32;

  rootdir = rootdirlength = 0;
  IsJoliet = -1;

	/** Look for Joliet PVD first **/
  while (sector < 32)
    {
      if (dvd_read (dvdbuffer, 2048, (u64)(sector << 11)))
	{
	  if (memcmp (&dvdbuffer, "\2CD001\1", 8) == 0)
	    {
	      memcpy(&rootdir32, &dvdbuffer[PVDROOT + EXTENT], 4);
	      rootdir = (u64)rootdir32;
	      rootdir <<= 11;
    	  memcpy (&rootdirlength, &dvdbuffer[PVDROOT + FILE_LENGTH], 4);
	      IsJoliet = 1;
	      break;
	    }
	}
      else
	return 0;			/*** Can't read sector! ***/

      sector++;
    }

  if (IsJoliet > 0)		/*** Joliet PVD Found ? ***/
    return 1;

  sector = 16;

	/*** Look for standard ISO9660 PVD ***/
  while (sector < 32)
    {
      if (dvd_read (&dvdbuffer, 2048, sector << 11))
	{
	  if (memcmp (&dvdbuffer, "\1CD001\1", 8) == 0)
	    {
	      memcpy (&rootdir32, &dvdbuffer[PVDROOT + EXTENT], 4);
	      rootdir = (u64)rootdir32;
	      rootdir <<= 11;
	      memcpy (&rootdirlength, &dvdbuffer[PVDROOT + FILE_LENGTH], 4);
	      IsJoliet = 0;
	      break;
	    }
	}
      else
	return 0;			/*** Can't read sector! ***/

      sector++;

    }

  return (IsJoliet == 0);

}

/**
 * getentry
 *
 * Support function to return the next file entry, if any
 * Declared static to avoid accidental external entry.
 */
static int diroffset = 0;
static int
getentry (int entrycount)
{
  char fname[512];		/* Huge, but experience has determined this */
  char *ptr;
  char *filename;
  char *filenamelength;
  char *rr;
  int j;
  u32 offset32;

  /* Basic checks */
  if (entrycount >= MAXFILES)
    return 0;

  if (diroffset >= 2048)
    return 0;

	/** Decode this entry **/
  if (dvdbuffer[diroffset])	/* Record length available */
    {
      /* Update offsets into sector buffer */
      ptr = (char *) &dvdbuffer[0];
      ptr += diroffset;
      filename = ptr + FILENAME;
      filenamelength = ptr + FILENAME_LENGTH;

      /* Check for wrap round - illegal in ISO spec,
       * but certain crap writers do it! */
      if ((diroffset + dvdbuffer[diroffset]) > 2048)
	return 0;

      if (*filenamelength)
	{
	  memset (&fname, 0, 512);

	  if (!IsJoliet)			/*** Do ISO 9660 first ***/
	    strcpy (fname, filename);
	  else
	    {			/*** The more tortuous unicode joliet entries ***/

	      for (j = 0; j < (*filenamelength >> 1); j++)
		{
		  fname[j] = filename[j * 2 + 1];
		}

	      fname[j] = 0;

	      if (strlen (fname) >= MAXJOLIET)
		fname[MAXJOLIET] = 0;

	      if (strlen (fname) == 0)
		fname[0] = filename[0];
	    }

	  if (strlen (fname) == 0)
	    strcpy (fname, "ROOT");
	  else
	    {
	      if (fname[0] == 1)
		strcpy (fname, "..");
	      else
		{		/*
				 * Move *filenamelength to t,
				 * Only to stop gcc warning for noobs :)
				 */
		  int t = *filenamelength;
		  fname[t] = 0;
		}
	    }

			/** Rockridge Check **/
	  rr = strstr (fname, ";");
	  if (rr != NULL)
	    *rr = 0;

	  strcpy (filelist[entrycount].filename, fname);
	  fname[MAXDISPLAY] = 0;
	  strcpy (filelist[entrycount].displayname, fname);

	  memcpy (&offset32,
		  &dvdbuffer[diroffset + EXTENT], 4);
	  filelist[entrycount].offset = (u64)offset32;
	  memcpy (&filelist[entrycount].length,
		  &dvdbuffer[diroffset + FILE_LENGTH], 4);
	  memcpy (&filelist[entrycount].flags,
		  &dvdbuffer[diroffset + FILE_FLAGS], 1);

	  filelist[entrycount].offset <<= 11;
	  filelist[entrycount].flags = filelist[entrycount].flags & 2;

			/*** Prepare for next entry ***/
	  diroffset += dvdbuffer[diroffset];

	  return 1;

	}

    }

  return 0;

}

/**
 * parseDVDdirectory
 *
 * This function will parse the directory tree.
 * It relies on rootdir and rootdirlength being pre-populated by a call to
 * getpvd, a previous parse or a menu selection.
 *
 * The return value is number of files collected, or 0 on failure.
 */
int
ParseDVDdirectory ()
{
  int pdlength;
  u64 pdoffset;
  u64 rdoffset;
  int len = 0;
  int filecount = 0;

  // initialize selection
  	selection = offset = 0;

  pdoffset = rdoffset = rootdir;
  pdlength = rootdirlength;
  filecount = 0;

	/*** Clear any existing values ***/
  memset (&filelist, 0, sizeof (FILEENTRIES) * MAXFILES);

	/*** Get as many files as possible ***/
  while (len < pdlength)
    {
      if (dvd_read (&dvdbuffer, 2048, pdoffset) == 0)
	return 0;

      diroffset = 0;

      while (getentry (filecount))
	{
	  if (filecount < MAXFILES)
	    filecount++;
	}

      len += 2048;
      pdoffset = rdoffset + len;
    }

  return filecount;
}

/****************************************************************************
 * LoadDVDFile
 * This function will load a file from DVD, in BIN, SMD or ZIP format.
 * The values for offset and length are inherited from rootdir and
 * rootdirlength.
 *
 * The buffer parameter should re-use the initial ROM buffer.
 ****************************************************************************/

int
LoadDVDFile (unsigned char *buffer)
{
  int offset;
  int blocks;
  int i;
  u64 discoffset;
  char readbuffer[2048];

  // How many 2k blocks to read
  blocks = rootdirlength / 2048;
  offset = 0;
  discoffset = rootdir;
  ShowAction ((char*) "Loading...");
  dvd_read (readbuffer, 2048, discoffset);

  if (!IsZipFile (readbuffer))

    {
      for (i = 0; i < blocks; i++)

        {
          dvd_read (readbuffer, 2048, discoffset);
          memcpy (buffer + offset, readbuffer, 2048);
          offset += 2048;
          discoffset += 2048;
        }

                /*** And final cleanup ***/
      if (rootdirlength % 2048)

        {
          i = rootdirlength % 2048;
          dvd_read (readbuffer, 2048, discoffset);
          memcpy (buffer + offset, readbuffer, i);
        }
    }

  else

    {
      return UnZipBuffer (buffer, discoffset, 1, NULL);	// unzip from dvd
    }
  return rootdirlength;
}

/****************************************************************************
 * uselessinquiry
 *
 * As the name suggests, this function is quite useless.
 * It's only purpose is to stop any pending DVD interrupts while we use the
 * memcard interface.
 *
 * libOGC tends to foul up if you don't, and sometimes does if you do!
 ****************************************************************************/
void uselessinquiry ()
{

  dvd[0] = 0;
  dvd[1] = 0;
  dvd[2] = 0x12000000;
  dvd[3] = 0;
  dvd[4] = 0x20;
  dvd[5] = 0x80000000;
  dvd[6] = 0x20;
  dvd[7] = 1;

  while (dvd[7] & 1);
}

void dvd_motor_off( )
{
	dvd[0] = 0x2e;
	dvd[1] = 0;
	dvd[2] = 0xe3000000;
	dvd[3] = 0;
	dvd[4] = 0;
	dvd[5] = 0;
	dvd[6] = 0;
	dvd[7] = 1; // Do immediate
	while (dvd[7] & 1);

	/*** PSO Stops blackscreen at reload ***/
	dvd[0] = 0x14;
	dvd[1] = 0;
}


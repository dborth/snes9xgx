/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * svpe & crunchy2 June 2007
 * Tantric September 2008
 *
 * dvd.cpp
 *
 * DVD I/O functions
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#ifdef WII_DVD
extern "C" {
#include <di/di.h>
}
#endif

#include "memmap.h"

#include "menudraw.h"
#include "snes9xGX.h"
#include "unzip.h"

u64 dvddir = 0; // offset of currently selected file or folder
int dvddirlength = 0; // length of currently selected file or folder
u64 dvdrootdir = 0; // offset of DVD root
int dvdrootlength = 0; // length of DVD root
bool isWii = false;

#ifdef HW_DOL
/** DVD I/O Address base **/
volatile unsigned long *dvd = (volatile unsigned long *) 0xCC006000;
#endif

/****************************************************************************
 * dvd_read
 *
 * Main DVD function, everything else uses this!
 * returns: 1 - ok ; 0 - error
 ***************************************************************************/
#define ALIGN_FORWARD(x,align) 	((typeof(x))((((uint32_t)(x)) + (align) - 1) & (~(align-1))))
#define ALIGN_BACKWARD(x,align)	((typeof(x))(((uint32_t)(x)) & (~(align-1))))

int
dvd_read (void *dst, unsigned int len, u64 offset)
{
	if (len > 2048)
		return 0;				/*** We only allow 2k reads **/

	// don't read past the end of the DVD (1.5 GB for GC DVD, 4.7 GB for DVD)
	if((offset < 0x57057C00) || (isWii && (offset < 0x118244F00LL)))
	{
		u8 * buffer = (u8 *)memalign(32, 0x8000);
		u32 off_size = 0;

		DCInvalidateRange ((void *) buffer, len);

		#ifdef HW_DOL
			dvd[0] = 0x2E;
			dvd[1] = 0;
			dvd[2] = 0xA8000000;
			dvd[3] = (u32)(offset >> 2);
			dvd[4] = len;
			dvd[5] = (u32) buffer;
			dvd[6] = len;
			dvd[7] = 3;

			// Enable reading with DMA
			while (dvd[7] & 1);

			// Ensure it has completed
			if (dvd[0] & 0x4)
				return 0;
		#else
			off_size = offset - ALIGN_BACKWARD(offset,0x800);
			if (DI_ReadDVD(
				buffer,
				(ALIGN_FORWARD(offset + len,0x800) - ALIGN_BACKWARD(offset,0x800)) >> 11,
				(u32)(ALIGN_BACKWARD(offset, 0x800) >> 11)
			))
				return 0;
		#endif

		memcpy (dst, buffer+off_size, len);
		free(buffer);
		return 1;
	}

	return 0;
}

/****************************************************************************
 * dvd_buffered_read
 *
 * the GC's dvd drive only supports offsets and length which are a multiple
 * of 32 bytes additionally the max length of a read is 2048 bytes
 * this function removes these limitations
 * additionally the 7zip SDK does often read data in 1 byte parts from the
 * DVD even when it could read 32 bytes. the dvdsf_buffer has been added to
 * avoid having to read the same sector over and over again
 ***************************************************************************/

#define DVD_LENGTH_MULTIPLY 32
#define DVD_OFFSET_MULTIPLY 32
#define DVD_MAX_READ_LENGTH 2048
#define DVD_SECTOR_SIZE 2048

unsigned char dvdsf_buffer[DVD_SECTOR_SIZE];
u64 dvdsf_last_offset = 0;
u64 dvdsf_last_length = 0;

int dvd_buffered_read(void *dst, u32 len, u64 offset)
{
    int ret = 0;

    // only read data if the data inside dvdsf_buffer cannot be used
    if(offset != dvdsf_last_offset || len > dvdsf_last_length)
    {
        memset(&dvdsf_buffer, '\0', DVD_SECTOR_SIZE);
        ret = dvd_read(&dvdsf_buffer, len, offset);
        dvdsf_last_offset = offset;
        dvdsf_last_length = len;
    }

    memcpy(dst, &dvdsf_buffer, len);
    return ret;
}

int dvd_safe_read(void *dst_v, u32 len, u64 offset)
{
    unsigned char buffer[DVD_SECTOR_SIZE]; // buffer for one dvd sector

    // if read size and length are a multiply of DVD_(OFFSET,LENGTH)_MULTIPLY and length < DVD_MAX_READ_LENGTH
    // we don't need to fix anything
    if(len % DVD_LENGTH_MULTIPLY == 0 && offset % DVD_OFFSET_MULTIPLY == 0 && len <= DVD_MAX_READ_LENGTH)
    {
        int ret = dvd_buffered_read(buffer, len, offset);
        memcpy(dst_v, &buffer, len);
        return ret;
    }
    else
    {
        // no errors yet -> ret = 0
        // the return value of dvd_read will be OR'd with ret
        // because dvd_read does return 1 on error and 0 on success and
        // because 0 | 1 = 1 ret will also contain 1 if at least one error
        // occured and 0 otherwise ;)
        int ret = 0; // return value of dvd_read

        // we might need to fix all 3 issues
        unsigned char *dst = (unsigned char *)dst_v; // gcc will not allow to use var[num] on void* types
        u64 bytesToRead; // the number of bytes we still need to read & copy to the output buffer
        u64 currentOffset; // the current dvd offset
        u64 bufferOffset; // the current buffer offset
        u64 i, j, k; // temporary variables which might be used for different stuff
        //	unsigned char buffer[DVD_SECTOR_SIZE]; // buffer for one dvd sector

        currentOffset = offset;
        bytesToRead = len;
        bufferOffset = 0;

        // fix first issue (offset is not a multiply of 32)
        if(offset % DVD_OFFSET_MULTIPLY)
        {
            // calculate offset of the prior 32 byte position
            i = currentOffset - (currentOffset % DVD_OFFSET_MULTIPLY);

            // calculate the offset from which the data of the dvd buffer will be copied
            j = currentOffset % DVD_OFFSET_MULTIPLY;

            // calculate the number of bytes needed to reach the next DVD_OFFSET_MULTIPLY byte mark
            k = DVD_OFFSET_MULTIPLY - j;

            // maybe we'll only need to copy a few bytes and we therefore don't even reach the next sector
            if(k > len)
            {
                k = len;
            }

            // read 32 bytes from the last 32 byte position
            ret |= dvd_buffered_read(buffer, DVD_OFFSET_MULTIPLY, i);

            // copy the bytes to the output buffer and update currentOffset, bufferOffset and bytesToRead
            memcpy(&dst[bufferOffset], &buffer[j], k);
            currentOffset += k;
            bufferOffset += k;
            bytesToRead -= k;
        }

        // fix second issue (more than 2048 bytes are needed)
        if(bytesToRead > DVD_MAX_READ_LENGTH)
        {
            // calculate the number of 2048 bytes sector needed to get all data
            i = (bytesToRead - (bytesToRead % DVD_MAX_READ_LENGTH)) / DVD_MAX_READ_LENGTH;

            // read data in 2048 byte sector
            for(j = 0; j < i; j++)
            {
                ret |= dvd_buffered_read(buffer, DVD_MAX_READ_LENGTH, currentOffset); // read sector
                memcpy(&dst[bufferOffset], buffer, DVD_MAX_READ_LENGTH); // copy to output buffer

                // update currentOffset, bufferOffset and bytesToRead
                currentOffset += DVD_MAX_READ_LENGTH;
                bufferOffset += DVD_MAX_READ_LENGTH;
                bytesToRead -= DVD_MAX_READ_LENGTH;
            }
        }

        // fix third issue (length is not a multiply of 32)
        if(bytesToRead)
        {
            ret |= dvd_buffered_read(buffer, DVD_MAX_READ_LENGTH, currentOffset); // read 32 byte from the dvd
            memcpy(&dst[bufferOffset], buffer, bytesToRead); // copy bytes to output buffer
        }
        return ret;
    }
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

/****************************************************************************
 * Primary Volume Descriptor
 *
 * The PVD should reside between sector 16 and 31.
 * This is for single session DVD only.
 ***************************************************************************/
int
getpvd ()
{
	int sector = 16;
	u32 rootdir32;
	unsigned char dvdbuffer[2048];

	dvddir = dvddirlength = 0;
	IsJoliet = -1;

	/** Look for Joliet PVD first **/
	while (sector < 32)
	{
		if (dvd_read (&dvdbuffer, 2048, (u64)(sector << 11)))
		{
			if (memcmp (&dvdbuffer, "\2CD001\1", 8) == 0)
			{
				memcpy(&rootdir32, &dvdbuffer[PVDROOT + EXTENT], 4);
				dvddir = (u64)rootdir32;
				dvddir <<= 11;
				memcpy (&dvddirlength, &dvdbuffer[PVDROOT + FILE_LENGTH], 4);
				dvdrootdir = dvddir;
				dvdrootlength = dvddirlength;
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
				dvddir = (u64)rootdir32;
				dvddir <<= 11;
				memcpy (&dvddirlength, &dvdbuffer[PVDROOT + FILE_LENGTH], 4);
				dvdrootdir = dvddir;
				dvdrootlength = dvddirlength;
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

/****************************************************************************
 * TestDVD()
 *
 * Tests if a ISO9660 DVD is inserted and available
 ***************************************************************************/
bool TestDVD()
{
	if (!getpvd())
	{
		#ifdef HW_DOL
		DVD_Mount();
		#elif WII_DVD
		DI_Mount();
		while(DI_GetStatus() & DVD_INIT);
		#endif
		if (!getpvd())
			return false;
	}

	return true;
}

/****************************************************************************
 * getentry
 *
 * Support function to return the next file entry, if any
 * Declared static to avoid accidental external entry.
 ***************************************************************************/
static int diroffset = 0;
static int
getentry (int entrycount, unsigned char dvdbuffer[])
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
					fname[MAXJOLIET - 1] = 0;

				if (strlen (fname) == 0)
					fname[0] = filename[0];
			}

			if (strlen (fname) == 0) // root entry
			{
				fname[0] = 0; // we'll skip it by setting the filename to 0 length
			}
			else
			{
				if (fname[0] == 1)
				{
					if(dvddir == dvdrootdir) // at root already, don't show ..
						fname[0] = 0;
					else
						strcpy (fname, "..");
				}
				else
				{
					/*
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
			fname[MAXDISPLAY - 1] = 0;
			strcpy (filelist[entrycount].displayname, fname);

			memcpy (&offset32, &dvdbuffer[diroffset + EXTENT], 4);

			filelist[entrycount].offset = (u64)offset32;
			memcpy (&filelist[entrycount].length, &dvdbuffer[diroffset + FILE_LENGTH], 4);
			memcpy (&filelist[entrycount].flags, &dvdbuffer[diroffset + FILE_FLAGS], 1);

			filelist[entrycount].offset <<= 11;
			filelist[entrycount].flags = filelist[entrycount].flags & 2;

			/*** Prepare for next entry ***/

			diroffset += dvdbuffer[diroffset];
			return 1;
		}
	}
	return 0;
}

/****************************************************************************
 * parseDVDdirectory
 *
 * This function will parse the directory tree.
 * It relies on dvddir and dvddirlength being pre-populated by a call to
 * getpvd, a previous parse or a menu selection.
 *
 * The return value is number of files collected, or 0 on failure.
 ***************************************************************************/
int
ParseDVDdirectory ()
{
	int pdlength;
	u64 pdoffset;
	u64 rdoffset;
	int len = 0;
	int filecount = 0;
	unsigned char dvdbuffer[2048];

	// initialize selection
	selection = offset = 0;

	pdoffset = rdoffset = dvddir;
	pdlength = dvddirlength;
	filecount = 0;

	// Clear any existing values
	memset (&filelist, 0, sizeof (FILEENTRIES) * MAXFILES);

	/*** Get as many files as possible ***/
	while (len < pdlength)
	{
		if (dvd_read (&dvdbuffer, 2048, pdoffset) == 0)
			return 0;

		diroffset = 0;

		while (getentry (filecount, dvdbuffer))
		{
			if(strlen(filelist[filecount].filename) > 0 && filecount < MAXFILES)
				filecount++;
		}

		len += 2048;
		pdoffset = rdoffset + len;
	}

	// Sort the file list
	qsort(filelist, filecount, sizeof(FILEENTRIES), FileSortCallback);

	return filecount;
}

/****************************************************************************
 * DirectorySearch
 *
 * Searches for the directory name specified within the current directory
 * Returns the index of the directory, or -1 if not found
 ***************************************************************************/
int DirectorySearch(char dir[512])
{
	for (int i = 0; i < maxfiles; i++ )
		if (strcmp(filelist[i].filename, dir) == 0)
			return i;
	return -1;
}

/****************************************************************************
 * SwitchDVDFolder
 *
 * Recursively searches for any directory path 'dir' specified
 * Also loads the directory contents via ParseDVDdirectory()
 * It relies on dvddir, dvddirlength, and filelist being pre-populated
 ***************************************************************************/
bool SwitchDVDFolder(char * dir, int maxDepth)
{
	if(maxDepth > 8) // only search to a max depth of 8 levels
		return false;

	bool lastdir = false;
	char * nextdir = NULL;
	unsigned int t = strcspn(dir, "/");

	if(t != strlen(dir))
		nextdir = dir + t + 1; // next directory path to find
	else
		lastdir = true;

	dir[t] = 0;

	int dirindex = DirectorySearch(dir);

	if(dirindex >= 0)
	{
		dvddir = filelist[dirindex].offset;
		dvddirlength = filelist[dirindex].length;
		selection = dirindex;

		if(filelist[dirindex].flags) // only parse directories
			maxfiles = ParseDVDdirectory();

		if(lastdir)
			return true;
		else
			return SwitchDVDFolder(nextdir, maxDepth++);
	}
	return false;
}

bool SwitchDVDFolder(char origdir[])
{
	// make a copy of origdir so we don't mess with original
	char dir[200];
	strcpy(dir, origdir);

	char * dirptr = dir;

	// strip off leading/trailing slashes on the directory path
	// we don't want to screw up our recursion!
	if(dir[0] == '/')
		dirptr = dirptr + 1;
	if(dir[strlen(dir)-1] == '/')
		dir[strlen(dir)-1] = 0;

	// start searching at root of DVD
	dvddir = dvdrootdir;
	dvddirlength = dvdrootlength;
	ParseDVDdirectory();

	return SwitchDVDFolder(dirptr, 0);
}

/****************************************************************************
 * LoadDVDFile
 * This function will load a file from DVD
 * The values for offset and length are inherited from dvddir and
 * dvddirlength.
 *
 * The buffer parameter should re-use the initial ROM buffer
 ***************************************************************************/

int
LoadDVDFileOffset (unsigned char *buffer, int length)
{
	int offset;
	int blocks;
	int i;
	u64 discoffset;
	char readbuffer[2048];

	// How many 2k blocks to read
	blocks = dvddirlength / 2048;
	offset = 0;
	discoffset = dvddir;
	ShowAction ((char*) "Loading...");

	if(length > 0 && length <= 2048)
	{
		dvd_read (buffer, length, discoffset);
	}
	else // load whole file
	{
		dvd_read (readbuffer, 2048, discoffset);

		if (IsZipFile (readbuffer))
		{
			return UnZipBuffer (buffer, METHOD_DVD); // unzip from dvd
		}
		else
		{
			for (i = 0; i < blocks; i++)
			{
				dvd_read (readbuffer, 2048, discoffset);
				memcpy (buffer + offset, readbuffer, 2048);
				offset += 2048;
				discoffset += 2048;
				ShowProgress ((char *)"Loading...", offset, length);
			}

			/*** And final cleanup ***/
			if (dvddirlength % 2048)
			{
				i = dvddirlength % 2048;
				dvd_read (readbuffer, 2048, discoffset);
				memcpy (buffer + offset, readbuffer, i);
			}
		}
	}
	return dvddirlength;
}

int
LoadDVDFile(char * buffer, char *filepath, int datasize, bool silent)
{
	if(SwitchDVDFolder(filepath))
	{
		return LoadDVDFileOffset ((unsigned char *)buffer, datasize);
	}
	else
	{
		if(!silent)
			WaitPrompt((char *)"Error loading file!");
		return 0;
	}
}

/****************************************************************************
 * uselessinquiry
 *
 * As the name suggests, this function is quite useless.
 * It's only purpose is to stop any pending DVD interrupts while we use the
 * memcard interface.
 *
 * libOGC tends to foul up if you don't, and sometimes does if you do!
 ***************************************************************************/
#ifdef HW_DOL
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

/****************************************************************************
 * dvd_motor_off( )
 * Turns off DVD drive motor so it doesn't make noise (Gamecube only)
 ***************************************************************************/
void dvd_motor_off ()
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

/****************************************************************************
 * dvd_driveid
 *
 * Gets and returns the dvd driveid
 ***************************************************************************/

int dvd_driveid()
{
	static unsigned char *inquiry=(unsigned char *)0x80000004;

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

#endif

/****************************************************************************
 * SetDVDDriveType()
 *
 * Sets the DVD drive ID for use to determine disc size (1.5 GB or 4.7 GB)
 ***************************************************************************/
void SetDVDDriveType()
{
	#ifdef HW_RVL
	isWii = true;
	#else
	int drvid = dvd_driveid ();
	if ( drvid == 4 || drvid == 6 || drvid == 8 )
		isWii = false;
	else
		isWii = true;
	#endif
}

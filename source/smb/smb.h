/****************************************************************************
 * TinySMB-GC
 *
 * Nintendo Gamecube SaMBa implementation.
 *
 * Copyright softdev@tehskeen.com
 *
 * Authentication modules, LMhash and DES are 
 *
 * Copyright Christopher R Hertel.
 * http://www.ubiqx.org
 *
 * You WILL find Ethereal, available from http://www.ethereal.com
 * invaluable for debugging each new SAMBA implementation.
 *
 * Recommended Reading
 *	Implementing CIFS - Christopher R Hertel
 *	SNIA CIFS Documentation - http://www.snia.org
 *
 * License:
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ****************************************************************************/
#ifndef NBTSMB_INC
#define NBTSMB_INC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * NBT/SMB Wrapper 
 */
typedef struct
{
  unsigned char msg;		 /*** NBT Message ***/
  unsigned char flags;		 /*** Not much here really ***/
  unsigned short length;	 /*** Length, excluding NBT ***/
  char smb[2916];		 /*** GC Actual is 2920 bytes ***/
}
NBTSMB;

/**
 * Session Information
 */
typedef struct
{
  unsigned short TID;
  unsigned short PID;
  unsigned short UID;
  unsigned short MID;
  unsigned short sKey;
  unsigned short MaxBuffer;
  unsigned short MaxMpx;
  unsigned short MaxVCS;
  unsigned char challenge[10];
  char p_domain[64];
  unsigned short sid;
  unsigned short count;
  unsigned short eos;
}
SMBSESSION;

/*** SMB_FILEENTRY
     SMB Long Filename Directory Entry
 ***/
typedef struct
{
  unsigned int size_low;
  unsigned int size_high;
  unsigned int attributes;
  char name[256];
}
SMBDIRENTRY;

/***
 * SMB File Handle
 */
typedef unsigned short SMBFILE;

#define SMB_HEADER 32		/*** SMB Headers are always 32 bytes long ***/
#define SMB_PROTO 0x424d53ff

/**
 * Field offsets.
 */
#define SMB_OFFSET_CMD      4
#define SMB_OFFSET_NTSTATUS 5
#define SMB_OFFSET_ECLASS   5
#define SMB_OFFSET_ECODE    7
#define SMB_OFFSET_FLAGS    9
#define SMB_OFFSET_FLAGS2  10
#define SMB_OFFSET_EXTRA   12
#define SMB_OFFSET_TID     24
#define SMB_OFFSET_PID     26
#define SMB_OFFSET_UID     28
#define SMB_OFFSET_MID     30

/**
 * Message / Commands
 */
#define SESS_MSG            0x00
#define SMB_NEG_PROTOCOL    0x72
#define SMB_SETUP_ANDX      0x73
#define SMB_TREEC_ANDX      0x75

/**
 * SMBTrans2 
 */
#define SMB_TRANS2           0x32

#define SMB_OPEN2            0
#define SMB_FIND_FIRST2      1
#define SMB_FIND_NEXT2       2
#define SMB_QUERY_FS_INFO    3
#define SMB_QUERY_PATH_INFO  5
#define SMB_SET_PATH_INFO    6
#define SMB_QUERY_FILE_INFO  7
#define SMB_SET_FILE_INFO    8
#define SMB_CREATE_DIR       13
#define SMB_FIND_CLOSE2      0x34

/**
 * File I/O
 */
#define SMB_OPEN_ANDX        0x2d
#define SMB_WRITE_ANDX       0x2f
#define SMB_READ_ANDX        0x2e
#define SMB_CLOSE            4

/**
 * SMB File Access Modes
 */
#define SMB_OPEN_READING     0
#define SMB_OPEN_WRITING     1
#define SMB_OPEN_READWRITE   2
#define SMB_OPEN_COMPATIBLE  0
#define SMB_DENY_READWRITE   0x10
#define SMB_DENY_WRITE       0x20
#define SMB_DENY_READ        0x30
#define SMB_DENY_NONE        0x40

/**
 * SMB File Open Function
 */
#define SMB_OF_OPEN             1
#define SMB_OF_TRUNCATE      2
#define SMB_OF_CREATE        16

/**
 * FileSearch
 */
#define SMB_SRCH_DIRECTORY	16
#define SMB_SRCH_READONLY  	1
#define SMB_SRCH_HIDDEN		2
#define SMB_SRCH_SYSTEM	        4
#define SMB_SRCH_VOLUME		8

/**
 * SMB Error codes
 */
#define SMB_SUCCESS         1
#define BAD_PROTOCOL        -1
#define SMB_ERROR           -2
#define SMB_BAD_COMMAND     -3
#define SMB_PROTO_FAIL      -4
#define SMB_NOT_USER        -5
#define SMB_BAD_KEYLEN      -6

/**
 * TRANS2 Offsets
 */
#define T2_WORD_CNT         SMB_HEADER
#define T2_PRM_CNT          T2_WORD_CNT + 1
#define T2_DATA_CNT         T2_PRM_CNT + 2
#define T2_MAXPRM_CNT       T2_DATA_CNT + 2
#define T2_MAXBUFFER        T2_MAXPRM_CNT + 2
#define T2_SETUP_CNT        T2_MAXBUFFER + 2
#define T2_SPRM_CNT         T2_SETUP_CNT + 10
#define T2_SPRM_OFS         T2_SPRM_CNT + 2
#define T2_SDATA_CNT        T2_SPRM_OFS + 2
#define T2_SDATA_OFS        T2_SDATA_CNT + 2
#define T2_SSETUP_CNT       T2_SDATA_OFS + 2
#define T2_SUB_CMD          T2_SSETUP_CNT + 2
#define T2_BYTE_CNT         T2_SUB_CMD + 2

/**
 * Prototypes
 */

/*** Session ***/
int SMB_Init (char *user, char *password, char *client,
	      char *server, char *share, char *IP);
void SMB_Destroy ();

/*** File Find ***/
int SMB_FindFirst (char *filename, unsigned short flags, SMBDIRENTRY * sdir);
int SMB_FindNext (SMBDIRENTRY * sdir);
int SMB_FindClose ();

/*** File I/O ***/
SMBFILE SMB_Open (char *filename, unsigned short access,
		  unsigned short creation);
void SMB_Close (SMBFILE sfid);
int SMB_Read (char *buffer, int size, int offset, SMBFILE sfile);
int SMB_Write (char *buffer, int size, int offset, SMBFILE sfile);

#endif

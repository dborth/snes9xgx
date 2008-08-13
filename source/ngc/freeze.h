/****************************************************************************
 * Snes9x 1.50
 *
 * Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Tantric August 2008
 *
 * freeze.h
 *
 * Snapshots Memory File System
 *
 * This is a single global memory file controller. Don't even think of opening two
 * at the same time !
 *
 * There's just enough here to do SnapShots - you should add anything else you
 * need.
 ****************************************************************************/

#ifndef _NGCMEMFILE_
#define _NGCMEMFILE_

typedef struct
{
  char filename[512];		/*** Way over size - but who cares -;) ***/
  int filehandle;
  int currpos;
  int length;
  int mode;
  char *buffer;			/*** Memspace for read / write ***/
}
MEMFILE;

int NGCFreezeGame (int method, bool8 silent);
int NGCUnfreezeGame (int method, bool8 silent);

#endif

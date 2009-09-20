/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007-July 2007
 * Michniewski 2008
 * Tantric August 2008
 *
 * freeze.h
 *
 * Snapshots Memory File System
 *
 * This is a single global memory file controller.
 * Don't even think of opening two at the same time!
 ***************************************************************************/

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

int NGCFreezeGame (char * filepath, int method, bool silent);
int NGCFreezeGameAuto (int method, bool silent);
int NGCUnfreezeGame (char * filepath, int method, bool silent);
int NGCUnfreezeGameAuto (int method, bool silent);

#endif

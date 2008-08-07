/****************************************************************************
 * Snes9x 1.50 - GX 2.0
 *
 * NGC Snapshots Memory File System
 *
 * This is a single global memory file controller. Don't even think of opening two
 * at the same time !
 *
 * There's just enough here to do SnapShots - you should add anything else you
 * need.
 ****************************************************************************/
#include "snes9x.h"

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

void quickLoadFreeze (bool8 silent);
void quickSaveFreeze (bool8 silent);

#endif

#ifndef _XBOX360_H_
#define _XBOX360_H_

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

bool XBOX360_ScanPads();
u32 XBOX360_ButtonsHeld(int chan);
char* XBOX360_Status();

#ifdef __cplusplus
}
#endif

#endif
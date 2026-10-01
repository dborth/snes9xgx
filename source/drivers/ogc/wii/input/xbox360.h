#ifndef _XBOX360_H_
#define _XBOX360_H_

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

void XBOX360_ScanPads(void);
u32 XBOX360_ButtonsHeld(int chan);
char* XBOX360_Status(void);

#ifdef __cplusplus
}
#endif

#endif
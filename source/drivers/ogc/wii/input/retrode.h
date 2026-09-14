#ifndef _RETRODE_H_
#define _RETRODE_H_

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Retrode_ScanPads();
u32 Retrode_ButtonsHeld(int chan);
char* Retrode_Status();

#ifdef __cplusplus
}
#endif

#endif

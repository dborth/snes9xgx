#ifndef _RETRODE_H_
#define _RETRODE_H_

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

void Retrode_ScanPads(void);
u32 Retrode_ButtonsHeld(int chan);
char* Retrode_Status(void);

#ifdef __cplusplus
}
#endif

#endif

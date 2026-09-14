#ifndef _HORNET_H_
#define _HORNET_H_

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Hornet_ScanPads();
u32 Hornet_ButtonsHeld(int chan);
char* Hornet_Status();

#ifdef __cplusplus
}
#endif

#endif

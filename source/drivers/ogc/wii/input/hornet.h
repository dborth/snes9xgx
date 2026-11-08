#ifndef _HORNET_H_
#define _HORNET_H_

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

void Hornet_ScanPads(void);
u32 Hornet_ButtonsHeld(int chan);

#ifdef __cplusplus
}
#endif

#endif

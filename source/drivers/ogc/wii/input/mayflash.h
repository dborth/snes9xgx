#ifndef _MAYFLASH_H_
#define _MAYFLASH_H_

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

void Mayflash_ScanPads(void);
u32 Mayflash_ButtonsHeld(int chan);

#ifdef __cplusplus
}
#endif

#endif

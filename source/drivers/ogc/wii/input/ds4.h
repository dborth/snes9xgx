#ifndef _DS4_H_
#define _DS4_H_

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DS4_BUTTON_UP        0x00001
#define DS4_BUTTON_DOWN      0x00002
#define DS4_BUTTON_LEFT      0x00004
#define DS4_BUTTON_RIGHT     0x00008
#define DS4_BUTTON_CROSS     0x00010
#define DS4_BUTTON_CIRCLE    0x00020
#define DS4_BUTTON_SQUARE    0x00040
#define DS4_BUTTON_TRIANGLE  0x00080
#define DS4_BUTTON_L1        0x00100
#define DS4_BUTTON_R1        0x00200
#define DS4_BUTTON_L2        0x00400
#define DS4_BUTTON_R2        0x00800
#define DS4_BUTTON_SHARE     0x01000
#define DS4_BUTTON_OPTIONS   0x02000
#define DS4_BUTTON_L3        0x04000
#define DS4_BUTTON_R3        0x08000
#define DS4_BUTTON_PS        0x10000
#define DS4_BUTTON_TOUCHPAD  0x20000

void DS4_ScanPads();
bool DS4_Connected(int chan);
u32 DS4_ButtonsHeld();
u32 DS4_ButtonsDown();
u32 DS4_ButtonsUp();
s16 DS4_lStickX();
s16 DS4_lStickY();
s16 DS4_rStickX();
s16 DS4_rStickY();
char* DS4_Status();

#ifdef __cplusplus
}
#endif

#endif

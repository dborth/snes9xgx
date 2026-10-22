#ifndef _XBOX360_H_
#define _XBOX360_H_

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// Native Xbox 360 button bits (the Guide button is not reported: it switches player)
#define XBOX360_BUTTON_UP     0x0001
#define XBOX360_BUTTON_DOWN   0x0002
#define XBOX360_BUTTON_LEFT   0x0004
#define XBOX360_BUTTON_RIGHT  0x0008
#define XBOX360_BUTTON_A      0x0010
#define XBOX360_BUTTON_B      0x0020
#define XBOX360_BUTTON_X      0x0040
#define XBOX360_BUTTON_Y      0x0080
#define XBOX360_BUTTON_LB     0x0100
#define XBOX360_BUTTON_RB     0x0200
#define XBOX360_BUTTON_LT     0x0400  // analog trigger, reported digitally past a threshold
#define XBOX360_BUTTON_RT     0x0800  // analog trigger, reported digitally past a threshold
#define XBOX360_BUTTON_BACK   0x1000
#define XBOX360_BUTTON_START  0x2000
#define XBOX360_BUTTON_L3     0x4000
#define XBOX360_BUTTON_R3     0x8000

void XBOX360_ScanPads(void);
bool XBOX360_Connected(int chan);
char* XBOX360_Status(void);

// Button state, latched once per XBOX360_ScanPads(). All of these return 0 for any
// channel other than the one the controller is currently assigned to.
u32 XBOX360_ButtonsHeld(int chan);
u32 XBOX360_ButtonsDown(int chan);
u32 XBOX360_ButtonsUp(int chan);

// Sticks, -128..127, positive Y is up
s16 XBOX360_lStickX(int chan);
s16 XBOX360_lStickY(int chan);
s16 XBOX360_rStickX(int chan);
s16 XBOX360_rStickY(int chan);

#ifdef __cplusplus
}
#endif

#endif

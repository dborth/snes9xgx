#ifndef _TABLET_H_
#define _TABLET_H_

/*
 * USB pen tablet (Wacom, XP-Pen, Huion, ...), read through usbinput.c like
 * the other USB controllers: no thread of its own, and nothing allocated
 * until a tablet is attached. Only tablets that describe their pen in their
 * HID report descriptor are supported.
 */

#ifdef HW_RVL

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TABLET_BUTTON_TIP     0x01 // the nib (or eraser) touching the surface
#define TABLET_BUTTON_BARREL  0x02 // lower side switch
#define TABLET_BUTTON_BARREL2 0x04 // upper side switch

void Tablet_ScanPads(void);
bool Tablet_Connected(void);
// The pen is near enough to be tracked: it owns the pointer
bool Tablet_InRange(void);
// The pen's position over the tablet's area, 0..1
float Tablet_X(void);
float Tablet_Y(void);
u32 Tablet_ButtonsHeld(void);
u32 Tablet_ButtonsDown(void);
u32 Tablet_ButtonsUp(void);
char* Tablet_Status(void);

#ifdef __cplusplus
}
#endif

#endif // HW_RVL
#endif

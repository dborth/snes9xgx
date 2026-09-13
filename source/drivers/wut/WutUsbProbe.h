/****************************************************************************
 * libgui - drivers/wut
 * Daryl Borth 2026
 * WutUsbProbe.h
 *
 * USB MSD probing. Used to enable hot-plugging support.
 ***************************************************************************/
#pragma once
#include <stdint.h>

//! One USBCLASS_STORAGE interface as seen by a single
//! ScanUsbHardwareSignature() call. Not a per-port identity (see above) -
//! ifHandle is only meaningful within one boot session and only for
//! detecting that *something* changed, not attributing it to a slot.
struct UsbHardwareInterfaceInfo
{
	uint32_t ifHandle;
	uint16_t vid;
	uint16_t pid;
};

//! The set of USBCLASS_STORAGE interfaces nsysuhs currently reports
//! across every controller_num probed. Deliberately small/fixed-size -
//! this is compared every poll cycle, not stored long-term.
struct UsbHardwareSignature
{
	static const int kMaxInterfaces = 8;
	UsbHardwareInterfaceInfo interfaces[kMaxInterfaces];
	int count = 0;
};

//! Cheap, read-only nsysuhs scan across all controllers, filtered
//! client-side to USBCLASS_STORAGE. No logging, no allocation beyond one
//! small stack-sized UHS config buffer, never touches Mocha/FSA state -
//! safe and cheap enough to call every pollStorageDevices() cycle.
void ScanUsbHardwareSignature(UsbHardwareSignature & out);

//! Unordered comparison by ifHandle - true if the set of attached
//! storage interfaces differs between a and b (an insertion, a removal,
//! or both), false if identical. Order and array position don't matter.
bool UsbHardwareSignatureChanged(const UsbHardwareSignature & a, const UsbHardwareSignature & b);

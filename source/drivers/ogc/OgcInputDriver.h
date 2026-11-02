/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcInputDriver.h
 ***************************************************************************/
#pragma once
#include "../InputDriver.h"
#include "../MousePointer.h"

class OgcInputDriver : public InputDriver
{
public:
    OgcInputDriver();
    ~OgcInputDriver() override;

    void init() override;
    void shutdown() override;
    void update() override;
    void setRumble(int channel, bool rumble) override;
    InputUsbStatus getUsbStatus() override;

private:
    bool rumbleRequest[4];
    int menuRumbleFrames[4];     // frames left in the current menu "tick" (0 = idle)
    int menuRumbleGapFrames[4];  // frames left in the enforced silent gap after a tick
    uint32_t prevGenericUsbHeld[4]; // previous-frame held state of the generic USB adapters, for down/up edges
    MousePointer mousePointer;   // the USB mouse as a pointer on channel 0 (see Platform::getMouse())
};

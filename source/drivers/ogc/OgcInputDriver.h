/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcInputDriver.h
 ***************************************************************************/
#pragma once
#include "../InputDriver.h"

class OgcInputDriver : public InputDriver
{
public:
    OgcInputDriver();
    ~OgcInputDriver() override;

    void init() override;
    void shutdown() override;
    void update() override;
    void setRumble(int channel, bool rumble) override;

private:
    bool rumbleRequest[4];
    int menuRumbleFrames[4];     // frames left in the current menu "tick" (0 = idle)
    int menuRumbleGapFrames[4];  // frames left in the enforced silent gap after a tick
};

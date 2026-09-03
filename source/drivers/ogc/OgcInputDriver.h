/****************************************************************************
 * libgui - drivers/ogc
 * Daryl Borth 2009-2026
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
    int rumbleCount[4];
    bool rumbleRequest[4];
};

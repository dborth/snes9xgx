/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * InputDriver.h
 *
 * Platform input backend GuiElements delegate to.
 * Exactly one driver implements this and assigns the single global instance.
 ***************************************************************************/
#pragma once

class InputDriver
{
public:
    virtual ~InputDriver() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;

    //! Polls the hardware and dispatches GuiInputPadData payloads to the UI
    virtual void update(float deltaTime) = 0;

    //! Requests a rumble event on the specified controller channel
    virtual void setRumble(int channel, bool rumble) = 0;
};

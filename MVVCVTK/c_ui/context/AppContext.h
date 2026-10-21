#pragma once

#include "c_ui/command/Commands.h"
#include "c_ui/context/SessionManager.h"
#include "c_ui/context/VolumeManager.h"

class AppContext
{
public:
    Commands& getCommands()
    {
        return commands_;
    }

    const Commands& getCommands() const
    {
        return commands_;
    }

    SessionManager& getSessionManager()
    {
        return sessionManager_;
    }

    const SessionManager& getSessionManager() const
    {
        return sessionManager_;
    }

    VolumeManager& getVolumeManager()
    {
        return volumeManager_;
    }

    const VolumeManager& getVolumeManager() const
    {
        return volumeManager_;
    }

    bool hasData() const
    {
        return sessionManager_.gethasData();
    }

private:
    Commands commands_;
    SessionManager sessionManager_;
    VolumeManager volumeManager_;
};
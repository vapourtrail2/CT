#pragma once

#include "c_ui/command/Commands.h"
#include "c_ui/context/SessionManager.h"

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

    bool hasData() const
    {
        return sessionManager_.gethasData();
    }

private:
    Commands commands_;
    SessionManager sessionManager_;
};
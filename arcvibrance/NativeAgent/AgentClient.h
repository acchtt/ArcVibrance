#pragma once

#include "AgentProtocol.h"

namespace ArcVibrance
{
class AgentClient
{
public:
    bool EnsureRunning();
    bool GetStatus(AgentStatus& status) const;
    bool ReloadProfiles(AgentStatus* status = nullptr) const;
    bool ReapplyActiveProfile(AgentStatus* status = nullptr) const;
    bool RestoreDesktop(AgentStatus* status = nullptr) const;
    bool ShutdownAgent() const;

private:
    bool Send(AgentCommand command, AgentResponse& response) const;
};
}

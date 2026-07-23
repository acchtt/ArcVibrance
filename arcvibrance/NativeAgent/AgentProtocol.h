#pragma once

#include <Windows.h>

namespace ArcVibrance
{
inline constexpr wchar_t AGENT_PIPE_NAME[] = L"\\\\.\\pipe\\ArcVibrance.Agent.v1";
inline constexpr wchar_t AGENT_MUTEX_NAME[] = L"Local\\ArcVibrance.Agent.v1";
inline constexpr wchar_t UI_CLOSE_EVENT_NAME[] = L"Local\\ArcVibrance.Ui.Close.v1";
inline constexpr unsigned long AGENT_PROTOCOL_VERSION = 1;
inline constexpr UINT WM_ARCVIBRANCE_FORCE_CLOSE_UI = WM_APP + 30;

enum class AgentCommand : unsigned long
{
    GetStatus = 1,
    ReloadProfiles = 2,
    ReapplyActiveProfile = 3,
    RestoreDesktop = 4,
    Shutdown = 5
};

enum class AgentError : unsigned long
{
    None = 0,
    InvalidRequest = 1,
    UnsupportedProtocol = 2,
    RuntimeFailure = 3,
    ProfileLoadFailure = 4
};

struct AgentRequest
{
    unsigned long size = sizeof(AgentRequest);
    unsigned long protocolVersion = AGENT_PROTOCOL_VERSION;
    AgentCommand command = AgentCommand::GetStatus;
    unsigned long requestId = 0;
};

struct AgentStatus
{
    unsigned long size = sizeof(AgentStatus);
    unsigned long protocolVersion = AGENT_PROTOCOL_VERSION;
    BOOL agentRunning = FALSE;
    BOOL runtimeInitialized = FALSE;
    BOOL gameActive = FALSE;
    int activeProfileIndex = -1;
    int appliedSaturationPercent = 100;
    wchar_t activeExecutableName[260] = {};
};

struct AgentResponse
{
    unsigned long size = sizeof(AgentResponse);
    unsigned long protocolVersion = AGENT_PROTOCOL_VERSION;
    unsigned long requestId = 0;
    BOOL success = FALSE;
    AgentError error = AgentError::None;
    wchar_t errorMessage[160] = {};
    AgentStatus status{};
};
}

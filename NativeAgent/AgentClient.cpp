#include "AgentClient.h"

#include <filesystem>
#include <string>
#include <atomic>

namespace ArcVibrance
{
namespace
{
std::atomic<unsigned long> g_nextRequestId{1};

bool GetAgentPath(std::wstring& path)
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return false;
    }

    std::filesystem::path executablePath(modulePath);
    executablePath.replace_filename(L"ArcVibrance.Agent.exe");
    path = executablePath.wstring();
    return true;
}
}

bool AgentClient::EnsureRunning()
{
    AgentStatus status{};
    if (GetStatus(status))
    {
        return true;
    }

    std::wstring agentPath;
    if (!GetAgentPath(agentPath))
    {
        return false;
    }

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    std::wstring commandLine = L"\"" + agentPath + L"\"";

    const BOOL created = CreateProcessW(
        agentPath.c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo);

    if (!created)
    {
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    for (int attempt = 0; attempt < 20; ++attempt)
    {
        if (WaitNamedPipeW(AGENT_PIPE_NAME, 100) && GetStatus(status))
        {
            return true;
        }
        Sleep(50);
    }

    return false;
}

bool AgentClient::GetStatus(AgentStatus& status) const
{
    AgentResponse response{};
    if (!Send(AgentCommand::GetStatus, response))
    {
        return false;
    }
    status = response.status;
    return response.success != FALSE;
}

bool AgentClient::ReloadProfiles(AgentStatus* status) const
{
    AgentResponse response{};
    const bool result = Send(AgentCommand::ReloadProfiles, response) && response.success;
    if (status != nullptr) *status = response.status;
    return result;
}

bool AgentClient::ReapplyActiveProfile(AgentStatus* status) const
{
    AgentResponse response{};
    const bool result = Send(AgentCommand::ReapplyActiveProfile, response) && response.success;
    if (status != nullptr) *status = response.status;
    return result;
}

bool AgentClient::RestoreDesktop(AgentStatus* status) const
{
    AgentResponse response{};
    const bool result = Send(AgentCommand::RestoreDesktop, response) && response.success;
    if (status != nullptr) *status = response.status;
    return result;
}

bool AgentClient::ShutdownAgent() const
{
    AgentResponse response{};
    return Send(AgentCommand::Shutdown, response) && response.success;
}

bool AgentClient::Send(AgentCommand command, AgentResponse& response) const
{
    AgentRequest request{};
    request.command = command;
    request.requestId = g_nextRequestId.fetch_add(1);

    DWORD bytesRead = 0;
    const BOOL result = CallNamedPipeW(
        AGENT_PIPE_NAME,
        &request,
        sizeof(request),
        &response,
        sizeof(response),
        &bytesRead,
        750);

    return result &&
        bytesRead == sizeof(response) &&
        response.size == sizeof(response) &&
        response.protocolVersion == AGENT_PROTOCOL_VERSION &&
        response.requestId == request.requestId &&
        response.status.size == sizeof(AgentStatus) &&
        response.status.protocolVersion == AGENT_PROTOCOL_VERSION;
}
}

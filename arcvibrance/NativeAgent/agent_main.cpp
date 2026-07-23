#include "AgentProtocol.h"
#include "AgentTray.h"
#include "ArcVibranceRuntime.h"
#include "ForegroundWatcher.h"
#include "Settings.h"
#include "Logger.h"

#include <Windows.h>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <filesystem>
#include <string>

namespace ArcVibrance
{
namespace
{
constexpr wchar_t kWindowClass[] = L"ArcVibranceAgentWindow";
constexpr UINT WM_AGENT_FOREGROUND_CHANGED = WM_APP + 20;
constexpr UINT_PTR kRecoveryTimerId = 1;
constexpr UINT_PTR kBackendRetryTimerId = 2;
constexpr UINT_PTR kTrayRetryTimerId = 3;
constexpr UINT kRecoveryIntervalMs = 2000;
constexpr UINT kInitialBackendDelayMs = 100;
constexpr UINT kBackendRetryIntervalMs = 10000;
constexpr UINT kTrayRetryIntervalMs = 2000;

ArcVibranceRuntime g_agentRuntime;
ForegroundWatcher g_agentWatcher;
std::vector<GameProfile> g_agentProfiles;
std::atomic<bool> g_pipeRunning{true};
std::mutex g_runtimeMutex;
HWND g_agentWindow = nullptr;

void CloseUiIfRunning()
{
    HANDLE closeEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, UI_CLOSE_EVENT_NAME);
    if (closeEvent != nullptr)
    {
        Log(LogLevel::Info, L"Closing settings UI");
        SetEvent(closeEvent);
        CloseHandle(closeEvent);
    }
}

bool LaunchOrShowUi()
{
    // The WinUI frontend is single-instanced. Starting ArcVibrance.exe again
    // redirects activation to the existing window and brings it to the front.
    Log(LogLevel::Info, L"Opening settings UI");
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return false;

    std::filesystem::path uiPath(modulePath);
    uiPath.replace_filename(L"ArcVibrance.exe");
    std::wstring commandLine = L"\"" + uiPath.wstring() + L"\"";

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    const BOOL created = CreateProcessW(
        uiPath.c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo);

    if (!created)
    {
        LogLastError(LogLevel::Error, L"CreateProcessW failed for UI", GetLastError());
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}

void NotifyIfStatusChanged(const RuntimeStatus& before, const RuntimeStatus& after)
{
    if (before.gameActive == after.gameActive &&
        before.appliedSaturationPercent == after.appliedSaturationPercent &&
        before.activeExecutableName == after.activeExecutableName) return;
    if (after.gameActive)
        ShowAgentTrayNotification(L"Game detected", after.activeExecutableName + L"\nSaturation set to " +
            std::to_wstring(after.appliedSaturationPercent) + L"%");
    else if (before.gameActive)
        ShowAgentTrayNotification(L"Desktop restored", L"Saturation returned to 100%");
}

AgentStatus MakeStatus()
{
    std::lock_guard<std::mutex> lock(g_runtimeMutex);
    AgentStatus result{};
    result.agentRunning = TRUE;
    const RuntimeStatus& runtimeStatus = g_agentRuntime.GetStatus();
    result.runtimeInitialized = runtimeStatus.initialized ? TRUE : FALSE;
    result.gameActive = runtimeStatus.gameActive ? TRUE : FALSE;
    result.activeProfileIndex = runtimeStatus.activeProfileIndex;
    result.appliedSaturationPercent = runtimeStatus.appliedSaturationPercent;
    wcsncpy_s(result.activeExecutableName,
        runtimeStatus.activeExecutableName.c_str(),
        _TRUNCATE);
    return result;
}

bool ReloadProfiles()
{
    std::vector<GameProfile> profiles;
    if (!LoadGameProfiles(profiles))
    {
        Log(LogLevel::Error, L"Failed to reload profiles");
        return false;
    }

    std::lock_guard<std::mutex> lock(g_runtimeMutex);
    g_agentRuntime.RestoreDesktop();
    g_agentProfiles = std::move(profiles);
    g_agentRuntime.AttachProfiles(&g_agentProfiles);
    g_agentRuntime.HandleForegroundWindow();
    Log(LogLevel::Info, L"Profiles reloaded: " + std::to_wstring(g_agentProfiles.size()));
    return true;
}

void PipeServer()
{
    while (g_pipeRunning.load())
    {
        HANDLE pipe = CreateNamedPipeW(
            AGENT_PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            sizeof(AgentResponse),
            sizeof(AgentRequest),
            0,
            nullptr);

        if (pipe == INVALID_HANDLE_VALUE)
        {
            LogLastError(LogLevel::Error, L"CreateNamedPipeW failed", GetLastError());
            return;
        }

        const BOOL connected = ConnectNamedPipe(pipe, nullptr) ||
            GetLastError() == ERROR_PIPE_CONNECTED;

        if (connected)
        {
            AgentRequest request{};
            DWORD bytesRead = 0;
            AgentResponse response{};

            if (ReadFile(pipe, &request, sizeof(request), &bytesRead, nullptr) &&
                bytesRead == sizeof(request))
            {
                response.requestId = request.requestId;

                if (request.size != sizeof(request))
                {
                    response.error = AgentError::InvalidRequest;
                    wcsncpy_s(response.errorMessage, L"Invalid request size", _TRUNCATE);
                }
                else if (request.protocolVersion != AGENT_PROTOCOL_VERSION)
                {
                    response.error = AgentError::UnsupportedProtocol;
                    wcsncpy_s(response.errorMessage, L"Unsupported IPC protocol version", _TRUNCATE);
                }
                else switch (request.command)
                {
                case AgentCommand::GetStatus:
                    response.success = TRUE;
                    break;
                case AgentCommand::ReloadProfiles:
                    response.success = ReloadProfiles() ? TRUE : FALSE;
                    if (!response.success)
                    {
                        response.error = AgentError::ProfileLoadFailure;
                        wcsncpy_s(response.errorMessage, L"Could not load profiles", _TRUNCATE);
                    }
                    break;
                case AgentCommand::ReapplyActiveProfile:
                    {
                        std::lock_guard<std::mutex> lock(g_runtimeMutex);
                        response.success = g_agentRuntime.ReapplyActiveProfile() ? TRUE : FALSE;
                    }
                    break;
                case AgentCommand::RestoreDesktop:
                    {
                        std::lock_guard<std::mutex> lock(g_runtimeMutex);
                        response.success = g_agentRuntime.RestoreDesktop() ? TRUE : FALSE;
                    }
                    break;
                case AgentCommand::Shutdown:
                    response.success = TRUE;
                    g_pipeRunning.store(false);
                    PostMessageW(g_agentWindow, WM_CLOSE, 0, 0);
                    break;
                default:
                    response.success = FALSE;
                    response.error = AgentError::InvalidRequest;
                    wcsncpy_s(response.errorMessage, L"Unknown command", _TRUNCATE);
                    break;
                }

                if (!response.success && response.error == AgentError::None)
                {
                    response.error = AgentError::RuntimeFailure;
                    wcsncpy_s(response.errorMessage, L"Runtime command failed", _TRUNCATE);
                }
                response.status = MakeStatus();
                DWORD bytesWritten = 0;
                WriteFile(pipe, &response, sizeof(response), &bytesWritten, nullptr);
                FlushFileBuffers(pipe);
            }
        }

        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

LRESULT CALLBACK AgentWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_AGENT_FOREGROUND_CHANGED:
        {
            std::lock_guard<std::mutex> lock(g_runtimeMutex);
            const RuntimeStatus before = g_agentRuntime.GetStatus();
            g_agentRuntime.HandleForegroundWindow(reinterpret_cast<HWND>(wParam));
            NotifyIfStatusChanged(before, g_agentRuntime.GetStatus());
        }
        return 0;
    case WM_AGENT_TRAY:
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONDBLCLK: LaunchOrShowUi(); return 0;
        case WM_CONTEXTMENU:
        case WM_RBUTTONUP: ShowAgentTrayMenu(window); return 0;
        default: return 0;
        }
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_AGENT_TRAY_SHOW) { LaunchOrShowUi(); return 0; }
        if (LOWORD(wParam) == ID_AGENT_TRAY_EXIT)
        {
            Log(LogLevel::Info, L"Exit requested from tray");
            CloseUiIfRunning();
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_TIMER:
        if (wParam == kRecoveryTimerId)
        {
            std::lock_guard<std::mutex> lock(g_runtimeMutex);
            if (g_agentRuntime.GetStatus().initialized)
            {
                g_agentRuntime.HandleForegroundWindow();
            }
            return 0;
        }
        if (wParam == kTrayRetryTimerId)
        {
            if (AddAgentTrayIcon(window))
            {
                KillTimer(window, kTrayRetryTimerId);
                Log(LogLevel::Info, L"Tray icon created after Explorer became ready");
            }
            return 0;
        }
        if (wParam == kBackendRetryTimerId)
        {
            // Treat this timer as one-shot. If initialization fails, re-arm it
            // with a slower retry interval while the tray and IPC stay alive.
            KillTimer(window, kBackendRetryTimerId);
            bool initialized = false;
            {
                std::lock_guard<std::mutex> lock(g_runtimeMutex);
                initialized = g_agentRuntime.GetStatus().initialized || g_agentRuntime.Initialize();
                if (initialized)
                {
                    g_agentRuntime.HandleForegroundWindow();
                }
            }

            if (initialized)
            {
                Log(LogLevel::Info, L"Intel color backend initialized");
            }
            else
            {
                Log(LogLevel::Warning, L"Intel color backend is unavailable; the tray agent will retry");
                SetTimer(window, kBackendRetryTimerId, kBackendRetryIntervalMs, nullptr);
            }
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        RemoveAgentTrayIcon();
        g_agentWatcher.Stop();
        KillTimer(window, kRecoveryTimerId);
        KillTimer(window, kBackendRetryTimerId);
        KillTimer(window, kTrayRetryTimerId);
        {
            std::lock_guard<std::mutex> lock(g_runtimeMutex);
            g_agentRuntime.Shutdown();
        }
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    using namespace ArcVibrance;

    InitializeLogging();
    Log(LogLevel::Info, L"ArcVibrance Agent starting");

    HANDLE mutex = CreateMutexW(nullptr, TRUE, AGENT_MUTEX_NAME);
    if (mutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (mutex != nullptr) CloseHandle(mutex);
        Log(LogLevel::Info, L"Agent already running; exiting duplicate instance");
        ShutdownLogging();
        return 0;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = AgentWindowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&windowClass))
    {
        CloseHandle(mutex);
        return 1;
    }

    g_agentWindow = CreateWindowExW(0, kWindowClass, L"ArcVibrance Agent", 0,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
    if (g_agentWindow == nullptr)
    {
        CloseHandle(mutex);
        return 1;
    }

    if (!LoadGameProfiles(g_agentProfiles))
    {
        Log(LogLevel::Warning, L"Profiles could not be loaded; starting with an empty list");
        g_agentProfiles.clear();
    }
    else
    {
        Log(LogLevel::Info, L"Loaded profiles: " + std::to_wstring(g_agentProfiles.size()));
    }
    g_agentRuntime.AttachProfiles(&g_agentProfiles);

    // Create the tray icon and IPC server before initializing IGCL. The agent
    // therefore remains usable and can reopen the UI even when the graphics
    // backend is temporarily unavailable during sign-in or driver startup.
    if (!AddAgentTrayIcon(g_agentWindow))
    {
        // Explorer's notification area may not be ready yet when the Run entry
        // starts us during sign-in. Keep IPC and monitoring alive and retry the
        // tray icon instead of terminating the entire background service.
        Log(LogLevel::Warning, L"Explorer is not ready for the tray icon; retrying in the background");
        SetTimer(g_agentWindow, kTrayRetryTimerId, kTrayRetryIntervalMs, nullptr);
    }

    std::thread pipeThread(PipeServer);

    if (!g_agentWatcher.Start(g_agentWindow, WM_AGENT_FOREGROUND_CHANGED))
    {
        Log(LogLevel::Warning, L"Foreground watcher could not be started; recovery polling remains active");
    }
    SetTimer(g_agentWindow, kRecoveryTimerId, kRecoveryIntervalMs, nullptr);
    SetTimer(g_agentWindow, kBackendRetryTimerId, kInitialBackendDelayMs, nullptr);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    g_pipeRunning.store(false);
    // Wake a blocked ConnectNamedPipe call so the server thread can exit.
    HANDLE wakePipe = CreateFileW(AGENT_PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (wakePipe != INVALID_HANDLE_VALUE) CloseHandle(wakePipe);
    if (pipeThread.joinable()) pipeThread.join();

    Log(LogLevel::Info, L"ArcVibrance Agent stopped cleanly");
    CloseHandle(mutex);
    ShutdownLogging();
    return static_cast<int>(message.wParam);
}

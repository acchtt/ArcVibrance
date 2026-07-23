#pragma once

#include <Windows.h>

namespace ArcVibrance
{
class ForegroundWatcher
{
public:
    ForegroundWatcher() = default;
    ~ForegroundWatcher();

    ForegroundWatcher(const ForegroundWatcher&) = delete;
    ForegroundWatcher& operator=(const ForegroundWatcher&) = delete;

    bool Start(HWND notificationWindow, UINT notificationMessage);
    void Stop();
    bool IsRunning() const;

private:
    static void CALLBACK WinEventCallback(
        HWINEVENTHOOK hook,
        DWORD event,
        HWND window,
        LONG objectId,
        LONG childId,
        DWORD eventThread,
        DWORD eventTime);

    HWINEVENTHOOK hook_ = nullptr;
    HWND notificationWindow_ = nullptr;
    UINT notificationMessage_ = 0;

    static ForegroundWatcher* activeWatcher_;
};
}

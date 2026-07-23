#include "ForegroundWatcher.h"

namespace ArcVibrance
{
ForegroundWatcher* ForegroundWatcher::activeWatcher_ = nullptr;

ForegroundWatcher::~ForegroundWatcher()
{
    Stop();
}

bool ForegroundWatcher::Start(
    HWND notificationWindow,
    UINT notificationMessage)
{
    Stop();

    if (notificationWindow == nullptr ||
        notificationMessage < WM_APP)
    {
        return false;
    }

    notificationWindow_ = notificationWindow;
    notificationMessage_ = notificationMessage;
    activeWatcher_ = this;

    hook_ = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        nullptr,
        &ForegroundWatcher::WinEventCallback,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    if (hook_ == nullptr)
    {
        activeWatcher_ = nullptr;
        notificationWindow_ = nullptr;
        notificationMessage_ = 0;
        return false;
    }

    return true;
}

void ForegroundWatcher::Stop()
{
    if (hook_ != nullptr)
    {
        UnhookWinEvent(hook_);
        hook_ = nullptr;
    }

    if (activeWatcher_ == this)
    {
        activeWatcher_ = nullptr;
    }

    notificationWindow_ = nullptr;
    notificationMessage_ = 0;
}

bool ForegroundWatcher::IsRunning() const
{
    return hook_ != nullptr;
}

void CALLBACK ForegroundWatcher::WinEventCallback(
    HWINEVENTHOOK,
    DWORD event,
    HWND window,
    LONG objectId,
    LONG childId,
    DWORD,
    DWORD)
{
    ForegroundWatcher* watcher = activeWatcher_;

    if (watcher == nullptr ||
        event != EVENT_SYSTEM_FOREGROUND ||
        objectId != OBJID_WINDOW ||
        childId != CHILDID_SELF ||
        watcher->notificationWindow_ == nullptr)
    {
        return;
    }

    PostMessageW(
        watcher->notificationWindow_,
        watcher->notificationMessage_,
        reinterpret_cast<WPARAM>(window),
        0);
}
}

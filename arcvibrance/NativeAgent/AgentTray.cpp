#include "AgentTray.h"
#include "resource.h"
#include <Shellapi.h>

namespace ArcVibrance
{
namespace
{
NOTIFYICONDATAW data{};
bool added = false;
}

bool AddAgentTrayIcon(HWND owner)
{
    if (added)
    {
        return true;
    }

    data = {};
    data.cbSize = sizeof(data);
    data.hWnd = owner;
    data.uID = 1;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = WM_AGENT_TRAY;

    const auto instance = reinterpret_cast<HINSTANCE>(
        GetWindowLongPtrW(owner, GWLP_HINSTANCE));
    data.hIcon = static_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_ARCVIBRANCE),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    if (data.hIcon == nullptr)
    {
        return false;
    }

    wcscpy_s(data.szTip, L"ArcVibrance");
    if (!Shell_NotifyIconW(NIM_ADD, &data))
    {
        return false;
    }

    added = true;
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    return true;
}

void RemoveAgentTrayIcon()
{
    if (!added)
    {
        return;
    }

    Shell_NotifyIconW(NIM_DELETE, &data);
    added = false;
}

void ShowAgentTrayMenu(HWND owner)
{
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }

    AppendMenuW(menu, MF_STRING | MF_DEFAULT, ID_AGENT_TRAY_SHOW, L"Open ArcVibrance");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_AGENT_TRAY_EXIT, L"Exit ArcVibrance");

    POINT point{};
    GetCursorPos(&point);
    SetForegroundWindow(owner);
    TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        point.x,
        point.y,
        0,
        owner,
        nullptr);
    DestroyMenu(menu);
}

void ShowAgentTrayNotification(const std::wstring& title, const std::wstring& message)
{
    if (!added)
    {
        return;
    }

    NOTIFYICONDATAW notification = data;
    notification.uFlags = NIF_INFO;
    wcsncpy_s(notification.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(notification.szInfo, message.c_str(), _TRUNCATE);
    notification.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &notification);
}
}

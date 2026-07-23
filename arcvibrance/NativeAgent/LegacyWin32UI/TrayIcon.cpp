#include "TrayIcon.h"
#include "AppState.h"
#include "resource.h"
#include <Shellapi.h>

namespace ArcVibrance
{
    bool AddTrayIcon(HWND window)
    {
        g_trayIconData = {};
        g_trayIconData.cbSize = sizeof(g_trayIconData);
        g_trayIconData.hWnd = window;
        g_trayIconData.uID = TRAY_ICON_ID;
        g_trayIconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        g_trayIconData.uCallbackMessage = WM_TRAY_ICON;
        HINSTANCE instance =
            reinterpret_cast<HINSTANCE>(
                GetWindowLongPtrW(
                    window,
                    GWLP_HINSTANCE));

        g_trayIconData.hIcon =
            LoadIconW(
                instance,
                MAKEINTRESOURCEW(
                    IDI_ARCVIBRANCE));

        wcscpy_s(
            g_trayIconData.szTip,
            _countof(g_trayIconData.szTip),
            L"ArcVibrance");

        if (!Shell_NotifyIconW(
            NIM_ADD,
            &g_trayIconData))
        {
            return false;
        }

        g_trayIconAdded = true;

        g_trayIconData.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(
            NIM_SETVERSION,
            &g_trayIconData);

        return true;
    }

    void RemoveTrayIcon()
    {
        if (!g_trayIconAdded)
        {
            return;
        }

        Shell_NotifyIconW(
            NIM_DELETE,
            &g_trayIconData);

        g_trayIconAdded = false;
    }

    void ShowMainWindow(HWND window)
    {
        ShowWindow(window, SW_RESTORE);
        SetForegroundWindow(window);
    }

    void ShowTrayMenu(HWND window)
    {
        HMENU menu = CreatePopupMenu();

        if (menu == nullptr)
        {
            return;
        }

        AppendMenuW(
            menu,
            MF_STRING | MF_DEFAULT,
            ID_TRAY_SHOW,
            L"Show ArcVibrance");

        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

        AppendMenuW(
            menu,
            MF_STRING,
            ID_TRAY_EXIT,
            L"Exit");

        POINT cursorPosition = {};
        GetCursorPos(&cursorPosition);

        SetForegroundWindow(window);

        TrackPopupMenu(
            menu,
            TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
            cursorPosition.x,
            cursorPosition.y,
            0,
            window,
            nullptr);

        DestroyMenu(menu);
    }
    void ShowTrayNotification(
        const std::wstring& title,
        const std::wstring& message)
    {
        if (!g_trayIconAdded)
        {
            return;
        }

        NOTIFYICONDATAW notification =
            g_trayIconData;

        notification.uFlags = NIF_INFO;

        wcsncpy_s(
            notification.szInfoTitle,
            title.c_str(),
            _TRUNCATE);

        wcsncpy_s(
            notification.szInfo,
            message.c_str(),
            _TRUNCATE);

        notification.dwInfoFlags =
            NIIF_INFO;

        Shell_NotifyIconW(
            NIM_MODIFY,
            &notification);
    }
}
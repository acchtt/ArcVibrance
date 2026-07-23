#pragma once

#include <windows.h>
#include <string>
namespace ArcVibrance
{
    bool AddTrayIcon(HWND window);
    void RemoveTrayIcon();
    void ShowMainWindow(HWND window);
    void ShowTrayMenu(HWND window);
    void ShowTrayNotification(
        const std::wstring& title,
        const std::wstring& message);
}
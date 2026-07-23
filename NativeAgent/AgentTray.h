#pragma once
#include <Windows.h>
#include <string>
namespace ArcVibrance {
inline constexpr UINT WM_AGENT_TRAY = WM_APP + 21;
inline constexpr UINT ID_AGENT_TRAY_SHOW = 2401;
inline constexpr UINT ID_AGENT_TRAY_EXIT = 2402;
bool AddAgentTrayIcon(HWND owner);
void RemoveAgentTrayIcon();
void ShowAgentTrayMenu(HWND owner);
void ShowAgentTrayNotification(const std::wstring& title, const std::wstring& message);
}

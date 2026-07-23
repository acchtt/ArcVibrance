#pragma once

#include <Windows.h>
#include <Shellapi.h>
#include <vector>

#include "AgentClient.h"
#include "GameProfile.h"

namespace ArcVibrance
{
constexpr int IDC_GAME_LIST = 1101;
constexpr int IDC_ADD_GAME_BUTTON = 1102;
constexpr int IDC_REMOVE_GAME_BUTTON = 1103;
constexpr int IDC_EDIT_GAME_BUTTON = 1104;
constexpr int IDC_TOGGLE_GAME_BUTTON = 1105;

constexpr int IDC_PROFILE_SLIDER = 1201;
constexpr int IDC_PROFILE_VALUE = 1202;
constexpr int IDC_PROFILE_CLOSE = 1203;
constexpr int IDC_PROFILE_VALUE_EDIT = 1204;
constexpr int IDC_AUTOSTART_CHECKBOX = 1301;
constexpr int IDC_RELOAD_AGENT_BUTTON = 1302;
constexpr int IDC_CLOSE_MINIMIZE_RADIO = 1303;
constexpr int IDC_CLOSE_EXIT_RADIO = 1304;
constexpr int IDC_THEME_COMBO = 1305;

constexpr UINT WM_TRAY_ICON = WM_APP + 1;
constexpr UINT WM_FOREGROUND_CHANGED = WM_APP + 2;
constexpr UINT TRAY_ICON_ID = 1;
constexpr UINT ID_TRAY_SHOW = 1401;
constexpr UINT ID_TRAY_EXIT = 1402;

constexpr int SATURATION_MIN = 0;
constexpr int SATURATION_MAX = 300;
constexpr double NORMAL_SATURATION = 1.0;

constexpr UINT_PTR GAME_MONITOR_TIMER_ID = 1;
constexpr UINT GAME_MONITOR_INTERVAL_MS = 2000;

inline constexpr wchar_t PROFILE_EDITOR_CLASS_NAME[] = L"ArcVibranceProfileEditor";
inline constexpr wchar_t AUTOSTART_VALUE_NAME[] = L"ArcVibrance";

extern AgentClient g_agentClient;
extern AgentStatus g_agentStatus;
extern std::vector<GameProfile> g_gameProfiles;
extern int g_editingProfileIndex;

extern NOTIFYICONDATAW g_trayIconData;
extern bool g_trayIconAdded;

extern HFONT g_uiFont;
extern HFONT g_headerFont;

extern HWND g_gameList;
extern HWND g_profileEmptyState;
extern HWND g_addGameButton;
extern HWND g_removeGameButton;
extern HWND g_editGameButton;
extern HWND g_toggleGameButton;
extern HWND g_autoStartCheckbox;
extern HWND g_reloadAgentButton;
extern HWND g_closeMinimizeRadio;
extern HWND g_closeExitRadio;
extern HWND g_themeCombo;
extern HWND g_statusGameLabel;
extern HWND g_statusSaturationLabel;

extern HWND g_profileEditorWindow;
extern HWND g_profileSlider;
extern HWND g_profileValueLabel;
extern HWND g_profileValueEdit;
}

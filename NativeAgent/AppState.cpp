#include "AppState.h"

namespace ArcVibrance
{
AgentClient g_agentClient;
AgentStatus g_agentStatus;
std::vector<GameProfile> g_gameProfiles;
int g_editingProfileIndex = -1;

NOTIFYICONDATAW g_trayIconData = {};
bool g_trayIconAdded = false;

HFONT g_uiFont = nullptr;
HFONT g_headerFont = nullptr;

HWND g_gameList = nullptr;
HWND g_profileEmptyState = nullptr;
HWND g_addGameButton = nullptr;
HWND g_removeGameButton = nullptr;
HWND g_editGameButton = nullptr;
HWND g_toggleGameButton = nullptr;
HWND g_autoStartCheckbox = nullptr;
HWND g_reloadAgentButton = nullptr;
HWND g_closeMinimizeRadio = nullptr;
HWND g_closeExitRadio = nullptr;
HWND g_themeCombo = nullptr;
HWND g_statusGameLabel = nullptr;
HWND g_statusSaturationLabel = nullptr;

HWND g_profileEditorWindow = nullptr;
HWND g_profileSlider = nullptr;
HWND g_profileValueLabel = nullptr;
HWND g_profileValueEdit = nullptr;
}

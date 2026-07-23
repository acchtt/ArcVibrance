#pragma once

#include <windows.h>
#include "Settings.h"

namespace ArcVibrance
{
    extern COLORREF g_windowBackgroundColor;
    extern COLORREF g_panelBackgroundColor;
    extern COLORREF g_textColor;
    extern COLORREF g_mutedTextColor;
    extern COLORREF g_borderColor;
    extern COLORREF g_accentColor;
    extern HFONT g_uiBoldFont;
    bool IsDarkThemeActive();
    void ApplyThemeMode(ThemeMode mode);
    void ApplyWindowTheme(HWND window);
    bool CreateThemeFonts();
    void DestroyThemeFonts();

    void ApplyUiFont(HWND window);
    void ApplyHeaderFont(HWND window);

    bool CreateThemeBrushes();
    void DestroyThemeBrushes();

    HBRUSH GetWindowBackgroundBrush();
    HBRUSH GetPanelBackgroundBrush();
}
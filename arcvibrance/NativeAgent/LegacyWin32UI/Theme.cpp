#include "Theme.h"
#include "AppState.h"
#include <dwmapi.h>
#include <uxtheme.h>
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "UxTheme.lib")

namespace ArcVibrance
{
    COLORREF g_windowBackgroundColor = RGB(1, 5, 18);
    COLORREF g_panelBackgroundColor = RGB(4, 11, 27);
    COLORREF g_textColor = RGB(245, 248, 255);
    COLORREF g_mutedTextColor = RGB(151, 161, 184);
    COLORREF g_borderColor = RGB(24, 40, 66);
    COLORREF g_accentColor = RGB(24, 188, 255);

    namespace
    {
        bool g_darkThemeActive = true;
        HBRUSH g_windowBackgroundBrush = nullptr;
        HBRUSH g_panelBackgroundBrush = nullptr;
    }
    HFONT g_uiBoldFont = nullptr;

    bool IsDarkThemeActive()
    {
        return g_darkThemeActive;
    }

    namespace
    {
        bool IsSystemDarkTheme()
        {
            HKEY key = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
            {
                return true;
            }
            DWORD value = 0;
            DWORD size = sizeof(value);
            DWORD type = 0;
            const LONG result = RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type,
                reinterpret_cast<BYTE*>(&value), &size);
            RegCloseKey(key);
            return result != ERROR_SUCCESS || value == 0;
        }
    }

    void ApplyThemeMode(ThemeMode mode)
    {
        g_darkThemeActive = mode == ThemeMode::Dark ||
            (mode == ThemeMode::System && IsSystemDarkTheme());

        if (g_darkThemeActive)
        {
            g_windowBackgroundColor = RGB(1, 5, 18);
            g_panelBackgroundColor = RGB(4, 11, 27);
            g_textColor = RGB(245, 248, 255);
            g_mutedTextColor = RGB(151, 161, 184);
            g_borderColor = RGB(24, 40, 66);
            g_accentColor = RGB(24, 188, 255);
        }
        else
        {
            g_windowBackgroundColor = RGB(245, 247, 250);
            g_panelBackgroundColor = RGB(255, 255, 255);
            g_textColor = RGB(24, 31, 43);
            g_mutedTextColor = RGB(92, 104, 122);
            g_borderColor = RGB(207, 214, 224);
            g_accentColor = RGB(0, 120, 212);
        }

        DestroyThemeBrushes();
        CreateThemeBrushes();
    }

    void ApplyWindowTheme(HWND window)
    {
        if (window == nullptr) return;
        const BOOL enabled = g_darkThemeActive ? TRUE : FALSE;
        constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_VALUE = 20;
        DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE_VALUE,
            &enabled, sizeof(enabled));

        // Windows 11 styling. Unsupported attributes fail harmlessly on older builds.
        constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE_VALUE = 33;
        constexpr DWORD DWMWA_BORDER_COLOR_VALUE = 34;
        constexpr DWORD DWMWA_CAPTION_COLOR_VALUE = 35;
        const DWORD roundedCornerPreference = 2; // DWMWCP_ROUND
        const COLORREF borderColor = g_darkThemeActive ? RGB(20, 31, 52) : g_borderColor;
        const COLORREF captionColor = g_darkThemeActive ? RGB(1, 5, 18) : g_windowBackgroundColor;
        DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE_VALUE,
            &roundedCornerPreference, sizeof(roundedCornerPreference));
        DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR_VALUE,
            &borderColor, sizeof(borderColor));
        DwmSetWindowAttribute(window, DWMWA_CAPTION_COLOR_VALUE,
            &captionColor, sizeof(captionColor));

        SetWindowTheme(window, g_darkThemeActive ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        RedrawWindow(window, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
    }

    bool CreateThemeFonts()
    {
        if (g_uiFont != nullptr &&
            g_headerFont != nullptr)
        {
            return true;
        }

        g_uiFont = CreateFontW(
            -14,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI");

        g_headerFont = CreateFontW(
            -20,
            0,
            0,
            0,
            FW_SEMIBOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI"
        );
        g_uiBoldFont = CreateFontW(
            -14,
            0,
            0,
            0,
            FW_SEMIBOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI");

        if (g_uiFont == nullptr ||
            g_headerFont == nullptr ||
            g_uiBoldFont == nullptr)
        {
            DestroyThemeFonts();
            return false;
        }

        return true;
    }

    void DestroyThemeFonts()
    {
        if (g_headerFont != nullptr)
        {
            DeleteObject(g_headerFont);
            g_headerFont = nullptr;
        }
        if (g_uiBoldFont != nullptr)
        {
            DeleteObject(g_uiBoldFont);
            g_uiBoldFont = nullptr;
        }
        if (g_uiFont != nullptr)
        {
            DeleteObject(g_uiFont);
            g_uiFont = nullptr;
        }
    }

    void ApplyUiFont(HWND control)
    {
        if (control != nullptr &&
            g_uiFont != nullptr)
        {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(g_uiFont),
                TRUE);
        }
    }

    void ApplyHeaderFont(HWND control)
    {
        if (control != nullptr &&
            g_headerFont != nullptr)
        {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(g_headerFont),
                TRUE);
        }
    }

    bool CreateThemeBrushes()
    {
        if (g_windowBackgroundBrush != nullptr &&
            g_panelBackgroundBrush != nullptr)
        {
            return true;
        }

        g_windowBackgroundBrush =
            CreateSolidBrush(g_windowBackgroundColor);

        g_panelBackgroundBrush =
            CreateSolidBrush(g_panelBackgroundColor);

        if (g_windowBackgroundBrush == nullptr ||
            g_panelBackgroundBrush == nullptr)
        {
            DestroyThemeBrushes();
            return false;
        }

        return true;
    }

    void DestroyThemeBrushes()
    {
        if (g_panelBackgroundBrush != nullptr)
        {
            DeleteObject(g_panelBackgroundBrush);
            g_panelBackgroundBrush = nullptr;
        }

        if (g_windowBackgroundBrush != nullptr)
        {
            DeleteObject(g_windowBackgroundBrush);
            g_windowBackgroundBrush = nullptr;
        }
    }

    HBRUSH GetWindowBackgroundBrush()
    {
        return g_windowBackgroundBrush;
    }

    HBRUSH GetPanelBackgroundBrush()
    {
        return g_panelBackgroundBrush;
    }
}
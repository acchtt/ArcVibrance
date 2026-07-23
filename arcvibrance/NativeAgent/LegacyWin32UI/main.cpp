
#include "MainWindow.h"
#include "Theme.h"
#include "Settings.h"
#include "resource.h"
#include <Windows.h>
#include <CommCtrl.h>
#pragma comment(lib, "Comctl32.lib")

using namespace ArcVibrance;

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR commandLine,
    int showCommand)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    ApplyThemeMode(GetThemeMode());

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&controls);
        

    if (!CreateThemeFonts())
    {
        MessageBoxW(nullptr,
            L"Could not create the application fonts.",
            L"ArcVibrance Error",
            MB_OK | MB_ICONERROR);
        return 1;
    }
    if (!CreateThemeBrushes())
    {
        MessageBoxW(
            nullptr,
            L"Could not create theme brushes.",
            L"ArcVibrance Error",
            MB_OK | MB_ICONERROR);
        DestroyThemeBrushes();
        DestroyThemeFonts();
        return 1;
    }
    if (!RegisterProfileEditorClass(instance))
    {
        DestroyThemeBrushes();
        DestroyThemeFonts();
        MessageBoxW(nullptr,
            L"Could not register the profile editor window.",
            L"ArcVibrance Error",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    constexpr wchar_t windowClassName[] = L"ArcVibranceMainWindow";

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon =
        LoadIconW(
            instance,
            MAKEINTRESOURCEW(
                IDI_ARCVIBRANCE));

    windowClass.hIconSm =
        LoadIconW(
            instance,
            MAKEINTRESOURCEW(
                IDI_ARCVIBRANCE));
    // The window procedure paints the full background with the active theme.
    // A COLOR_WINDOW class brush can briefly flash white during page changes.
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = windowClassName;

    if (RegisterClassExW(&windowClass) == 0)
    {
        DestroyThemeBrushes();
        DestroyThemeFonts();
        MessageBoxW(nullptr,
            L"Could not register the main window class.",
            L"ArcVibrance Error",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND window = CreateWindowExW(
        0,
        windowClassName,
        L"ArcVibrance",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        820,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (window == nullptr)
    {
        DestroyThemeBrushes();
        DestroyThemeFonts();
        MessageBoxW(nullptr,
            L"Could not create the main window.",
            L"ArcVibrance Error",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    const bool startupLaunch =
        commandLine != nullptr &&
        wcsstr(commandLine, L"--startup") != nullptr;

    ShowWindow(window, startupLaunch ? SW_HIDE : showCommand);
    UpdateWindow(window);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    DestroyThemeBrushes();
    DestroyThemeFonts();
    return static_cast<int>(message.wParam);
}

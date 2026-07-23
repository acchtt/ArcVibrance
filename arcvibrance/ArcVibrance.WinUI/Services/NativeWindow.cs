using System.Runtime.InteropServices;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using WinRT.Interop;

namespace ArcVibrance.Services;

public static partial class NativeWindow
{
    private const int SwHide = 0;
    private const int SwShow = 5;
    private const int SwRestore = 9;

    public static nint GetHandle(Window window) => WindowNative.GetWindowHandle(window);

    public static AppWindow GetAppWindow(Window window)
    {
        nint handle = GetHandle(window);
        WindowId id = Microsoft.UI.Win32Interop.GetWindowIdFromWindow(handle);
        return AppWindow.GetFromWindowId(id);
    }

    public static bool Hide(Window window) => ShowWindow(GetHandle(window), SwHide);

    public static void ShowAndActivate(Window window)
    {
        nint handle = GetHandle(window);
        ShowWindow(handle, IsIconic(handle) ? SwRestore : SwShow);
        BringWindowToTop(handle);
        SetForegroundWindow(handle);
        window.Activate();
    }

    [LibraryImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static partial bool ShowWindow(nint hWnd, int nCmdShow);

    [LibraryImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static partial bool SetForegroundWindow(nint hWnd);

    [LibraryImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static partial bool BringWindowToTop(nint hWnd);

    [LibraryImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static partial bool IsIconic(nint hWnd);
}

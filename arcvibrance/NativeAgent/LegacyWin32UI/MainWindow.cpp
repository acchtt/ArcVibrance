#include "MainWindow.h"
#include "AppState.h"
#include "Settings.h"
#include "Theme.h"
#include <utility>
#include "GameMonitor.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <shellapi.h>
#include <winver.h>
#include <windowsx.h>
#include <cwctype>
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Version.lib")
#include <string>
#include <vector>
#include <unordered_map>
#include <gdiplus.h>
#include <uxtheme.h>
#pragma comment(lib, "Gdiplus.lib")


namespace ArcVibrance
{
    constexpr int IDC_STATUS_DOT = 1010;
    bool g_updatingProfileValueControl = false;
    int g_originalProfileSaturation = 100;
    HWND g_profileNameLabel = nullptr;
    HWND g_profilePathLabel = nullptr;
    HWND g_profileCancelButton = nullptr;
    constexpr int IDC_PROFILE_CANCEL = 1205;
    // Keep navigation IDs separate from the settings control IDs declared in
    // AppState.h. Reusing 1301/1302 made checkbox and reload notifications look
    // like sidebar navigation clicks.
    constexpr int IDC_NAV_PROFILES = 1501;
    constexpr int IDC_NAV_SETTINGS = 1502;
    constexpr int IDC_NAV_ABOUT = 1503;
    constexpr int IDC_BRAND_LOGO = 1504;
    constexpr int IDC_BRAND_NAME = 1505;

    enum class MainPage
    {
        Profiles,
        Settings,
        About
    };

    HWND g_brandLogo = nullptr;
    HWND g_profilesNav = nullptr;
    HWND g_settingsNav = nullptr;
    HWND g_aboutNav = nullptr;
    HWND g_profilesPageHost = nullptr;
    HWND g_settingsPageHost = nullptr;
    HWND g_aboutPageHost = nullptr;
    HWND g_pageTitle = nullptr;
    HWND g_pageSubtitle = nullptr;
    HWND g_settingsPageTitle = nullptr;
    HWND g_settingsPageSubtitle = nullptr;
    HWND g_aboutPageTitle = nullptr;
    HWND g_aboutPageSubtitle = nullptr;
    HWND g_statusGroup = nullptr;
    HWND g_statusDot = nullptr;
    HWND g_profilesGroup = nullptr;
    HWND g_dropZoneGroup = nullptr;
    HWND g_quickActionsGroup = nullptr;
    HWND g_instructionLabel = nullptr;
    HWND g_settingsGroup = nullptr;
    HWND g_aboutGroup = nullptr;
    HWND g_closeBehaviorLabel = nullptr;
    HWND g_themeLabel = nullptr;
    MainPage g_currentPage = MainPage::Profiles;
    bool g_pageLayoutInitialized = false;
    HFONT g_brandFont = nullptr;
    HFONT g_pageTitleFont = nullptr;

    LRESULT CALLBACK ProfileEditorProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);
    HWND g_hoveredButton = nullptr;
    std::unordered_map<std::wstring, HICON>
        g_executableIconCache;
    std::unordered_map<std::wstring, std::wstring>
        g_friendlyNameCache;
    LRESULT CALLBACK ButtonSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData);
    LRESULT CALLBACK ProfilePanelSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData);
    LRESULT CALLBACK PageHostSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData);
    LRESULT CALLBACK CardPanelSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData);
    LRESULT CALLBACK DropZoneSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData);
    LRESULT CALLBACK GameListSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData);
    LRESULT CALLBACK SliderSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData);
    LRESULT CALLBACK NumericSpinnerSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData);
    void CommitProfileSaturation(int value, bool redrawProfileList = true);
    void OpenProfileEditor(HWND ownerWindow, int profileIndex);
    COLORREF BlendColor(COLORREF from, COLORREF to, int step, int steps)
    {
        if (steps <= 0) return from;
        const int red = GetRValue(from) +
            ((GetRValue(to) - GetRValue(from)) * step / steps);
        const int green = GetGValue(from) +
            ((GetGValue(to) - GetGValue(from)) * step / steps);
        const int blue = GetBValue(from) +
            ((GetBValue(to) - GetBValue(from)) * step / steps);
        return RGB(red, green, blue);
    }

    bool EnsureGdiPlus()
    {
        static ULONG_PTR token = 0;
        static bool initialized = false;
        if (!initialized)
        {
            Gdiplus::GdiplusStartupInput input;
            initialized = Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok;
        }
        return initialized;
    }

    void AddRoundedPath(Gdiplus::GraphicsPath& path,
        const Gdiplus::RectF& rect, float radius)
    {
        const float diameter = radius * 2.0f;
        if (diameter <= 0.0f)
        {
            path.AddRectangle(rect);
            return;
        }

        path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
        path.AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
        path.AddArc(rect.GetRight() - diameter, rect.GetBottom() - diameter,
            diameter, diameter, 0.0f, 90.0f);
        path.AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0f, 90.0f);
        path.CloseFigure();
    }

    Gdiplus::Color ToGdiPlusColor(COLORREF color, BYTE alpha = 255)
    {
        return Gdiplus::Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color));
    }

    void FillRoundedGradient(HDC dc, const RECT& rect,
        COLORREF leftColor, COLORREF rightColor, int radius)
    {
        if (!EnsureGdiPlus())
        {
            HBRUSH fallback = CreateSolidBrush(leftColor);
            FillRect(dc, &rect, fallback);
            DeleteObject(fallback);
            return;
        }

        Gdiplus::Graphics graphics(dc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::RectF bounds(
            static_cast<Gdiplus::REAL>(rect.left),
            static_cast<Gdiplus::REAL>(rect.top),
            static_cast<Gdiplus::REAL>(rect.right - rect.left),
            static_cast<Gdiplus::REAL>(rect.bottom - rect.top));
        Gdiplus::GraphicsPath path;
        AddRoundedPath(path, bounds, radius / 2.0f);
        Gdiplus::LinearGradientBrush brush(
            Gdiplus::PointF(bounds.X, bounds.Y),
            Gdiplus::PointF(bounds.GetRight(), bounds.Y),
            ToGdiPlusColor(leftColor), ToGdiPlusColor(rightColor));
        graphics.FillPath(&brush, &path);
    }

    void FillRoundedThreeColorGradient(HDC dc, const RECT& rect,
        COLORREF leftColor, COLORREF middleColor, COLORREF rightColor, int radius)
    {
        if (!EnsureGdiPlus())
        {
            HBRUSH fallback = CreateSolidBrush(middleColor);
            FillRect(dc, &rect, fallback);
            DeleteObject(fallback);
            return;
        }

        Gdiplus::Graphics graphics(dc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::RectF bounds(
            static_cast<Gdiplus::REAL>(rect.left),
            static_cast<Gdiplus::REAL>(rect.top),
            static_cast<Gdiplus::REAL>(rect.right - rect.left),
            static_cast<Gdiplus::REAL>(rect.bottom - rect.top));
        Gdiplus::GraphicsPath path;
        AddRoundedPath(path, bounds, radius / 2.0f);
        Gdiplus::LinearGradientBrush brush(
            Gdiplus::PointF(bounds.X, bounds.Y),
            Gdiplus::PointF(bounds.GetRight(), bounds.Y),
            ToGdiPlusColor(leftColor), ToGdiPlusColor(rightColor));
        Gdiplus::Color colors[3] = {
            ToGdiPlusColor(leftColor),
            ToGdiPlusColor(middleColor),
            ToGdiPlusColor(rightColor)
        };
        Gdiplus::REAL positions[3] = { 0.0f, 0.55f, 1.0f };
        brush.SetInterpolationColors(colors, positions, 3);
        graphics.FillPath(&brush, &path);
    }

    void FillRoundedSolid(HDC dc, const RECT& rect, COLORREF color, int radius);

    void FillRoundedGradientBorder(HDC dc, const RECT& rect,
        COLORREF leftColor, COLORREF middleColor, COLORREF rightColor,
        COLORREF fillColor, int radius, int thickness = 2)
    {
        FillRoundedThreeColorGradient(dc, rect, leftColor, middleColor, rightColor, radius);
        RECT inner = rect;
        InflateRect(&inner, -thickness, -thickness);
        if (inner.right > inner.left && inner.bottom > inner.top)
            FillRoundedSolid(dc, inner, fillColor, radius - thickness);
    }

    void DrawNeonBackground(HDC dc, const RECT& client, bool sidebar)
    {
        const COLORREF base = IsDarkThemeActive()
            ? (sidebar ? RGB(0, 3, 16) : RGB(1, 6, 20))
            : g_windowBackgroundColor;
        HBRUSH baseBrush = CreateSolidBrush(base);
        FillRect(dc, &client, baseBrush);
        DeleteObject(baseBrush);

        if (!IsDarkThemeActive() || !EnsureGdiPlus())
            return;

        Gdiplus::Graphics graphics(dc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

        Gdiplus::RectF bounds(
            static_cast<Gdiplus::REAL>(client.left),
            static_cast<Gdiplus::REAL>(client.top),
            static_cast<Gdiplus::REAL>(client.right - client.left),
            static_cast<Gdiplus::REAL>(client.bottom - client.top));

        Gdiplus::LinearGradientBrush background(
            Gdiplus::PointF(bounds.X, bounds.Y),
            Gdiplus::PointF(bounds.GetRight(), bounds.GetBottom()),
            Gdiplus::Color(255, sidebar ? 0 : 1, 3, sidebar ? 16 : 20),
            Gdiplus::Color(255, 3, 12, 29));
        graphics.FillRectangle(&background, bounds);

        if (sidebar)
        {
            Gdiplus::SolidBrush blueGlow(Gdiplus::Color(34, 0, 119, 255));
            Gdiplus::SolidBrush magentaGlow(Gdiplus::Color(25, 224, 0, 218));
            graphics.FillEllipse(&blueGlow, -55.0f, 42.0f, 260.0f, 260.0f);
            graphics.FillEllipse(&magentaGlow, 68.0f, 205.0f, 210.0f, 230.0f);
        }
        else
        {
            const float width = bounds.Width;
            Gdiplus::SolidBrush violetGlow(Gdiplus::Color(20, 92, 22, 220));
            Gdiplus::SolidBrush cyanGlow(Gdiplus::Color(16, 0, 164, 255));
            Gdiplus::SolidBrush pinkGlow(Gdiplus::Color(14, 242, 20, 181));
            graphics.FillEllipse(&violetGlow, width - 360.0f, -160.0f, 470.0f, 420.0f);
            graphics.FillEllipse(&cyanGlow, -150.0f, 430.0f, 440.0f, 360.0f);
            graphics.FillEllipse(&pinkGlow, width - 250.0f, 460.0f, 360.0f, 330.0f);
        }
    }

    void FillRoundedSolid(HDC dc, const RECT& rect, COLORREF color, int radius)
    {
        if (!EnsureGdiPlus())
        {
            HBRUSH fallback = CreateSolidBrush(color);
            FillRect(dc, &rect, fallback);
            DeleteObject(fallback);
            return;
        }

        Gdiplus::Graphics graphics(dc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::RectF bounds(
            static_cast<Gdiplus::REAL>(rect.left),
            static_cast<Gdiplus::REAL>(rect.top),
            static_cast<Gdiplus::REAL>(rect.right - rect.left),
            static_cast<Gdiplus::REAL>(rect.bottom - rect.top));
        Gdiplus::GraphicsPath path;
        AddRoundedPath(path, bounds, radius / 2.0f);
        Gdiplus::SolidBrush brush(ToGdiPlusColor(color));
        graphics.FillPath(&brush, &path);
    }

    void DrawRoundedBorder(HDC dc, const RECT& rect, COLORREF color,
        int radius, float width)
    {
        if (!EnsureGdiPlus())
            return;

        Gdiplus::Graphics graphics(dc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        const float inset = width / 2.0f;
        Gdiplus::RectF bounds(
            static_cast<Gdiplus::REAL>(rect.left) + inset,
            static_cast<Gdiplus::REAL>(rect.top) + inset,
            static_cast<Gdiplus::REAL>(rect.right - rect.left) - width,
            static_cast<Gdiplus::REAL>(rect.bottom - rect.top) - width);
        Gdiplus::GraphicsPath path;
        AddRoundedPath(path, bounds, radius / 2.0f);
        Gdiplus::Pen pen(ToGdiPlusColor(color), width);
        pen.SetAlignment(Gdiplus::PenAlignmentCenter);
        graphics.DrawPath(&pen, &path);
    }

    void ApplyRoundedButtonRegion(HWND button, int radius = 14)
    {
        (void)radius;
        // Keep the HWND rectangular. A Win32 rounded window region has hard,
        // pixel-stepped edges and was the source of the remaining square/jagged
        // corners. The owner-draw renderer paints an anti-aliased rounded shape
        // over the correct parent background instead.
        if (button != nullptr)
            SetWindowRgn(button, nullptr, TRUE);
    }

    void DrawCustomSlider(HWND window, HDC dc)
    {
        RECT client{};
        GetClientRect(window, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        if (width <= 0 || height <= 0)
            return;

        HDC memoryDc = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
        HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
        FillRect(memoryDc, &client, GetPanelBackgroundBrush());

        if (!EnsureGdiPlus())
        {
            BitBlt(dc, 0, 0, width, height, memoryDc, 0, 0, SRCCOPY);
            SelectObject(memoryDc, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(memoryDc);
            return;
        }

        const float margin = 13.0f;
        const float trackHeight = 8.0f;
        const float trackY = (height - trackHeight) / 2.0f;
        const float trackWidth = width - margin * 2.0f;
        if (trackWidth <= 1.0f)
        {
            BitBlt(dc, 0, 0, width, height, memoryDc, 0, 0, SRCCOPY);
            SelectObject(memoryDc, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(memoryDc);
            return;
        }

        int minimum = static_cast<int>(SendMessageW(window, TBM_GETRANGEMIN, 0, 0));
        int maximum = static_cast<int>(SendMessageW(window, TBM_GETRANGEMAX, 0, 0));
        int value = static_cast<int>(SendMessageW(window, TBM_GETPOS, 0, 0));
        if (maximum <= minimum)
            maximum = minimum + 1;
        if (value < minimum) value = minimum;
        if (value > maximum) value = maximum;

        Gdiplus::Graphics graphics(memoryDc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

        Gdiplus::RectF track(margin, trackY, trackWidth, trackHeight);
        Gdiplus::GraphicsPath trackPath;
        AddRoundedPath(trackPath, track, trackHeight / 2.0f);

        Gdiplus::LinearGradientBrush gradient(
            Gdiplus::PointF(track.X, track.Y),
            Gdiplus::PointF(track.GetRight(), track.Y),
            Gdiplus::Color(255, 0, 105, 190),
            Gdiplus::Color(255, 190, 30, 135));
        Gdiplus::Color colors[3] = {
            Gdiplus::Color(255, 0, 105, 190),
            Gdiplus::Color(255, 82, 40, 190),
            Gdiplus::Color(255, 190, 30, 135)
        };
        Gdiplus::REAL positions[3] = { 0.0f, 0.55f, 1.0f };
        gradient.SetInterpolationColors(colors, positions, 3);
        graphics.FillPath(&gradient, &trackPath);

        const float fraction = static_cast<float>(value - minimum) /
            static_cast<float>(maximum - minimum);
        const float thumbX = track.X + track.Width * fraction;
        const float thumbY = track.Y + track.Height / 2.0f;
        const float thumbRadius = 10.0f;

        Gdiplus::SolidBrush shadow(Gdiplus::Color(90, 25, 120, 255));
        graphics.FillEllipse(&shadow,
            thumbX - thumbRadius - 3.0f, thumbY - thumbRadius - 3.0f,
            (thumbRadius + 3.0f) * 2.0f, (thumbRadius + 3.0f) * 2.0f);

        Gdiplus::SolidBrush thumbFill(Gdiplus::Color(255, 45, 91, 220));
        graphics.FillEllipse(&thumbFill,
            thumbX - thumbRadius, thumbY - thumbRadius,
            thumbRadius * 2.0f, thumbRadius * 2.0f);
        Gdiplus::Pen thumbOutline(Gdiplus::Color(255, 245, 248, 255), 2.2f);
        graphics.DrawEllipse(&thumbOutline,
            thumbX - thumbRadius, thumbY - thumbRadius,
            thumbRadius * 2.0f, thumbRadius * 2.0f);

        graphics.Flush();
        BitBlt(dc, 0, 0, width, height, memoryDc, 0, 0, SRCCOPY);
        SelectObject(memoryDc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memoryDc);
    }

    int SliderValueFromPoint(HWND window, int x)
    {
        RECT client{};
        GetClientRect(window, &client);
        const int minimum = static_cast<int>(SendMessageW(window, TBM_GETRANGEMIN, 0, 0));
        const int maximum = static_cast<int>(SendMessageW(window, TBM_GETRANGEMAX, 0, 0));
        const float margin = 13.0f;
        const float usableWidth = static_cast<float>(client.right - client.left) - margin * 2.0f;
        if (maximum <= minimum || usableWidth <= 0.0f)
            return minimum;

        float fraction = (static_cast<float>(x) - margin) / usableWidth;
        if (fraction < 0.0f) fraction = 0.0f;
        if (fraction > 1.0f) fraction = 1.0f;
        return minimum + static_cast<int>(fraction * (maximum - minimum) + 0.5f);
    }

    void SetSliderValueFromPoint(HWND window, int x, int notificationCode)
    {
        const int value = SliderValueFromPoint(window, x);
        const int oldValue = static_cast<int>(SendMessageW(window, TBM_GETPOS, 0, 0));
        if (value != oldValue)
        {
            SendMessageW(window, TBM_SETPOS, FALSE, value);
        }

        HWND parent = GetParent(window);
        if (parent != nullptr && (value != oldValue || notificationCode == TB_THUMBPOSITION))
            SendMessageW(parent, WM_HSCROLL, MAKEWPARAM(notificationCode, value),
                reinterpret_cast<LPARAM>(window));
    }

    LRESULT CALLBACK SliderSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData)
    {
        (void)subclassId;
        (void)referenceData;

        switch (message)
        {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            DrawCustomSlider(window, dc);
            EndPaint(window, &paint);
            return 0;
        }

        case TBM_SETPOS:
        case TBM_SETRANGE:
        case TBM_SETRANGEMIN:
        case TBM_SETRANGEMAX:
        {
            const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
            InvalidateRect(window, nullptr, FALSE);
            return result;
        }

        case WM_LBUTTONDOWN:
        {
            SetFocus(window);
            SetCapture(window);
            SetPropW(window, L"ArcVibranceSliderDragging", reinterpret_cast<HANDLE>(1));
            SetSliderValueFromPoint(window, GET_X_LPARAM(lParam), TB_THUMBTRACK);
            return 0;
        }

        case WM_MOUSEMOVE:
            if (GetCapture() == window &&
                GetPropW(window, L"ArcVibranceSliderDragging") != nullptr)
            {
                SetSliderValueFromPoint(window, GET_X_LPARAM(lParam), TB_THUMBTRACK);
            }
            return 0;

        case WM_LBUTTONUP:
            if (GetCapture() == window)
            {
                SetSliderValueFromPoint(window, GET_X_LPARAM(lParam), TB_THUMBPOSITION);
                ReleaseCapture();
            }
            RemovePropW(window, L"ArcVibranceSliderDragging");
            return 0;

        case WM_CAPTURECHANGED:
            RemovePropW(window, L"ArcVibranceSliderDragging");
            return 0;

        case WM_MOUSEWHEEL:
        {
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int value = static_cast<int>(SendMessageW(window, TBM_GETPOS, 0, 0));
            value += delta > 0 ? 1 : -1;
            const int minimum = static_cast<int>(SendMessageW(window, TBM_GETRANGEMIN, 0, 0));
            const int maximum = static_cast<int>(SendMessageW(window, TBM_GETRANGEMAX, 0, 0));
            if (value < minimum) value = minimum;
            if (value > maximum) value = maximum;
            SendMessageW(window, TBM_SETPOS, FALSE, value);
            SendMessageW(GetParent(window), WM_HSCROLL, MAKEWPARAM(TB_THUMBPOSITION, value),
                reinterpret_cast<LPARAM>(window));
            return 0;
        }

        case WM_KEYDOWN:
        {
            int step = 0;
            if (wParam == VK_LEFT || wParam == VK_DOWN) step = -1;
            else if (wParam == VK_RIGHT || wParam == VK_UP) step = 1;
            else if (wParam == VK_PRIOR) step = 10;
            else if (wParam == VK_NEXT) step = -10;
            else return DefSubclassProc(window, message, wParam, lParam);

            int value = static_cast<int>(SendMessageW(window, TBM_GETPOS, 0, 0)) + step;
            const int minimum = static_cast<int>(SendMessageW(window, TBM_GETRANGEMIN, 0, 0));
            const int maximum = static_cast<int>(SendMessageW(window, TBM_GETRANGEMAX, 0, 0));
            if (value < minimum) value = minimum;
            if (value > maximum) value = maximum;
            SendMessageW(window, TBM_SETPOS, FALSE, value);
            SendMessageW(GetParent(window), WM_HSCROLL, MAKEWPARAM(TB_THUMBPOSITION, value),
                reinterpret_cast<LPARAM>(window));
            return 0;
        }

        case WM_NCDESTROY:
            RemovePropW(window, L"ArcVibranceSliderDragging");
            RemoveWindowSubclass(window, SliderSubclassProcedure, 70);
            break;
        }

        return DefSubclassProc(window, message, wParam, lParam);
    }

    void DrawNavigationIcon(HDC dc, const RECT& rect, int controlId, COLORREF color)
    {
        if (!EnsureGdiPlus())
            return;

        Gdiplus::Graphics graphics(dc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::Pen pen(ToGdiPlusColor(color), 1.9f);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        pen.SetLineJoin(Gdiplus::LineJoinRound);

        const float x = static_cast<float>(rect.left);
        const float y = static_cast<float>(rect.top);
        const float width = static_cast<float>(rect.right - rect.left);
        const float height = static_cast<float>(rect.bottom - rect.top);

        if (controlId == IDC_NAV_SETTINGS)
        {
            const float cx = x + width / 2.0f;
            const float cy = y + height / 2.0f;
            const float outer = 8.0f;
            const float inner = 5.6f;
            const float diagonal = 0.70710678f;
            const float directions[8][2] = {
                { 0.0f, -1.0f }, { diagonal, -diagonal },
                { 1.0f, 0.0f }, { diagonal, diagonal },
                { 0.0f, 1.0f }, { -diagonal, diagonal },
                { -1.0f, 0.0f }, { -diagonal, -diagonal }
            };
            for (const auto& direction : directions)
            {
                graphics.DrawLine(&pen,
                    cx + direction[0] * inner,
                    cy + direction[1] * inner,
                    cx + direction[0] * outer,
                    cy + direction[1] * outer);
            }
            graphics.DrawEllipse(&pen, cx - 5.7f, cy - 5.7f, 11.4f, 11.4f);
            graphics.DrawEllipse(&pen, cx - 2.1f, cy - 2.1f, 4.2f, 4.2f);
            return;
        }

        if (controlId == IDC_NAV_ABOUT)
        {
            const float cx = x + width / 2.0f;
            const float cy = y + height / 2.0f;
            graphics.DrawEllipse(&pen, cx - 8.0f, cy - 8.0f, 16.0f, 16.0f);
            graphics.DrawLine(&pen, cx, cy - 1.0f, cx, cy + 5.0f);
            Gdiplus::SolidBrush dot(ToGdiPlusColor(color));
            graphics.FillEllipse(&dot, cx - 1.2f, cy - 5.3f, 2.4f, 2.4f);
            return;
        }

        // Compact controller outline with a d-pad and two face buttons.
        Gdiplus::GraphicsPath controller;
        controller.StartFigure();
        controller.AddBezier(x + 3.0f, y + 7.0f,
            x + 4.0f, y + 3.0f,
            x + 7.0f, y + 3.0f,
            x + 9.0f, y + 4.4f);
        controller.AddLine(x + 9.0f, y + 4.4f, x + 15.0f, y + 4.4f);
        controller.AddBezier(x + 15.0f, y + 4.4f,
            x + 17.0f, y + 3.0f,
            x + 20.0f, y + 3.0f,
            x + 21.0f, y + 7.0f);
        controller.AddLine(x + 21.0f, y + 7.0f, x + 22.0f, y + 12.2f);
        controller.AddBezier(x + 22.0f, y + 12.2f,
            x + 22.6f, y + 15.5f,
            x + 19.2f, y + 17.0f,
            x + 17.0f, y + 14.0f);
        controller.AddLine(x + 17.0f, y + 14.0f, x + 15.3f, y + 11.8f);
        controller.AddLine(x + 15.3f, y + 11.8f, x + 8.7f, y + 11.8f);
        controller.AddLine(x + 8.7f, y + 11.8f, x + 7.0f, y + 14.0f);
        controller.AddBezier(x + 7.0f, y + 14.0f,
            x + 4.8f, y + 17.0f,
            x + 1.4f, y + 15.5f,
            x + 2.0f, y + 12.2f);
        controller.CloseFigure();
        graphics.DrawPath(&pen, &controller);
        graphics.DrawLine(&pen, x + 6.0f, y + 7.9f, x + 10.0f, y + 7.9f);
        graphics.DrawLine(&pen, x + 8.0f, y + 5.9f, x + 8.0f, y + 9.9f);
        graphics.DrawEllipse(&pen, x + 16.0f, y + 6.2f, 1.5f, 1.5f);
        graphics.DrawEllipse(&pen, x + 18.7f, y + 8.8f, 1.5f, 1.5f);
    }

    void DrawBrandLogo(const DRAWITEMSTRUCT& drawItem)
    {
        const int width = drawItem.rcItem.right - drawItem.rcItem.left;
        const int height = drawItem.rcItem.bottom - drawItem.rcItem.top;
        if (width <= 0 || height <= 0)
            return;

        HDC targetDc = drawItem.hDC;
        HDC dc = CreateCompatibleDC(targetDc);
        HBITMAP bitmap = CreateCompatibleBitmap(targetDc, width, height);
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        RECT client{ 0, 0, width, height };
        DrawNeonBackground(dc, client, true);

        if (EnsureGdiPlus())
        {
            Gdiplus::Graphics graphics(dc);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

            Gdiplus::PointF left(28.0f, static_cast<float>(height - 20));
            Gdiplus::PointF apex(static_cast<float>(width) / 2.0f, 18.0f);
            Gdiplus::PointF right(static_cast<float>(width - 26), static_cast<float>(height - 20));
            Gdiplus::LinearGradientBrush gradient(
                Gdiplus::PointF(18.0f, 18.0f),
                Gdiplus::PointF(static_cast<float>(width - 18), static_cast<float>(height - 14)),
                Gdiplus::Color(255, 0, 211, 255),
                Gdiplus::Color(255, 244, 25, 190));
            Gdiplus::Color colors[4] = {
                Gdiplus::Color(255, 0, 220, 255),
                Gdiplus::Color(255, 25, 94, 255),
                Gdiplus::Color(255, 139, 31, 255),
                Gdiplus::Color(255, 255, 32, 185)
            };
            Gdiplus::REAL positions[4] = { 0.0f, 0.35f, 0.68f, 1.0f };
            gradient.SetInterpolationColors(colors, positions, 4);

            Gdiplus::Pen glow(&gradient, 31.0f);
            glow.SetStartCap(Gdiplus::LineCapRound);
            glow.SetEndCap(Gdiplus::LineCapRound);
            graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
            graphics.DrawLine(&glow, left, apex);
            graphics.DrawLine(&glow, apex, right);

            Gdiplus::Pen logoPen(&gradient, 22.0f);
            logoPen.SetStartCap(Gdiplus::LineCapSquare);
            logoPen.SetEndCap(Gdiplus::LineCapSquare);
            logoPen.SetLineJoin(Gdiplus::LineJoinRound);
            graphics.DrawLine(&logoPen, left, apex);
            graphics.DrawLine(&logoPen, apex, right);
            graphics.DrawLine(&logoPen,
                Gdiplus::PointF(50.0f, static_cast<float>(height - 48)),
                Gdiplus::PointF(static_cast<float>(width - 49), static_cast<float>(height - 48)));
        }

        BitBlt(targetDc, drawItem.rcItem.left, drawItem.rcItem.top,
            width, height, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }

    void DrawBrandName(const DRAWITEMSTRUCT& drawItem)
    {
        const int width = drawItem.rcItem.right - drawItem.rcItem.left;
        const int height = drawItem.rcItem.bottom - drawItem.rcItem.top;
        if (width <= 0 || height <= 0)
            return;

        HDC targetDc = drawItem.hDC;
        HDC dc = CreateCompatibleDC(targetDc);
        HBITMAP bitmap = CreateCompatibleBitmap(targetDc, width, height);
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        RECT client{ 0, 0, width, height };
        HBRUSH background = CreateSolidBrush(IsDarkThemeActive()
            ? RGB(0, 3, 16) : g_windowBackgroundColor);
        FillRect(dc, &client, background);
        DeleteObject(background);

        SetBkMode(dc, TRANSPARENT);
        HGDIOBJ oldFont = SelectObject(dc, g_brandFont != nullptr ? g_brandFont : g_headerFont);
        const wchar_t arcText[] = L"Arc";
        const wchar_t vibranceText[] = L"Vibrance";
        SIZE arcSize{};
        SIZE vibranceSize{};
        GetTextExtentPoint32W(dc, arcText, 3, &arcSize);
        GetTextExtentPoint32W(dc, vibranceText, 8, &vibranceSize);
        const int totalWidth = arcSize.cx + vibranceSize.cx;
        const int startX = (width - totalWidth) / 2;
        RECT arcRect{ startX, 0, startX + arcSize.cx + 2, height };
        RECT vibranceRect{ startX + arcSize.cx, 0, width, height };
        SetTextColor(dc, IsDarkThemeActive() ? RGB(25, 184, 255) : g_accentColor);
        DrawTextW(dc, arcText, -1, &arcRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SetTextColor(dc, g_textColor);
        DrawTextW(dc, vibranceText, -1, &vibranceRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, oldFont);

        BitBlt(targetDc, drawItem.rcItem.left, drawItem.rcItem.top,
            width, height, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }

    constexpr wchar_t NUMERIC_SPINNER_VALUE_PROPERTY[] =
        L"ArcVibranceSpinnerValue";

    int ReadNumericSpinnerValue(HWND window)
    {
        // Keep the custom spinner value outside the stock STATIC text storage.
        // SetWindowText causes the STATIC class to invalidate itself before our
        // custom painter runs, producing a bright flash on every value change.
        // The +1 encoding keeps a valid zero distinct from a missing property.
        const INT_PTR stored = reinterpret_cast<INT_PTR>(
            GetPropW(window, NUMERIC_SPINNER_VALUE_PROPERTY));
        if (stored > 0)
        {
            const int value = static_cast<int>(stored - 1);
            if (value < SATURATION_MIN) return SATURATION_MIN;
            if (value > SATURATION_MAX) return SATURATION_MAX;
            return value;
        }

        // The property is initialized immediately after subclassing. Returning
        // the default here also avoids recursively sending WM_GETTEXT while the
        // custom WM_GETTEXT handler itself is reading the value.
        return 100;
    }

    void SetNumericSpinnerValue(HWND window, int value, bool commit)
    {
        if (value < SATURATION_MIN) value = SATURATION_MIN;
        if (value > SATURATION_MAX) value = SATURATION_MAX;

        const bool hadStoredValue =
            GetPropW(window, NUMERIC_SPINNER_VALUE_PROPERTY) != nullptr;
        const int previousValue = hadStoredValue
            ? ReadNumericSpinnerValue(window)
            : 100;
        if (!hadStoredValue || previousValue != value)
        {
            SetPropW(window, NUMERIC_SPINNER_VALUE_PROPERTY,
                reinterpret_cast<HANDLE>(static_cast<INT_PTR>(value + 1)));
            InvalidateRect(window, nullptr, FALSE);
        }

        if (commit)
            CommitProfileSaturation(value, true);
    }

    void AdjustNumericSpinner(HWND window, int amount)
    {
        SetNumericSpinnerValue(window, ReadNumericSpinnerValue(window) + amount, true);
    }

    void DrawNumericSpinner(HWND window, HDC dc)
    {
        RECT client{};
        GetClientRect(window, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        if (width <= 0 || height <= 0)
            return;

        HDC memoryDc = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
        HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
        FillRect(memoryDc, &client, GetPanelBackgroundBrush());

        RECT frame = client;
        InflateRect(&frame, -1, -1);
        const COLORREF fillColor = IsDarkThemeActive()
            ? RGB(14, 20, 36) : RGB(247, 249, 253);
        const COLORREF arrowFill = IsDarkThemeActive()
            ? RGB(19, 27, 47) : RGB(235, 240, 248);
        const bool focused = GetFocus() == window;
        const COLORREF border = focused ? RGB(55, 151, 255) :
            (IsDarkThemeActive() ? RGB(55, 66, 92) : RGB(183, 193, 210));
        FillRoundedSolid(memoryDc, frame, fillColor, 14);

        const int arrowLeft = width - 42;
        if (EnsureGdiPlus())
        {
            Gdiplus::Graphics graphics(memoryDc);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

            Gdiplus::RectF outer(1.0f, 1.0f,
                static_cast<float>(width - 2), static_cast<float>(height - 2));
            Gdiplus::GraphicsPath clipPath;
            AddRoundedPath(clipPath, outer, 7.0f);
            graphics.SetClip(&clipPath);
            Gdiplus::SolidBrush arrowBrush(ToGdiPlusColor(arrowFill));
            graphics.FillRectangle(&arrowBrush,
                static_cast<Gdiplus::REAL>(arrowLeft), 1.0f,
                static_cast<Gdiplus::REAL>(width - arrowLeft - 1),
                static_cast<Gdiplus::REAL>(height - 2));

            const int pressedPart = static_cast<int>(
                reinterpret_cast<INT_PTR>(GetPropW(window, L"ArcVibranceSpinnerPressed")));
            if (pressedPart != 0)
            {
                Gdiplus::SolidBrush pressedBrush(Gdiplus::Color(105, 53, 105, 185));
                const float top = pressedPart > 0 ? 1.0f : height / 2.0f;
                const float partHeight = height / 2.0f - 1.0f;
                graphics.FillRectangle(&pressedBrush,
                    static_cast<Gdiplus::REAL>(arrowLeft), top,
                    static_cast<Gdiplus::REAL>(width - arrowLeft - 1), partHeight);
            }
            graphics.ResetClip();

            Gdiplus::Pen separator(ToGdiPlusColor(border), 1.0f);
            graphics.DrawLine(&separator,
                static_cast<Gdiplus::REAL>(arrowLeft), 1.5f,
                static_cast<Gdiplus::REAL>(arrowLeft), static_cast<Gdiplus::REAL>(height - 1.5f));
            graphics.DrawLine(&separator,
                static_cast<Gdiplus::REAL>(arrowLeft), static_cast<Gdiplus::REAL>(height / 2),
                static_cast<Gdiplus::REAL>(width - 1), static_cast<Gdiplus::REAL>(height / 2));

            Gdiplus::Pen arrowPen(ToGdiPlusColor(g_textColor), 1.9f);
            arrowPen.SetStartCap(Gdiplus::LineCapRound);
            arrowPen.SetEndCap(Gdiplus::LineCapRound);
            const float centerX = arrowLeft + (width - arrowLeft) / 2.0f;
            const float upperY = height * 0.25f;
            const float lowerY = height * 0.75f;
            graphics.DrawLine(&arrowPen, centerX - 4.5f, upperY + 2.0f,
                centerX, upperY - 2.0f);
            graphics.DrawLine(&arrowPen, centerX, upperY - 2.0f,
                centerX + 4.5f, upperY + 2.0f);
            graphics.DrawLine(&arrowPen, centerX - 4.5f, lowerY - 2.0f,
                centerX, lowerY + 2.0f);
            graphics.DrawLine(&arrowPen, centerX, lowerY + 2.0f,
                centerX + 4.5f, lowerY - 2.0f);
        }
        DrawRoundedBorder(memoryDc, frame, border, 14, focused ? 1.6f : 1.1f);

        const std::wstring valueText =
            std::to_wstring(ReadNumericSpinnerValue(window));
        SetBkMode(memoryDc, TRANSPARENT);
        SetTextColor(memoryDc, g_textColor);
        HGDIOBJ oldFont = SelectObject(memoryDc, g_uiBoldFont ? g_uiBoldFont : g_uiFont);

        RECT valueRect{ 13, 0, arrowLeft - 34, height };
        DrawTextW(memoryDc, valueText.c_str(), -1, &valueRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        RECT percentRect{ arrowLeft - 39, 0, arrowLeft - 5, height };
        DrawTextW(memoryDc, L"%", -1, &percentRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(memoryDc, oldFont);

        BitBlt(dc, 0, 0, width, height, memoryDc, 0, 0, SRCCOPY);
        SelectObject(memoryDc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memoryDc);
    }

    LRESULT CALLBACK NumericSpinnerSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData)
    {
        switch (message)
        {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            DrawNumericSpinner(window, dc);
            EndPaint(window, &paint);
            return 0;
        }

        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;

        case WM_SETFOCUS:
            RemovePropW(window, L"ArcVibranceSpinnerTyping");
            InvalidateRect(window, nullptr, FALSE);
            return 0;

        case WM_KILLFOCUS:
            RemovePropW(window, L"ArcVibranceSpinnerTyping");
            InvalidateRect(window, nullptr, FALSE);
            return 0;

        case WM_SETTEXT:
        {
            const wchar_t* text = reinterpret_cast<const wchar_t*>(lParam);
            wchar_t* end = nullptr;
            long parsed = text != nullptr ? wcstol(text, &end, 10) : 100;
            if (text == nullptr || end == text || *end != L'\0')
                parsed = 100;
            if (parsed < SATURATION_MIN) parsed = SATURATION_MIN;
            if (parsed > SATURATION_MAX) parsed = SATURATION_MAX;
            SetNumericSpinnerValue(window, static_cast<int>(parsed), false);
            return TRUE;
        }

        case WM_GETTEXTLENGTH:
            return static_cast<LRESULT>(
                std::to_wstring(ReadNumericSpinnerValue(window)).size());

        case WM_GETTEXT:
        {
            if (wParam == 0 || lParam == 0)
                return 0;
            const std::wstring text =
                std::to_wstring(ReadNumericSpinnerValue(window));
            lstrcpynW(reinterpret_cast<wchar_t*>(lParam), text.c_str(),
                static_cast<int>(wParam));
            const std::size_t available = static_cast<std::size_t>(wParam - 1);
            return static_cast<LRESULT>(
                text.size() < available ? text.size() : available);
        }

        case WM_LBUTTONDOWN:
        {
            SetFocus(window);
            RECT client{};
            GetClientRect(window, &client);
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            if (x >= client.right - 42)
            {
                const int part = y < (client.bottom / 2) ? 1 : -1;
                SetPropW(window, L"ArcVibranceSpinnerPressed",
                    reinterpret_cast<HANDLE>(static_cast<INT_PTR>(part)));
                AdjustNumericSpinner(window, part);
                SetCapture(window);
                SetTimer(window, 1, 135, nullptr);
                InvalidateRect(window, nullptr, FALSE);
            }
            else
            {
                RemovePropW(window, L"ArcVibranceSpinnerTyping");
            }
            return 0;
        }

        case WM_LBUTTONUP:
            KillTimer(window, 1);
            if (GetCapture() == window)
            {
                // WM_CAPTURECHANGED clears the pressed state and schedules the
                // single required repaint.
                ReleaseCapture();
            }
            else
            {
                RemovePropW(window, L"ArcVibranceSpinnerPressed");
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;

        case WM_CAPTURECHANGED:
            KillTimer(window, 1);
            RemovePropW(window, L"ArcVibranceSpinnerPressed");
            InvalidateRect(window, nullptr, FALSE);
            return 0;

        case WM_TIMER:
            if (wParam == 1)
            {
                const int part = static_cast<int>(
                    reinterpret_cast<INT_PTR>(GetPropW(window, L"ArcVibranceSpinnerPressed")));
                if (part != 0)
                    AdjustNumericSpinner(window, part);
                return 0;
            }
            break;

        case WM_MOUSEWHEEL:
            AdjustNumericSpinner(window, GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1);
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_UP || wParam == VK_RIGHT)
            {
                AdjustNumericSpinner(window, 1);
                return 0;
            }
            if (wParam == VK_DOWN || wParam == VK_LEFT)
            {
                AdjustNumericSpinner(window, -1);
                return 0;
            }
            if (wParam == VK_PRIOR)
            {
                AdjustNumericSpinner(window, 10);
                return 0;
            }
            if (wParam == VK_NEXT)
            {
                AdjustNumericSpinner(window, -10);
                return 0;
            }
            if (wParam == VK_HOME)
            {
                SetNumericSpinnerValue(window, SATURATION_MIN, true);
                return 0;
            }
            if (wParam == VK_END)
            {
                SetNumericSpinnerValue(window, SATURATION_MAX, true);
                return 0;
            }
            break;

        case WM_CHAR:
        {
            if (wParam == VK_RETURN || wParam == VK_TAB)
                return 0;
            if (wParam == VK_BACK)
            {
                wchar_t currentBuffer[8] = {};
                GetWindowTextW(window, currentBuffer, 8);
                std::wstring current(currentBuffer);
                if (current.size() > 1)
                {
                    current.pop_back();
                    SetNumericSpinnerValue(window,
                        static_cast<int>(wcstol(current.c_str(), nullptr, 10)), true);
                }
                else
                {
                    SetNumericSpinnerValue(window, 0, true);
                }
                SetPropW(window, L"ArcVibranceSpinnerTyping",
                    reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1)));
                return 0;
            }
            if (wParam >= L'0' && wParam <= L'9')
            {
                const bool typing = GetPropW(window, L"ArcVibranceSpinnerTyping") != nullptr;
                std::wstring candidate;
                if (typing)
                {
                    wchar_t currentBuffer[8] = {};
                    GetWindowTextW(window, currentBuffer, 8);
                    candidate = currentBuffer;
                }
                candidate.push_back(static_cast<wchar_t>(wParam));
                int value = static_cast<int>(wcstol(candidate.c_str(), nullptr, 10));
                if (value > SATURATION_MAX)
                    value = SATURATION_MAX;
                SetNumericSpinnerValue(window, value, true);
                SetPropW(window, L"ArcVibranceSpinnerTyping",
                    reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1)));
                return 0;
            }
            return 0;
        }

        case WM_NCDESTROY:
            KillTimer(window, 1);
            RemovePropW(window, L"ArcVibranceSpinnerPressed");
            RemovePropW(window, L"ArcVibranceSpinnerTyping");
            RemovePropW(window, NUMERIC_SPINNER_VALUE_PROPERTY);
            RemoveWindowSubclass(window, NumericSpinnerSubclassProcedure, subclassId);
            break;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    void DrawChoiceControl(const DRAWITEMSTRUCT& drawItem, bool radioButton)
    {
        HDC dc = drawItem.hDC;
        RECT rect = drawItem.rcItem;
        FillRect(dc, &rect, GetPanelBackgroundBrush());

        const bool checked = SendMessageW(drawItem.hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED;
        const bool hovered = drawItem.hwndItem == g_hoveredButton;
        const bool disabled = (drawItem.itemState & ODS_DISABLED) != 0;
        const bool pressed = (drawItem.itemState & ODS_SELECTED) != 0;

        const int indicatorSize = 18;
        RECT indicator{};
        indicator.left = rect.left + 2;
        indicator.top = rect.top + ((rect.bottom - rect.top) - indicatorSize) / 2;
        indicator.right = indicator.left + indicatorSize;
        indicator.bottom = indicator.top + indicatorSize;

        const COLORREF controlFill = pressed ? RGB(18, 34, 58) : RGB(10, 25, 45);
        const COLORREF outline = hovered ? RGB(64, 178, 255) : RGB(54, 72, 104);

        if (radioButton)
        {
            if (EnsureGdiPlus())
            {
                Gdiplus::Graphics graphics(dc);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::SolidBrush fill(ToGdiPlusColor(controlFill));
                Gdiplus::Pen pen(ToGdiPlusColor(outline), 1.5f);
                graphics.FillEllipse(&fill, static_cast<Gdiplus::REAL>(indicator.left),
                    static_cast<Gdiplus::REAL>(indicator.top),
                    static_cast<Gdiplus::REAL>(indicatorSize),
                    static_cast<Gdiplus::REAL>(indicatorSize));
                graphics.DrawEllipse(&pen, static_cast<Gdiplus::REAL>(indicator.left) + 0.75f,
                    static_cast<Gdiplus::REAL>(indicator.top) + 0.75f,
                    static_cast<Gdiplus::REAL>(indicatorSize) - 1.5f,
                    static_cast<Gdiplus::REAL>(indicatorSize) - 1.5f);
                if (checked)
                {
                    Gdiplus::SolidBrush dot(ToGdiPlusColor(RGB(49, 174, 255)));
                    graphics.FillEllipse(&dot,
                        static_cast<Gdiplus::REAL>(indicator.left + 5),
                        static_cast<Gdiplus::REAL>(indicator.top + 5), 8.0f, 8.0f);
                }
            }
        }
        else
        {
            FillRoundedSolid(dc, indicator, checked ? RGB(42, 100, 220) : controlFill, 6);
            DrawRoundedBorder(dc, indicator, checked ? RGB(86, 191, 255) : outline, 6, 1.4f);
            if (checked && EnsureGdiPlus())
            {
                Gdiplus::Graphics graphics(dc);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::Pen checkPen(Gdiplus::Color(255, 255, 255, 255), 2.0f);
                checkPen.SetStartCap(Gdiplus::LineCapRound);
                checkPen.SetEndCap(Gdiplus::LineCapRound);
                graphics.DrawLine(&checkPen,
                    static_cast<Gdiplus::REAL>(indicator.left + 4),
                    static_cast<Gdiplus::REAL>(indicator.top + 9),
                    static_cast<Gdiplus::REAL>(indicator.left + 8),
                    static_cast<Gdiplus::REAL>(indicator.top + 13));
                graphics.DrawLine(&checkPen,
                    static_cast<Gdiplus::REAL>(indicator.left + 8),
                    static_cast<Gdiplus::REAL>(indicator.top + 13),
                    static_cast<Gdiplus::REAL>(indicator.left + 14),
                    static_cast<Gdiplus::REAL>(indicator.top + 5));
            }
        }

        wchar_t text[160] = {};
        GetWindowTextW(drawItem.hwndItem, text, 160);
        RECT textRect = rect;
        textRect.left = indicator.right + 10;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, disabled ? g_mutedTextColor : g_textColor);
        HGDIOBJ oldFont = SelectObject(dc, g_uiFont);
        DrawTextW(dc, text, -1, &textRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, oldFont);
    }

    void DrawThemeComboItem(const DRAWITEMSTRUCT& drawItem)
    {
        HDC dc = drawItem.hDC;
        RECT rect = drawItem.rcItem;
        FillRect(dc, &rect, GetPanelBackgroundBrush());

        if ((drawItem.itemState & ODS_SELECTED) != 0 && drawItem.itemID != static_cast<UINT>(-1))
        {
            RECT selectedRect = rect;
            InflateRect(&selectedRect, -2, -1);
            FillRoundedSolid(dc, selectedRect, RGB(20, 43, 72), 6);
        }

        int itemIndex = static_cast<int>(drawItem.itemID);
        if (itemIndex < 0)
            itemIndex = static_cast<int>(SendMessageW(drawItem.hwndItem, CB_GETCURSEL, 0, 0));

        wchar_t text[80] = {};
        if (itemIndex >= 0)
            SendMessageW(drawItem.hwndItem, CB_GETLBTEXT,
                static_cast<WPARAM>(itemIndex), reinterpret_cast<LPARAM>(text));

        RECT textRect = rect;
        textRect.left += 10;
        textRect.right -= 6;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, g_textColor);
        HGDIOBJ oldFont = SelectObject(dc, g_uiFont);
        DrawTextW(dc, text, -1, &textRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, oldFont);
    }

    void DrawThemedButton(const DRAWITEMSTRUCT& drawItem)
    {
        const int width = drawItem.rcItem.right - drawItem.rcItem.left;
        const int height = drawItem.rcItem.bottom - drawItem.rcItem.top;
        if (width <= 0 || height <= 0)
            return;

        // Compose the full button off-screen, then copy it in one pass. This
        // removes partial gradient/icon/text frames during hover and clicks.
        HDC targetDc = drawItem.hDC;
        HDC dc = CreateCompatibleDC(targetDc);
        HBITMAP bitmap = CreateCompatibleBitmap(targetDc, width, height);
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

        RECT itemRect{ 0, 0, width, height };
        RECT rect = itemRect;
        InflateRect(&rect, -1, -1);

        const int controlId = GetDlgCtrlID(drawItem.hwndItem);
        const bool navigationButton = controlId == IDC_NAV_PROFILES ||
            controlId == IDC_NAV_SETTINGS || controlId == IDC_NAV_ABOUT;
        const bool navActive =
            (controlId == IDC_NAV_PROFILES && g_currentPage == MainPage::Profiles) ||
            (controlId == IDC_NAV_SETTINGS && g_currentPage == MainPage::Settings) ||
            (controlId == IDC_NAV_ABOUT && g_currentPage == MainPage::About);
        const bool rawPressed = (drawItem.itemState & ODS_SELECTED) != 0;
        const bool customNavPressed = navigationButton &&
            GetPropW(drawItem.hwndItem, L"ArcVibranceButtonPressed") != nullptr;
        const bool pressed = navigationButton
            ? (customNavPressed && !navActive)
            : rawPressed;
        const bool focused = !navigationButton &&
            (drawItem.itemState & ODS_FOCUS) != 0;
        const bool disabled = (drawItem.itemState & ODS_DISABLED) != 0;
        const bool hovered = drawItem.hwndItem == g_hoveredButton;
        const bool primary = controlId == IDC_PROFILE_CLOSE || navActive;
        const bool outlinedAccent = controlId == IDC_ADD_GAME_BUTTON;
        const bool destructive = controlId == IDC_REMOVE_GAME_BUTTON;

        const HWND parent = GetParent(drawItem.hwndItem);
        const bool panelButton = parent == g_profileEditorWindow ||
            parent == g_profilesGroup || parent == g_settingsGroup ||
            parent == g_quickActionsGroup || parent == g_aboutGroup;
        FillRect(dc, &itemRect,
            panelButton ? GetPanelBackgroundBrush() : GetWindowBackgroundBrush());

        COLORREF borderColor = focused ? g_accentColor : g_borderColor;
        COLORREF textColor = disabled ? g_mutedTextColor : g_textColor;

        if (controlId == IDC_TOGGLE_GAME_BUTTON)
        {
            const bool checked = SendMessageW(drawItem.hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED;
            RECT switchRect = rect;
            FillRoundedSolid(dc, switchRect,
                checked ? RGB(13, 112, 242) : RGB(37, 49, 70), 20);
            DrawRoundedBorder(dc, switchRect,
                checked ? RGB(35, 165, 255) : RGB(72, 86, 110), 20, 1.0f);
            RECT knob = switchRect;
            const int inset = 4;
            const int size = (switchRect.bottom - switchRect.top) - inset * 2;
            knob.top += inset;
            knob.bottom = knob.top + size;
            if (checked)
            {
                knob.right -= inset;
                knob.left = knob.right - size;
            }
            else
            {
                knob.left += inset;
                knob.right = knob.left + size;
            }
            FillRoundedSolid(dc, knob, RGB(248, 251, 255), size);
            BitBlt(targetDc, drawItem.rcItem.left, drawItem.rcItem.top,
                width, height, dc, 0, 0, SRCCOPY);
            SelectObject(dc, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(dc);
            return;
        }

        if (primary)
        {
            COLORREF left = pressed ? RGB(0, 104, 190) : RGB(0, 120, 205);
            COLORREF middle = pressed ? RGB(88, 48, 194) : RGB(78, 48, 195);
            COLORREF right = pressed ? RGB(190, 42, 150) : RGB(198, 42, 158);
            if (hovered && !pressed)
            {
                left = RGB(18, 145, 220);
                middle = RGB(95, 65, 215);
                right = RGB(218, 58, 178);
            }
            if (disabled)
            {
                left = RGB(55, 72, 96);
                middle = RGB(68, 65, 98);
                right = RGB(82, 62, 86);
            }
            FillRoundedThreeColorGradient(dc, rect, left, middle, right, 14);
            borderColor = hovered ? RGB(150, 205, 255) : RGB(70, 150, 255);
            textColor = disabled ? RGB(155, 160, 175) : RGB(255, 255, 255);
        }
        else if (outlinedAccent)
        {
            COLORREF fillColor = panelButton ? g_panelBackgroundColor : g_windowBackgroundColor;
            if (hovered) fillColor = IsDarkThemeActive() ? RGB(8, 24, 45) : RGB(235, 245, 255);
            FillRoundedGradientBorder(dc, rect,
                RGB(0, 194, 255), RGB(70, 64, 255), RGB(231, 25, 190),
                fillColor, 14, hovered ? 2 : 1);
            textColor = hovered ? RGB(215, 244, 255) : g_textColor;
        }
        else
        {
            COLORREF fillColor = navigationButton
                ? g_windowBackgroundColor
                : g_panelBackgroundColor;
            if (pressed) fillColor = RGB(13, 27, 50);
            else if (hovered) fillColor = RGB(7, 20, 42);
            if (destructive && hovered) fillColor = RGB(64, 30, 42);
            if (navigationButton)
                textColor = hovered ? RGB(239, 245, 255) : RGB(204, 216, 235);

            FillRoundedSolid(dc, rect, fillColor, 14);

            if (destructive)
            {
                borderColor = hovered ? RGB(240, 92, 116) : RGB(116, 58, 74);
                textColor = hovered ? RGB(255, 190, 200) : g_textColor;
            }
        }

        // Inactive sidebar items intentionally have no outline.
        if ((!navigationButton || navActive) && !outlinedAccent)
        {
            const float borderWidth = navigationButton ? 1.0f : (focused ? 2.0f : 1.0f);
            DrawRoundedBorder(dc, rect, borderColor, 14, borderWidth);
        }

        wchar_t text[128] = {};
        GetWindowTextW(drawItem.hwndItem, text, 128);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, textColor);
        HFONT oldFont = reinterpret_cast<HFONT>(
            SelectObject(dc, (primary || navigationButton) ? g_uiBoldFont : g_uiFont));

        if (navigationButton)
        {
            RECT iconRect{};
            iconRect.left = rect.left + 20;
            iconRect.right = iconRect.left + 24;
            iconRect.top = rect.top + ((rect.bottom - rect.top) - 20) / 2;
            iconRect.bottom = iconRect.top + 20;
            DrawNavigationIcon(dc, iconRect, controlId, textColor);

            RECT textRect = rect;
            textRect.left = iconRect.right + 12;
            textRect.right -= 12;
            DrawTextW(dc, text, -1, &textRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        else
        {
            DrawTextW(dc, text, -1, &rect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        SelectObject(dc, oldFont);

        BitBlt(targetDc, drawItem.rcItem.left, drawItem.rcItem.top,
            width, height, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }
    LRESULT CALLBACK ButtonSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData)
    {
        switch (message)
        {
        case WM_ERASEBKGND:
            // Owner-drawn buttons paint their complete client area.
            return 1;

        case WM_LBUTTONDOWN:
        {
            const int controlId = GetDlgCtrlID(window);
            const bool navigationButton = controlId == IDC_NAV_PROFILES ||
                controlId == IDC_NAV_SETTINGS || controlId == IDC_NAV_ABOUT;
            if (navigationButton)
            {
                const bool activeNavigation =
                    (controlId == IDC_NAV_PROFILES && g_currentPage == MainPage::Profiles) ||
                    (controlId == IDC_NAV_SETTINGS && g_currentPage == MainPage::Settings) ||
                    (controlId == IDC_NAV_ABOUT && g_currentPage == MainPage::About);
                if (!activeNavigation)
                {
                    SetPropW(window, L"ArcVibranceButtonPressed",
                        reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1)));
                    SetCapture(window);
                    InvalidateRect(window, nullptr, FALSE);
                }
                return 0;
            }
            break;
        }

        case WM_LBUTTONUP:
        {
            const int controlId = GetDlgCtrlID(window);
            const bool navigationButton = controlId == IDC_NAV_PROFILES ||
                controlId == IDC_NAV_SETTINGS || controlId == IDC_NAV_ABOUT;
            if (navigationButton)
            {
                const bool wasPressed =
                    GetPropW(window, L"ArcVibranceButtonPressed") != nullptr;
                RemovePropW(window, L"ArcVibranceButtonPressed");
                if (GetCapture() == window)
                    ReleaseCapture();
                InvalidateRect(window, nullptr, FALSE);

                RECT client{};
                GetClientRect(window, &client);
                POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (wasPressed && PtInRect(&client, point))
                {
                    SendMessageW(GetParent(window), WM_COMMAND,
                        MAKEWPARAM(controlId, BN_CLICKED),
                        reinterpret_cast<LPARAM>(window));
                }
                return 0;
            }
            break;
        }

        case WM_CAPTURECHANGED:
        {
            const int controlId = GetDlgCtrlID(window);
            if (controlId == IDC_NAV_PROFILES || controlId == IDC_NAV_SETTINGS ||
                controlId == IDC_NAV_ABOUT)
            {
                if (GetPropW(window, L"ArcVibranceButtonPressed") != nullptr)
                {
                    RemovePropW(window, L"ArcVibranceButtonPressed");
                    InvalidateRect(window, nullptr, FALSE);
                }
                return 0;
            }
            break;
        }

        case WM_LBUTTONDBLCLK:
        {
            const int controlId = GetDlgCtrlID(window);
            if (controlId == IDC_NAV_PROFILES || controlId == IDC_NAV_SETTINGS ||
                controlId == IDC_NAV_ABOUT)
                return 0;
            break;
        }

        case WM_MOUSEMOVE:
        {
            if (g_hoveredButton != window)
            {
                if (g_hoveredButton != nullptr)
                {
                    InvalidateRect(
                        g_hoveredButton,
                        nullptr,
                        FALSE);
                }

                g_hoveredButton = window;

                TRACKMOUSEEVENT trackingData = {};
                trackingData.cbSize =
                    sizeof(trackingData);
                trackingData.dwFlags =
                    TME_LEAVE;
                trackingData.hwndTrack =
                    window;

                TrackMouseEvent(
                    &trackingData);

                InvalidateRect(
                    window,
                    nullptr,
                    FALSE);
            }

            const int controlId = GetDlgCtrlID(window);
            if (controlId == IDC_NAV_PROFILES || controlId == IDC_NAV_SETTINGS ||
                controlId == IDC_NAV_ABOUT)
                return 0;
            break;
        }

        case WM_MOUSELEAVE:
        {
            if (g_hoveredButton == window)
            {
                g_hoveredButton = nullptr;

                InvalidateRect(
                    window,
                    nullptr,
                    FALSE);
            }

            const int controlId = GetDlgCtrlID(window);
            if (controlId == IDC_NAV_PROFILES || controlId == IDC_NAV_SETTINGS ||
                controlId == IDC_NAV_ABOUT)
                return 0;
            break;
        }

        case WM_NCDESTROY:
        {
            if (g_hoveredButton == window)
            {
                g_hoveredButton = nullptr;
            }
            RemovePropW(window, L"ArcVibranceButtonPressed");

            RemoveWindowSubclass(
                window,
                ButtonSubclassProcedure,
                subclassId);

            break;
        }

        default:
        {
            break;
        }
        }

        return DefSubclassProc(
            window,
            message,
            wParam,
            lParam);
    }

    LRESULT CALLBACK ProfilePanelSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData)
    {
        switch (message)
        {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT clientRect{};
            GetClientRect(window, &clientRect);
            FillRect(dc, &clientRect, GetWindowBackgroundBrush());
            RECT panelRect = clientRect;
            InflateRect(&panelRect, -1, -1);
            FillRoundedSolid(dc, panelRect, g_panelBackgroundColor, 16);
            DrawRoundedBorder(dc, panelRect, g_borderColor, 16, 1.0f);

            HPEN separatorPen = CreatePen(PS_SOLID, 1, g_borderColor);
            HGDIOBJ oldPen = SelectObject(dc, separatorPen);
            MoveToEx(dc, 22, 64, nullptr);
            LineTo(dc, clientRect.right - 22, 64);
            MoveToEx(dc, 22, 184, nullptr);
            LineTo(dc, clientRect.right - 22, 184);
            MoveToEx(dc, 22, 404, nullptr);
            LineTo(dc, clientRect.right - 22, 404);
            MoveToEx(dc, 22, 520, nullptr);
            LineTo(dc, clientRect.right - 22, 520);

            SelectObject(dc, oldPen);
            DeleteObject(separatorPen);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, g_textColor);
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetPanelBackgroundBrush());
        }
        case WM_CTLCOLOREDIT:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, g_textColor);
            SetBkColor(dc, g_panelBackgroundColor);
            return reinterpret_cast<LRESULT>(GetPanelBackgroundBrush());
        }
        case WM_CTLCOLORBTN:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, g_textColor);
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetPanelBackgroundBrush());
        }
        case WM_DRAWITEM:
        {
            const auto drawItem = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (drawItem != nullptr)
            {
                DrawThemedButton(*drawItem);
                return TRUE;
            }
            break;
        }
        case WM_HSCROLL:
        case WM_COMMAND:
        case WM_NOTIFY:
            return SendMessageW(GetParent(window), message, wParam, lParam);

        case WM_NCDESTROY:
            RemoveWindowSubclass(window, ProfilePanelSubclassProcedure, subclassId);
            break;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    LRESULT ForwardChildMessageToRoot(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        HWND root = GetAncestor(window, GA_ROOT);
        if (root != nullptr && root != window)
            return SendMessageW(root, message, wParam, lParam);
        return 0;
    }

    LRESULT CALLBACK PageHostSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData)
    {
        (void)referenceData;
        switch (message)
        {
        case WM_ERASEBKGND:
        {
            RECT client{};
            GetClientRect(window, &client);
            DrawNeonBackground(reinterpret_cast<HDC>(wParam), client, false);
            return 1;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            DrawNeonBackground(dc, client, false);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, g_textColor);
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetWindowBackgroundBrush());
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORBTN:
            return ForwardChildMessageToRoot(window, message, wParam, lParam);

        case WM_DRAWITEM:
        case WM_MEASUREITEM:
        case WM_COMMAND:
        case WM_HSCROLL:
        case WM_NOTIFY:
            return ForwardChildMessageToRoot(window, message, wParam, lParam);

        case WM_NCDESTROY:
            RemoveWindowSubclass(window, PageHostSubclassProcedure, subclassId);
            break;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    LRESULT CALLBACK CardPanelSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData)
    {
        (void)referenceData;
        switch (message)
        {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);

            FillRect(dc, &client, GetWindowBackgroundBrush());
            RECT card = client;
            InflateRect(&card, -1, -1);
            FillRoundedSolid(dc, card, g_panelBackgroundColor, 16);
            DrawRoundedBorder(dc, card, g_borderColor, 16, 1.0f);

            wchar_t title[96] = {};
            GetWindowTextW(window, title, 96);
            if (title[0] != L'\0')
            {
                RECT titleRect = card;
                titleRect.left += 18;
                titleRect.top += 10;
                titleRect.right -= 18;
                titleRect.bottom = titleRect.top + 24;
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, g_textColor);
                HGDIOBJ oldFont = SelectObject(dc, g_uiBoldFont);
                DrawTextW(dc, title, -1, &titleRect,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(dc, oldFont);
            }
            EndPaint(window, &paint);
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, g_textColor);
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetPanelBackgroundBrush());
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, g_textColor);
            SetBkColor(dc, g_panelBackgroundColor);
            return reinterpret_cast<LRESULT>(GetPanelBackgroundBrush());
        }
        case WM_CTLCOLORBTN:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, g_textColor);
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetPanelBackgroundBrush());
        }

        case WM_DRAWITEM:
        case WM_MEASUREITEM:
        case WM_COMMAND:
        case WM_HSCROLL:
        case WM_NOTIFY:
            return ForwardChildMessageToRoot(window, message, wParam, lParam);

        case WM_NCDESTROY:
            RemoveWindowSubclass(window, CardPanelSubclassProcedure, subclassId);
            break;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    LRESULT CALLBACK DropZoneSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData)
    {
        (void)referenceData;
        switch (message)
        {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            FillRect(dc, &client, GetWindowBackgroundBrush());
            RECT zone = client;
            InflateRect(&zone, -1, -1);
            FillRoundedSolid(dc, zone, IsDarkThemeActive() ? RGB(3, 10, 25) : g_panelBackgroundColor, 16);
            if (EnsureGdiPlus())
            {
                Gdiplus::Graphics graphics(dc);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::RectF bounds(1.5f, 1.5f,
                    static_cast<float>(client.right - 3),
                    static_cast<float>(client.bottom - 3));
                Gdiplus::GraphicsPath path;
                AddRoundedPath(path, bounds, 8.0f);
                Gdiplus::Pen pen(Gdiplus::Color(190, 77, 92, 122), 1.0f);
                pen.SetDashStyle(Gdiplus::DashStyleDash);
                graphics.DrawPath(&pen, &path);
            }
            EndPaint(window, &paint);
            return 0;
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, g_mutedTextColor);
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetStockObject(HOLLOW_BRUSH));
        }
        case WM_COMMAND:
        case WM_NOTIFY:
            return ForwardChildMessageToRoot(window, message, wParam, lParam);
        case WM_NCDESTROY:
            RemoveWindowSubclass(window, DropZoneSubclassProcedure, subclassId);
            break;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    bool SetProfileEnabledState(std::size_t profileIndex, bool enabled)
    {
        if (profileIndex >= g_gameProfiles.size())
            return false;

        GameProfile& profile = g_gameProfiles[profileIndex];
        if (profile.enabled == enabled)
            return true;

        profile.enabled = enabled;
        InvalidateRect(g_gameList, nullptr, FALSE);
        if (g_toggleGameButton != nullptr &&
            g_editingProfileIndex == static_cast<int>(profileIndex))
        {
            SendMessageW(g_toggleGameButton, BM_SETCHECK,
                enabled ? BST_CHECKED : BST_UNCHECKED, 0);
            InvalidateRect(g_toggleGameButton, nullptr, FALSE);
        }

        if (!SaveGameProfiles())
        {
            MessageBoxW(GetParent(g_gameList),
                L"The profile state changed, but it could not be saved.",
                L"ArcVibrance", MB_OK | MB_ICONWARNING);
            return false;
        }

        g_agentClient.ReloadProfiles(&g_agentStatus);
        return true;
    }

    LRESULT CALLBACK GameListSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData)
    {
        if (message == WM_ERASEBKGND)
        {
            RECT client{};
            GetClientRect(window, &client);
            FillRect(reinterpret_cast<HDC>(wParam), &client, GetWindowBackgroundBrush());
            return 1;
        }
        if (message == WM_LBUTTONUP)
        {
            POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const DWORD itemResult = static_cast<DWORD>(
                SendMessageW(window, LB_ITEMFROMPOINT, 0, MAKELPARAM(point.x, point.y)));
            const int itemIndex = LOWORD(itemResult);
            const bool outside = HIWORD(itemResult) != 0;

            if (!outside && itemIndex >= 0 &&
                static_cast<std::size_t>(itemIndex) < g_gameProfiles.size())
            {
                RECT itemRect{};
                if (SendMessageW(window, LB_GETITEMRECT, itemIndex,
                    reinterpret_cast<LPARAM>(&itemRect)) != LB_ERR)
                {
                    RECT dotsRect = itemRect;
                    dotsRect.left = dotsRect.right - 34;
                    dotsRect.right -= 6;
                    dotsRect.top += 10;
                    dotsRect.bottom -= 10;
                    if (PtInRect(&dotsRect, point))
                    {
                        SendMessageW(window, LB_SETCURSEL, itemIndex, 0);
                        HMENU menu = CreatePopupMenu();
                        if (menu != nullptr)
                        {
                            AppendMenuW(menu, MF_STRING, 1, L"Edit profile");
                            AppendMenuW(menu, MF_STRING, 2,
                                g_gameProfiles[static_cast<std::size_t>(itemIndex)].enabled
                                    ? L"Disable profile" : L"Enable profile");
                            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                            AppendMenuW(menu, MF_STRING, 3, L"Remove");
                            POINT screenPoint = point;
                            ClientToScreen(window, &screenPoint);
                            const UINT command = TrackPopupMenu(menu,
                                TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                screenPoint.x, screenPoint.y, 0, GetAncestor(window, GA_ROOT), nullptr);
                            DestroyMenu(menu);
                            HWND root = GetAncestor(window, GA_ROOT);
                            if (command == 1)
                                OpenProfileEditor(root, itemIndex);
                            else if (command == 2)
                                SetProfileEnabledState(static_cast<std::size_t>(itemIndex),
                                    !g_gameProfiles[static_cast<std::size_t>(itemIndex)].enabled);
                            else if (command == 3)
                                SendMessageW(root, WM_COMMAND,
                                    MAKEWPARAM(IDC_REMOVE_GAME_BUTTON, BN_CLICKED),
                                    reinterpret_cast<LPARAM>(g_removeGameButton));
                        }
                        return 0;
                    }

                    RECT toggleRect = itemRect;
                    toggleRect.left = toggleRect.right - 92;
                    toggleRect.right -= 48;
                    const int centerY = (itemRect.top + itemRect.bottom) / 2;
                    toggleRect.top = centerY - 11;
                    toggleRect.bottom = centerY + 11;

                    if (PtInRect(&toggleRect, point))
                    {
                        SendMessageW(window, LB_SETCURSEL, itemIndex, 0);
                        const bool newState = !g_gameProfiles[static_cast<std::size_t>(itemIndex)].enabled;
                        SetProfileEnabledState(static_cast<std::size_t>(itemIndex), newState);
                        return 0;
                    }
                }
            }
        }
        else if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(window, GameListSubclassProcedure, subclassId);
        }

        return DefSubclassProc(window, message, wParam, lParam);
    }

    std::wstring ReadVersionString(
        const std::wstring& executablePath,
        const wchar_t* valueName)
    {
        DWORD ignored = 0;
        const DWORD size = GetFileVersionInfoSizeW(
            executablePath.c_str(), &ignored);
        if (size == 0) return {};

        std::vector<BYTE> data(size);
        if (!GetFileVersionInfoW(executablePath.c_str(), 0, size, data.data()))
            return {};

        struct Translation { WORD language; WORD codePage; };
        Translation* translations = nullptr;
        UINT translationBytes = 0;
        if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
            reinterpret_cast<void**>(&translations), &translationBytes) ||
            translations == nullptr || translationBytes < sizeof(Translation))
            return {};

        const UINT count = translationBytes / sizeof(Translation);
        for (UINT index = 0; index < count; ++index)
        {
            wchar_t query[128] = {};
            swprintf_s(query, L"\\StringFileInfo\\%04x%04x\\%s",
                translations[index].language, translations[index].codePage, valueName);
            wchar_t* value = nullptr;
            UINT valueLength = 0;
            if (VerQueryValueW(data.data(), query,
                reinterpret_cast<void**>(&value), &valueLength) &&
                value != nullptr && valueLength > 1)
                return value;
        }
        return {};
    }

    std::wstring GetFriendlyGameName(const GameProfile& profile)
    {
        const auto cached = g_friendlyNameCache.find(profile.executablePath);
        if (cached != g_friendlyNameCache.end()) return cached->second;

        std::wstring name = ReadVersionString(profile.executablePath, L"ProductName");
        if (name.empty()) name = ReadVersionString(profile.executablePath, L"FileDescription");
        if (name.empty())
        {
            name = profile.executableName;
            if (name.size() > 4 && _wcsicmp(name.c_str() + name.size() - 4, L".exe") == 0)
                name.resize(name.size() - 4);
        }
        g_friendlyNameCache.emplace(profile.executablePath, name);
        return name;
    }

    bool IsExecutableFile(const std::wstring& path)
    {
        return path.size() >= 4 &&
            _wcsicmp(path.c_str() + path.size() - 4, L".exe") == 0;
    }

    void UpdateProfileEmptyState()
    {
        if (g_profileEmptyState == nullptr) return;
        ShowWindow(g_profileEmptyState,
            g_gameProfiles.empty() ? SW_SHOW : SW_HIDE);
    }

    HICON LoadExecutableIcon(
        const std::wstring& executablePath)
    {
        const auto existingIcon =
            g_executableIconCache.find(
                executablePath);

        if (existingIcon !=
            g_executableIconCache.end())
        {
            return existingIcon->second;
        }

        SHFILEINFOW fileInfo = {};

        const DWORD_PTR result =
            SHGetFileInfoW(
                executablePath.c_str(),
                0,
                &fileInfo,
                sizeof(fileInfo),
                SHGFI_ICON |
                SHGFI_LARGEICON);

        if (result == 0 ||
            fileInfo.hIcon == nullptr)
        {
            return nullptr;
        }

        g_executableIconCache.emplace(
            executablePath,
            fileInfo.hIcon);

        return fileInfo.hIcon;
    }

    void RemoveExecutableIcon(
        const std::wstring& executablePath)
    {
        const auto iconEntry =
            g_executableIconCache.find(
                executablePath);

        if (iconEntry ==
            g_executableIconCache.end())
        {
            return;
        }

        if (iconEntry->second != nullptr)
        {
            DestroyIcon(iconEntry->second);
        }

        g_executableIconCache.erase(
            iconEntry);
    }

    void DestroyExecutableIconCache()
    {
        for (const auto& iconEntry :
            g_executableIconCache)
        {
            if (iconEntry.second != nullptr)
            {
                DestroyIcon(
                    iconEntry.second);
            }
        }

        g_executableIconCache.clear();
    }

    void DrawExecutableIcon(
        HDC deviceContext,
        const RECT& itemRect,
        const std::wstring& executablePath)
    {
        const HICON executableIcon =
            LoadExecutableIcon(
                executablePath);

        if (executableIcon == nullptr)
        {
            return;
        }

        constexpr int iconSize = 48;

        const int iconX =
            itemRect.left + 14;

        const int iconY =
            itemRect.top +
            ((itemRect.bottom -
                itemRect.top -
                iconSize) / 2);

        DrawIconEx(
            deviceContext,
            iconX,
            iconY,
            executableIcon,
            iconSize,
            iconSize,
            0,
            nullptr,
            DI_NORMAL);
    }
    void DrawGameProfileItem(const DRAWITEMSTRUCT& drawItem)
    {
        if (drawItem.itemID == static_cast<UINT>(-1)) return;
        const std::size_t profileIndex =
            static_cast<std::size_t>(drawItem.itemID);
        if (profileIndex >= g_gameProfiles.size()) return;

        const GameProfile& profile = g_gameProfiles[profileIndex];
        HDC dc = drawItem.hDC;
        RECT outerRect = drawItem.rcItem;
        const bool selected = (drawItem.itemState & ODS_SELECTED) != 0;
        const bool focused = (drawItem.itemState & ODS_FOCUS) != 0;

        FillRect(dc, &outerRect, GetWindowBackgroundBrush());

        RECT cardRect = outerRect;
        cardRect.left += 4;
        cardRect.right -= 4;
        cardRect.top += 4;
        cardRect.bottom -= 4;

        const COLORREF cardColor = IsDarkThemeActive()
            ? (selected ? RGB(7, 18, 39) : RGB(5, 14, 31))
            : g_panelBackgroundColor;
        if (selected)
        {
            FillRoundedGradientBorder(dc, cardRect,
                RGB(0, 190, 255), RGB(78, 59, 255), RGB(239, 26, 190),
                cardColor, 16, 2);
        }
        else
        {
            FillRoundedSolid(dc, cardRect, cardColor, 16);
            DrawRoundedBorder(dc, cardRect, RGB(22, 39, 66), 16, 1.0f);
        }

        SetBkMode(dc, TRANSPARENT);
        RECT contentRect = cardRect;
        contentRect.left += 8;
        contentRect.right -= 8;
        DrawExecutableIcon(dc, contentRect, profile.executablePath);

        HFONT oldFont = reinterpret_cast<HFONT>(
            SelectObject(dc, g_uiBoldFont));
        RECT nameRect = cardRect;
        nameRect.left += 82;
        nameRect.right -= 190;
        nameRect.top += 12;
        nameRect.bottom = nameRect.top + 22;
        SetTextColor(dc, profile.enabled ? g_textColor : g_mutedTextColor);
        const std::wstring friendlyName = GetFriendlyGameName(profile);
        DrawTextW(dc, friendlyName.c_str(), -1, &nameRect,
            DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        SelectObject(dc, g_uiFont);
        RECT executableRect = cardRect;
        executableRect.left += 82;
        executableRect.right -= 190;
        executableRect.top += 38;
        executableRect.bottom = executableRect.top + 18;
        SetTextColor(dc, g_mutedTextColor);
        DrawTextW(dc, profile.executablePath.c_str(), -1, &executableRect,
            DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        const std::wstring saturationText =
            std::to_wstring(profile.saturationPercent) + L"%";
        RECT saturationRect = cardRect;
        saturationRect.left = saturationRect.right - 182;
        saturationRect.right -= 106;
        const COLORREF saturationColor = profile.saturationPercent >= 150
            ? RGB(240, 35, 194) : RGB(26, 190, 255);
        SetTextColor(dc, profile.enabled ? saturationColor : g_mutedTextColor);
        SelectObject(dc, g_uiBoldFont);
        DrawTextW(dc, saturationText.c_str(), -1, &saturationRect,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        RECT toggleRect = cardRect;
        toggleRect.left = toggleRect.right - 88;
        toggleRect.right -= 44;
        const int toggleHeight = 22;
        const int centerY = (cardRect.top + cardRect.bottom) / 2;
        toggleRect.top = centerY - toggleHeight / 2;
        toggleRect.bottom = toggleRect.top + toggleHeight;
        FillRoundedSolid(dc, toggleRect,
            profile.enabled ? RGB(49, 91, 220) : RGB(48, 57, 75),
            toggleHeight);
        DrawRoundedBorder(dc, toggleRect,
            profile.enabled ? RGB(86, 137, 255) : RGB(78, 88, 108),
            toggleHeight, 1.0f);

        RECT knobRect = toggleRect;
        const int knobInset = 3;
        const int knobSize = toggleHeight - knobInset * 2;
        knobRect.top += knobInset;
        knobRect.bottom = knobRect.top + knobSize;
        if (profile.enabled)
        {
            knobRect.right -= knobInset;
            knobRect.left = knobRect.right - knobSize;
        }
        else
        {
            knobRect.left += knobInset;
            knobRect.right = knobRect.left + knobSize;
        }
        FillRoundedSolid(dc, knobRect, RGB(245, 248, 255), knobSize);

        if (EnsureGdiPlus())
        {
            Gdiplus::Graphics graphics(dc);
            Gdiplus::SolidBrush dots(ToGdiPlusColor(g_textColor));
            const float dotsX = static_cast<float>(cardRect.right - 22);
            const float dotsY = static_cast<float>((cardRect.top + cardRect.bottom) / 2 - 7);
            for (int dot = 0; dot < 3; ++dot)
                graphics.FillEllipse(&dots, dotsX, dotsY + dot * 6.0f, 2.6f, 2.6f);
        }

        (void)focused;
        SelectObject(dc, oldFont);
    }
    void DrawStatusDot(
        const DRAWITEMSTRUCT& drawItem)
    {
        HDC deviceContext =
            drawItem.hDC;

        RECT dotRect =
            drawItem.rcItem;

        FillRect(
            deviceContext,
            &dotRect,
            GetPanelBackgroundBrush());

        InflateRect(
            &dotRect,
            -2,
            -2);

        const COLORREF dotColor =
            g_agentStatus.gameActive != FALSE
            ? RGB(70, 220, 130)
            : RGB(110, 125, 150);

        HBRUSH dotBrush =
            CreateSolidBrush(dotColor);

        HPEN dotPen =
            CreatePen(
                PS_SOLID,
                1,
                dotColor);

        HGDIOBJ previousBrush =
            SelectObject(
                deviceContext,
                dotBrush);

        HGDIOBJ previousPen =
            SelectObject(
                deviceContext,
                dotPen);

        Ellipse(
            deviceContext,
            dotRect.left,
            dotRect.top,
            dotRect.right,
            dotRect.bottom);

        SelectObject(
            deviceContext,
            previousPen);

        SelectObject(
            deviceContext,
            previousBrush);

        DeleteObject(dotPen);
        DeleteObject(dotBrush);
    }
    void AddGameProfile(
        const std::wstring& executablePath)
    {
        for (const GameProfile& existingProfile :
            g_gameProfiles)
        {
            if (GameMonitor::PathsEqual(
                existingProfile.executablePath,
                executablePath))
            {
                MessageBoxW(
                    nullptr,
                    L"This game is already in the profile list.",
                    L"ArcVibrance",
                    MB_OK | MB_ICONINFORMATION);

                return;
            }
        }

        const std::wstring newExecutableName =
            GameMonitor::GetExecutableName(executablePath);
        for (const GameProfile& existingProfile : g_gameProfiles)
        {
            if (_wcsicmp(existingProfile.executableName.c_str(),
                newExecutableName.c_str()) == 0)
            {
                const int choice = MessageBoxW(nullptr,
                    L"A profile with the same executable name already exists.\n\nAdd this different path anyway?",
                    L"ArcVibrance", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                if (choice != IDYES) return;
                break;
            }
        }

        GameProfile profile;
        profile.executablePath = executablePath;
        profile.executableName = newExecutableName;

        profile.saturationPercent = 150;

        LoadExecutableIcon(
            profile.executablePath);

        const std::wstring executableName = profile.executableName;
        g_gameProfiles.push_back(
            std::move(profile));

        SendMessageW(
            g_gameList,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(
                executableName.c_str()));
        UpdateProfileEmptyState();
        if (!SaveGameProfiles())
        {
            MessageBoxW(
                nullptr,
                L"The profile was added, but it could not be saved.",
                L"ArcVibrance",
                MB_OK | MB_ICONWARNING);
        }
        else
        {
            g_agentClient.ReloadProfiles(&g_agentStatus);
        }
    }
    void UpdateProfileSliderLabel()
    {
        if (g_profileSlider == nullptr)
        {
            return;
        }

        const int value =
            static_cast<int>(
                SendMessageW(
                    g_profileSlider,
                    TBM_GETPOS,
                    0,
                    0));

        if (g_profileValueEdit != nullptr)
        {
            SetNumericSpinnerValue(g_profileValueEdit, value, false);
        }

        if (g_profileValueLabel != nullptr)
        {
            SetWindowTextW(
                g_profileValueLabel,
                L"%");
        }
    }

    void CommitProfileSaturation(int value, bool redrawProfileList)
    {
        value = (value < SATURATION_MIN)
            ? SATURATION_MIN
            : ((value > SATURATION_MAX)
                ? SATURATION_MAX
                : value);

        if (g_profileSlider != nullptr)
        {
            const int currentSliderValue = static_cast<int>(
                SendMessageW(g_profileSlider, TBM_GETPOS, 0, 0));
            if (currentSliderValue != value)
            {
                SendMessageW(g_profileSlider, TBM_SETPOS, FALSE, value);
            }
        }

        if (g_editingProfileIndex >= 0 &&
            g_editingProfileIndex <
            static_cast<int>(g_gameProfiles.size()))
        {
            g_gameProfiles[g_editingProfileIndex]
                .saturationPercent = value;

            if (redrawProfileList && g_gameList != nullptr)
            {
                InvalidateRect(g_gameList, nullptr, FALSE);
            }

            // Changes are committed by the inline panel's Save button.
        }
    }
    void OpenProfileEditor(HWND ownerWindow, int profileIndex)
    {
        if (profileIndex < 0 || profileIndex >= static_cast<int>(g_gameProfiles.size()) ||
            g_profileEditorWindow == nullptr)
            return;

        if (g_editingProfileIndex >= 0 &&
            g_editingProfileIndex < static_cast<int>(g_gameProfiles.size()) &&
            g_editingProfileIndex != profileIndex)
        {
            g_gameProfiles[g_editingProfileIndex].saturationPercent = g_originalProfileSaturation;
        }

        g_editingProfileIndex = profileIndex;
        const GameProfile& profile = g_gameProfiles[profileIndex];
        g_originalProfileSaturation = profile.saturationPercent;
        SetWindowTextW(g_profileNameLabel, GetFriendlyGameName(profile).c_str());
        SetWindowTextW(g_profilePathLabel, profile.executablePath.c_str());
        SendMessageW(g_profileSlider, TBM_SETPOS, TRUE, profile.saturationPercent);
        if (g_toggleGameButton != nullptr)
        {
            SendMessageW(g_toggleGameButton, BM_SETCHECK,
                profile.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
            InvalidateRect(g_toggleGameButton, nullptr, FALSE);
        }
        UpdateProfileSliderLabel();
        ShowWindow(g_profileEditorWindow, SW_SHOW);

        // Redraw only the inline editor. Invalidating the entire main window here
        // could repaint the panel background before its child controls received
        // their own paint messages, leaving the editor visually blank when it
        // was opened again. RDW_ALLCHILDREN keeps every label and control fresh.
        RedrawWindow(
            g_profileEditorWindow,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);

        if (g_gameList != nullptr)
        {
            InvalidateRect(g_gameList, nullptr, FALSE);
        }

        SetFocus(g_profileSlider);
    }

    bool RegisterProfileEditorClass(
        HINSTANCE instance)
    {
        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc =
            ProfileEditorProcedure;
        windowClass.hInstance = instance;
        windowClass.hCursor =
            LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
        windowClass.hbrBackground = nullptr;
        windowClass.lpszClassName =
            PROFILE_EDITOR_CLASS_NAME;

        const ATOM result =
            RegisterClassExW(&windowClass);

        if (result != 0)
        {
            return true;
        }

        return GetLastError() ==
            ERROR_CLASS_ALREADY_EXISTS;
    }

    void SetControlVisible(HWND control, bool visible)
    {
        if (control == nullptr) return;
        if ((IsWindowVisible(control) != FALSE) != visible)
            ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
    }

    // Page hosts isolate every screen so controls from one page can never
    // overlap another page. Re-selecting the current page is a no-op.
    void ShowMainPage(MainPage page)
    {
        if (g_pageLayoutInitialized && g_currentPage == page)
            return;

        g_currentPage = page;

        if (g_profilesPageHost != nullptr)
            ShowWindow(g_profilesPageHost, page == MainPage::Profiles ? SW_SHOW : SW_HIDE);
        if (g_settingsPageHost != nullptr)
            ShowWindow(g_settingsPageHost, page == MainPage::Settings ? SW_SHOW : SW_HIDE);
        if (g_aboutPageHost != nullptr)
            ShowWindow(g_aboutPageHost, page == MainPage::About ? SW_SHOW : SW_HIDE);

        if (page == MainPage::Profiles)
            UpdateProfileEmptyState();

        g_pageLayoutInitialized = true;

        const HWND navigation[] = { g_profilesNav, g_settingsNav, g_aboutNav };
        for (HWND control : navigation)
        {
            if (control != nullptr)
                InvalidateRect(control, nullptr, FALSE);
        }

        HWND activeHost = page == MainPage::Profiles ? g_profilesPageHost :
            (page == MainPage::Settings ? g_settingsPageHost : g_aboutPageHost);
        if (activeHost != nullptr)
        {
            SetWindowPos(activeHost, HWND_TOP, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            RedrawWindow(activeHost, nullptr, nullptr,
                RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
    }

    bool CreateMainControls(
        HWND window,
        HINSTANCE instance)
    {
        g_brandFont = CreateFontW(-27, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_pageTitleFont = CreateFontW(-23, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_brandLogo = CreateWindowExW(
            0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            34, 20, 160, 118,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BRAND_LOGO)), instance, nullptr);

        HWND brandLabel = CreateWindowExW(
            0, L"STATIC", L"ArcVibrance",
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            24, 145, 196, 38,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BRAND_NAME)),
            instance, nullptr);

        HWND brandSubtitle = CreateWindowExW(
            0, L"STATIC", L"V I B R A N T   C O L O R S .\r\nB E T T E R   G A M I N G .",
            WS_CHILD | WS_VISIBLE,
            31, 183, 185, 48,
            window, nullptr, instance, nullptr);

        g_profilesNav = CreateWindowExW(
            0, L"BUTTON", L"Profiles",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | BS_OWNERDRAW,
            23, 248, 194, 52,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NAV_PROFILES)), instance, nullptr);

        g_settingsNav = CreateWindowExW(
            0, L"BUTTON", L"Settings",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | BS_OWNERDRAW,
            23, 308, 194, 52,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NAV_SETTINGS)), instance, nullptr);

        g_aboutNav = CreateWindowExW(
            0, L"BUTTON", L"About",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | BS_OWNERDRAW,
            23, 368, 194, 52,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NAV_ABOUT)), instance, nullptr);

        g_statusGroup = CreateWindowExW(
            0, L"STATIC", L"Monitoring",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            23, 600, 194, 146,
            window, nullptr, instance, nullptr);

        g_statusDot = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            16, 36, 16, 16,
            g_statusGroup,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS_DOT)),
            instance, nullptr);

        g_statusGameLabel = CreateWindowExW(
            0, L"STATIC", L"No configured game active",
            WS_CHILD | WS_VISIBLE,
            16, 62, 162, 42,
            g_statusGroup, nullptr, instance, nullptr);

        g_statusSaturationLabel = CreateWindowExW(
            0, L"STATIC", L"Desktop saturation: 100%",
            WS_CHILD | WS_VISIBLE,
            16, 112, 164, 24,
            g_statusGroup, nullptr, instance, nullptr);

        g_profilesPageHost = CreateWindowExW(
            0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            220, 0, 1044, 780,
            window, nullptr, instance, nullptr);

        g_settingsPageHost = CreateWindowExW(
            0, L"STATIC", nullptr,
            WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            220, 0, 1044, 780,
            window, nullptr, instance, nullptr);

        g_aboutPageHost = CreateWindowExW(
            0, L"STATIC", nullptr,
            WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            220, 0, 1044, 780,
            window, nullptr, instance, nullptr);

        g_pageTitle = CreateWindowExW(
            0, L"STATIC", L"Game Profiles",
            WS_CHILD | WS_VISIBLE,
            30, 20, 350, 36,
            g_profilesPageHost, nullptr, instance, nullptr);

        g_pageSubtitle = CreateWindowExW(
            0, L"STATIC", L"Manage saturation for your games",
            WS_CHILD | WS_VISIBLE,
            30, 56, 430, 22,
            g_profilesPageHost, nullptr, instance, nullptr);

        g_settingsPageTitle = CreateWindowExW(
            0, L"STATIC", L"Settings",
            WS_CHILD | WS_VISIBLE,
            30, 20, 350, 36,
            g_settingsPageHost, nullptr, instance, nullptr);

        g_settingsPageSubtitle = CreateWindowExW(
            0, L"STATIC", L"Configure startup, close behavior, and appearance",
            WS_CHILD | WS_VISIBLE,
            30, 56, 560, 22,
            g_settingsPageHost, nullptr, instance, nullptr);

        g_aboutPageTitle = CreateWindowExW(
            0, L"STATIC", L"About ArcVibrance",
            WS_CHILD | WS_VISIBLE,
            30, 20, 420, 36,
            g_aboutPageHost, nullptr, instance, nullptr);

        g_aboutPageSubtitle = CreateWindowExW(
            0, L"STATIC", L"Application information and project details",
            WS_CHILD | WS_VISIBLE,
            30, 56, 560, 22,
            g_aboutPageHost, nullptr, instance, nullptr);

        g_addGameButton = CreateWindowExW(
            0, L"BUTTON", L"+  Add Game",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            474, 18, 142, 42,
            g_profilesPageHost,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ADD_GAME_BUTTON)),
            instance, nullptr);

        g_profilesGroup = CreateWindowExW(
            0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            24, 104, 610, 426,
            g_profilesPageHost, nullptr, instance, nullptr);

        g_gameList = CreateWindowExW(
            0, L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP |
                LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            0, 0, 610, 426,
            g_profilesGroup,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_GAME_LIST)),
            instance, nullptr);

        g_profileEmptyState = CreateWindowExW(
            0, L"STATIC",
            L"No game profiles yet\r\nClick Add Game or drop an executable below.",
            WS_CHILD | SS_CENTER,
            88, 156, 430, 62,
            g_profilesGroup, nullptr, instance, nullptr);

        g_dropZoneGroup = CreateWindowExW(
            0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            24, 540, 610, 80,
            g_profilesPageHost, nullptr, instance, nullptr);

        g_instructionLabel = CreateWindowExW(
            0, L"STATIC",
            L"Drag and drop game executable(s) here\r\nor click Add Game to browse",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            54, 18, 502, 46,
            g_dropZoneGroup, nullptr, instance, nullptr);

        g_quickActionsGroup = CreateWindowExW(
            0, L"STATIC", L"Quick Actions",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            24, 632, 610, 112,
            g_profilesPageHost, nullptr, instance, nullptr);

        g_editGameButton = CreateWindowExW(
            0, L"BUTTON", L"Edit Profile",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            18, 48, 132, 44,
            g_quickActionsGroup,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EDIT_GAME_BUTTON)),
            instance, nullptr);

        g_removeGameButton = CreateWindowExW(
            0, L"BUTTON", L"Remove",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            164, 48, 124, 44,
            g_quickActionsGroup,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_REMOVE_GAME_BUTTON)),
            instance, nullptr);

        g_reloadAgentButton = CreateWindowExW(
            0, L"BUTTON", L"Reload Profiles",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            430, 48, 160, 44,
            g_quickActionsGroup,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RELOAD_AGENT_BUTTON)),
            instance, nullptr);

        g_settingsGroup = CreateWindowExW(
            0, L"STATIC", L"General",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            24, 104, 980, 500,
            g_settingsPageHost, nullptr, instance, nullptr);

        g_autoStartCheckbox = CreateWindowExW(
            0, L"BUTTON", L"Start ArcVibrance with Windows",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            34, 68, 360, 36,
            g_settingsGroup,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_AUTOSTART_CHECKBOX)),
            instance, nullptr);

        g_closeBehaviorLabel = CreateWindowExW(
            0, L"STATIC", L"When closing:",
            WS_CHILD | WS_VISIBLE,
            34, 150, 120, 26,
            g_settingsGroup, nullptr, instance, nullptr);

        g_closeMinimizeRadio = CreateWindowExW(
            0, L"BUTTON", L"Minimize to tray",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            170, 144, 190, 38,
            g_settingsGroup,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CLOSE_MINIMIZE_RADIO)),
            instance, nullptr);

        g_closeExitRadio = CreateWindowExW(
            0, L"BUTTON", L"Exit completely",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            390, 144, 190, 38,
            g_settingsGroup,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CLOSE_EXIT_RADIO)),
            instance, nullptr);

        g_themeLabel = CreateWindowExW(
            0, L"STATIC", L"Theme:",
            WS_CHILD | WS_VISIBLE,
            34, 244, 100, 30,
            g_settingsGroup, nullptr, instance, nullptr);

        g_themeCombo = CreateWindowExW(
            0, L"COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST |
                CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
            170, 238, 260, 150,
            g_settingsGroup,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_THEME_COMBO)),
            instance, nullptr);
        SendMessageW(g_themeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"System"));
        SendMessageW(g_themeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Light"));
        SendMessageW(g_themeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dark"));
        SendMessageW(g_themeCombo, CB_SETCURSEL, static_cast<WPARAM>(GetThemeMode()), 0);
        SendMessageW(g_themeCombo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), 30);
        SendMessageW(g_themeCombo, CB_SETITEMHEIGHT, 0, 30);

        g_aboutGroup = CreateWindowExW(
            0, L"STATIC", L"ArcVibrance",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            24, 104, 980, 520,
            g_aboutPageHost, nullptr, instance, nullptr);

        HWND aboutHeading = CreateWindowExW(
            0, L"STATIC", L"Per-game saturation control for Intel Arc GPUs",
            WS_CHILD | WS_VISIBLE,
            34, 70, 720, 32,
            g_aboutGroup, nullptr, instance, nullptr);
        HWND aboutBody = CreateWindowExW(
            0, L"STATIC",
            L"ArcVibrance automatically detects configured games and applies each profile's "
            L"saturation level through the Intel graphics backend.\r\n\r\n"
            L"This interface keeps the existing monitoring, profile, tray, startup, and IPC logic "
            L"while presenting it in a cleaner neon dashboard.",
            WS_CHILD | WS_VISIBLE,
            34, 124, 860, 120,
            g_aboutGroup, nullptr, instance, nullptr);
        HWND aboutVersion = CreateWindowExW(
            0, L"STATIC", L"UI redesign milestone B3.0",
            WS_CHILD | WS_VISIBLE,
            34, 290, 420, 28,
            g_aboutGroup, nullptr, instance, nullptr);

        g_profileEditorWindow = CreateWindowExW(
            WS_EX_CONTROLPARENT, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            654, 18, 370, 726, g_profilesPageHost, nullptr, instance, nullptr);
        HWND editorTitle = CreateWindowExW(
            0, L"STATIC", L"Edit Profile", WS_CHILD | WS_VISIBLE,
            22, 18, 300, 32, g_profileEditorWindow, nullptr, instance, nullptr);
        g_profileNameLabel = CreateWindowExW(
            0, L"STATIC", L"Select a profile", WS_CHILD | WS_VISIBLE,
            22, 88, 326, 32, g_profileEditorWindow, nullptr, instance, nullptr);
        g_profilePathLabel = CreateWindowExW(
            0, L"STATIC", L"Choose a game from the list to edit it here.",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_PATHELLIPSIS,
            22, 124, 326, 44, g_profileEditorWindow, nullptr, instance, nullptr);
        HWND saturationLabel = CreateWindowExW(
            0, L"STATIC", L"Saturation", WS_CHILD | WS_VISIBLE,
            22, 210, 180, 24, g_profileEditorWindow, nullptr, instance, nullptr);
        HWND minimumLabel = CreateWindowExW(
            0, L"STATIC", L"0%", WS_CHILD | WS_VISIBLE,
            22, 246, 40, 20, g_profileEditorWindow, nullptr, instance, nullptr);
        HWND maximumLabel = CreateWindowExW(
            0, L"STATIC", L"300%", WS_CHILD | WS_VISIBLE | SS_RIGHT,
            298, 246, 50, 20, g_profileEditorWindow, nullptr, instance, nullptr);
        g_profileSlider = CreateWindowExW(
            0, TRACKBAR_CLASSW, nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            22, 270, 326, 40, g_profileEditorWindow,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROFILE_SLIDER)), instance, nullptr);
        g_profileValueEdit = CreateWindowExW(
            0, L"STATIC", L"100",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | SS_NOTIFY,
            106, 322, 158, 58, g_profileEditorWindow,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROFILE_VALUE_EDIT)), instance, nullptr);
        g_profileValueLabel = nullptr;
        HWND profileStatusLabel = CreateWindowExW(
            0, L"STATIC", L"Profile Status", WS_CHILD | WS_VISIBLE,
            22, 430, 190, 26, g_profileEditorWindow, nullptr, instance, nullptr);
        g_toggleGameButton = CreateWindowExW(
            0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            288, 424, 58, 30, g_profileEditorWindow,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TOGGLE_GAME_BUTTON)), instance, nullptr);
        g_profileCancelButton = CreateWindowExW(
            0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            22, 652, 148, 48, g_profileEditorWindow,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROFILE_CANCEL)), instance, nullptr);
        HWND saveButton = CreateWindowExW(
            0, L"BUTTON", L"Save Changes", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            184, 652, 164, 48, g_profileEditorWindow,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROFILE_CLOSE)), instance, nullptr);
        SendMessageW(g_profileSlider, TBM_SETRANGE, TRUE, MAKELPARAM(SATURATION_MIN, SATURATION_MAX));
        SendMessageW(g_profileSlider, TBM_SETTICFREQ, 10, 0);
        SendMessageW(g_toggleGameButton, BM_SETCHECK, BST_UNCHECKED, 0);

        if (g_brandLogo == nullptr || brandLabel == nullptr || brandSubtitle == nullptr ||
            g_profilesNav == nullptr || g_settingsNav == nullptr || g_aboutNav == nullptr ||
            g_statusGroup == nullptr || g_statusDot == nullptr ||
            g_statusGameLabel == nullptr || g_statusSaturationLabel == nullptr ||
            g_profilesPageHost == nullptr || g_settingsPageHost == nullptr ||
            g_aboutPageHost == nullptr || g_pageTitle == nullptr || g_pageSubtitle == nullptr ||
            g_settingsPageTitle == nullptr || g_settingsPageSubtitle == nullptr ||
            g_aboutPageTitle == nullptr || g_aboutPageSubtitle == nullptr ||
            g_profilesGroup == nullptr || g_dropZoneGroup == nullptr ||
            g_quickActionsGroup == nullptr || g_instructionLabel == nullptr ||
            g_gameList == nullptr || g_addGameButton == nullptr || g_editGameButton == nullptr ||
            g_removeGameButton == nullptr || g_reloadAgentButton == nullptr ||
            g_settingsGroup == nullptr || g_autoStartCheckbox == nullptr ||
            g_closeBehaviorLabel == nullptr || g_closeMinimizeRadio == nullptr ||
            g_closeExitRadio == nullptr || g_themeLabel == nullptr || g_themeCombo == nullptr ||
            g_aboutGroup == nullptr || aboutHeading == nullptr || aboutBody == nullptr ||
            aboutVersion == nullptr || g_profileEditorWindow == nullptr || editorTitle == nullptr ||
            g_profileNameLabel == nullptr || g_profilePathLabel == nullptr ||
            saturationLabel == nullptr || minimumLabel == nullptr || maximumLabel == nullptr ||
            g_profileSlider == nullptr || g_profileValueEdit == nullptr ||
            profileStatusLabel == nullptr || g_toggleGameButton == nullptr ||
            g_profileCancelButton == nullptr || saveButton == nullptr)
        {
            return false;
        }

        if (!SetWindowSubclass(g_profilesPageHost, PageHostSubclassProcedure, 100, 0) ||
            !SetWindowSubclass(g_settingsPageHost, PageHostSubclassProcedure, 101, 0) ||
            !SetWindowSubclass(g_aboutPageHost, PageHostSubclassProcedure, 102, 0) ||
            !SetWindowSubclass(g_profilesGroup, PageHostSubclassProcedure, 110, 0) ||
            !SetWindowSubclass(g_dropZoneGroup, DropZoneSubclassProcedure, 113, 0) ||
            !SetWindowSubclass(g_quickActionsGroup, CardPanelSubclassProcedure, 114, 0) ||
            !SetWindowSubclass(g_settingsGroup, CardPanelSubclassProcedure, 111, 0) ||
            !SetWindowSubclass(g_aboutGroup, CardPanelSubclassProcedure, 115, 0) ||
            !SetWindowSubclass(g_statusGroup, CardPanelSubclassProcedure, 112, 0))
        {
            return false;
        }

        const struct { HWND window; UINT_PTR id; } buttons[] = {
            { g_addGameButton, 1 }, { g_editGameButton, 2 },
            { g_removeGameButton, 3 }, { g_reloadAgentButton, 4 },
            { g_profileCancelButton, 5 }, { saveButton, 6 },
            { g_profilesNav, 7 }, { g_settingsNav, 8 }, { g_aboutNav, 12 },
            { g_autoStartCheckbox, 9 }, { g_closeMinimizeRadio, 10 },
            { g_closeExitRadio, 11 }, { g_toggleGameButton, 13 }
        };
        for (const auto& button : buttons)
        {
            if (button.window != g_profilesNav && button.window != g_settingsNav &&
                button.window != g_aboutNav)
                ApplyRoundedButtonRegion(button.window);
            if (!SetWindowSubclass(button.window, ButtonSubclassProcedure, button.id, 0))
                return false;
        }

        if (!SetWindowSubclass(g_profileSlider, SliderSubclassProcedure, 70, 0))
            return false;
        if (!SetWindowSubclass(g_profileValueEdit, NumericSpinnerSubclassProcedure, 71, 0))
            return false;
        SetNumericSpinnerValue(g_profileValueEdit, 100, false);

        SendMessageW(g_gameList, LB_SETITEMHEIGHT, 0, 72);
        if (!SetWindowSubclass(g_gameList, GameListSubclassProcedure, 90, 0))
            return false;

        SendMessageW(g_autoStartCheckbox, BM_SETCHECK,
            IsAutoStartEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
        const CloseBehavior closeBehavior = GetCloseBehavior();
        SendMessageW(g_closeMinimizeRadio, BM_SETCHECK,
            closeBehavior == CloseBehavior::MinimizeToTray ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(g_closeExitRadio, BM_SETCHECK,
            closeBehavior == CloseBehavior::ExitCompletely ? BST_CHECKED : BST_UNCHECKED, 0);

        ApplyUiFont(g_brandLogo);
        if (g_brandFont != nullptr)
            SendMessageW(brandLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_brandFont), TRUE);
        if (g_pageTitleFont != nullptr)
        {
            SendMessageW(g_pageTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_pageTitleFont), TRUE);
            SendMessageW(g_settingsPageTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_pageTitleFont), TRUE);
            SendMessageW(g_aboutPageTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_pageTitleFont), TRUE);
            SendMessageW(editorTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_pageTitleFont), TRUE);
        }
        const HWND controls[] = {
            brandSubtitle, g_profilesNav, g_settingsNav, g_aboutNav, g_statusGroup, g_statusDot,
            g_statusGameLabel, g_statusSaturationLabel, g_profilesPageHost, g_settingsPageHost,
            g_aboutPageHost, g_pageSubtitle, g_settingsPageSubtitle, g_aboutPageSubtitle,
            g_profilesGroup, g_dropZoneGroup, g_quickActionsGroup, g_instructionLabel,
            g_gameList, g_profileEmptyState, g_addGameButton, g_editGameButton,
            g_removeGameButton, g_reloadAgentButton, g_settingsGroup, g_autoStartCheckbox,
            g_closeBehaviorLabel, g_closeMinimizeRadio, g_closeExitRadio,
            g_themeLabel, g_themeCombo, g_aboutGroup, aboutHeading, aboutBody, aboutVersion,
            g_profileEditorWindow, g_profileNameLabel, g_profilePathLabel, saturationLabel,
            minimumLabel, maximumLabel, g_profileSlider, g_profileValueEdit,
            profileStatusLabel, g_toggleGameButton, g_profileCancelButton, saveButton
        };
        for (HWND control : controls)
            ApplyUiFont(control);
        ApplyHeaderFont(g_profileNameLabel);
        ApplyHeaderFont(aboutHeading);

        SetWindowSubclass(g_profileEditorWindow, ProfilePanelSubclassProcedure, 80, 0);
        ApplyWindowTheme(g_profileEditorWindow);
        ApplyWindowTheme(g_profileSlider);
        ApplyWindowTheme(g_gameList);
        SetWindowTheme(g_gameList, IsDarkThemeActive() ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        SetWindowTheme(g_themeCombo,
            IsDarkThemeActive() ? L"DarkMode_CFD" : L"CFD", nullptr);
        InvalidateRect(g_profileValueEdit, nullptr, FALSE);

        g_currentPage = MainPage::Profiles;
        g_pageLayoutInitialized = false;
        ShowMainPage(MainPage::Profiles);
        return true;
    }

    bool SelectGameExecutable(
        HWND ownerWindow,
        std::wstring& selectedPath)
    {
        wchar_t filePath[MAX_PATH] = {};

        OPENFILENAMEW dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = ownerWindow;
        dialog.lpstrFile = filePath;
        dialog.nMaxFile = MAX_PATH;

        dialog.lpstrFilter =
            L"Executable files (*.exe)\0*.exe\0"
            L"All files (*.*)\0*.*\0";

        dialog.nFilterIndex = 1;

        dialog.Flags =
            OFN_FILEMUSTEXIST |
            OFN_PATHMUSTEXIST |
            OFN_NOCHANGEDIR;

        dialog.lpstrDefExt = L"exe";

        if (!GetOpenFileNameW(&dialog))
        {
            return false;
        }

        selectedPath = filePath;
        return true;
    }


    bool RegisterProfileEditorClass(
        HINSTANCE instance);

    void OpenProfileEditor(
        HWND ownerWindow,
        int profileIndex);

    void UpdateProfileSliderLabel();
    LRESULT CALLBACK ProfileEditorProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            const auto createData =
                reinterpret_cast<CREATESTRUCTW*>(
                    lParam);

            HINSTANCE instance =
                reinterpret_cast<HINSTANCE>(
                    createData->hInstance);

            HWND label =
                CreateWindowExW(
                    0,
                    L"STATIC",
                    L"Saturation",
                    WS_CHILD | WS_VISIBLE,
                    24,
                    26,
                    180,
                    24,
                    window,
                    nullptr,
                    instance,
                    nullptr);

            g_profileSlider =
                CreateWindowExW(
                    0,
                    TRACKBAR_CLASSW,
                    nullptr,
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    0,
                    24,
                    64,
                    360,
                    38,
                    window,
                    reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(
                            IDC_PROFILE_SLIDER)),
                    instance,
                    nullptr);

            g_profileValueEdit =
                CreateWindowExW(
                    WS_EX_CLIENTEDGE,
                    L"EDIT",
                    L"150",
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    ES_NUMBER |
                    ES_CENTER |
                    ES_AUTOHSCROLL,
                    150,
                    120,
                    88,
                    36,
                    window,
                    reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(
                            IDC_PROFILE_VALUE_EDIT)),
                    instance,
                    nullptr);

            g_profileValueLabel =
                CreateWindowExW(
                    0,
                    L"STATIC",
                    L"%",
                    WS_CHILD |
                    WS_VISIBLE |
                    SS_LEFT,
                    242,
                    128,
                    24,
                    24,
                    window,
                    reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(
                            IDC_PROFILE_VALUE)),
                    instance,
                    nullptr);

            HWND closeButton =
                CreateWindowExW(
                    0,
                    L"BUTTON",
                    L"Save changes",
                    WS_CHILD |
                    WS_VISIBLE |
                    BS_OWNERDRAW,
                    244,
                    184,
                    140,
                    40,
                    window,
                    reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(
                            IDC_PROFILE_CLOSE)),
                    instance,
                    nullptr);

            if (label == nullptr ||
                g_profileSlider == nullptr ||
                g_profileValueEdit == nullptr ||
                g_profileValueLabel == nullptr ||
                closeButton == nullptr)
            {
                return -1;
            }

            ApplyUiFont(label);
            ApplyUiFont(g_profileValueEdit);
            ApplyUiFont(g_profileValueLabel);
            ApplyUiFont(closeButton);

            ApplyWindowTheme(window);
            ApplyWindowTheme(g_profileSlider);
            ApplyWindowTheme(g_profileValueEdit);
            ApplyWindowTheme(closeButton);
            ApplyRoundedButtonRegion(closeButton);
            SetWindowSubclass(closeButton, ButtonSubclassProcedure, 61, 0);
            SetWindowSubclass(g_profileSlider, SliderSubclassProcedure, 70, 0);

            SendMessageW(
                g_profileValueEdit,
                EM_SETLIMITTEXT,
                3,
                0);

            SendMessageW(
                g_profileSlider,
                TBM_SETRANGE,
                TRUE,
                MAKELPARAM(
                    SATURATION_MIN,
                    300));

            SendMessageW(
                g_profileSlider,
                TBM_SETTICFREQ,
                10,
                0);

            int saturation = 150;

            if (g_editingProfileIndex >= 0 &&
                g_editingProfileIndex <
                static_cast<int>(
                    g_gameProfiles.size()))
            {
                saturation =
                    g_gameProfiles[
                        g_editingProfileIndex]
                    .saturationPercent;
            }

            SendMessageW(
                g_profileSlider,
                TBM_SETPOS,
                TRUE,
                saturation);

            UpdateProfileSliderLabel();

            return 0;
        }


        case WM_ERASEBKGND:
        {
            RECT clientRect{};
            GetClientRect(window, &clientRect);
            FillRect(reinterpret_cast<HDC>(wParam),
                &clientRect, GetWindowBackgroundBrush());
            return 1;
        }

        case WM_CTLCOLORSTATIC:
        {
            HDC deviceContext = reinterpret_cast<HDC>(wParam);
            SetTextColor(deviceContext, g_textColor);
            SetBkColor(deviceContext, g_windowBackgroundColor);
            SetBkMode(deviceContext, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetWindowBackgroundBrush());
        }

        case WM_CTLCOLOREDIT:
        {
            HDC deviceContext = reinterpret_cast<HDC>(wParam);
            SetTextColor(deviceContext, g_textColor);
            SetBkColor(deviceContext, g_panelBackgroundColor);
            return reinterpret_cast<LRESULT>(GetPanelBackgroundBrush());
        }

        case WM_CTLCOLORBTN:
        {
            HDC deviceContext = reinterpret_cast<HDC>(wParam);
            SetTextColor(deviceContext, g_textColor);
            SetBkColor(deviceContext, g_windowBackgroundColor);
            SetBkMode(deviceContext, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetWindowBackgroundBrush());
        }

        case WM_DRAWITEM:
        {
            const auto drawItem = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (drawItem != nullptr &&
                static_cast<int>(drawItem->CtlID) == IDC_PROFILE_CLOSE)
            {
                DrawThemedButton(*drawItem);
                return TRUE;
            }
            break;
        }

        case WM_THEMECHANGED:
        {
            // SetWindowTheme() can synchronously send WM_THEMECHANGED.
            // Reapplying the theme here would recurse until the UI crashes.
            InvalidateRect(window, nullptr, TRUE);
            if (g_profileSlider != nullptr)
            {
                InvalidateRect(g_profileSlider, nullptr, FALSE);
            }
            return 0;
        }

        case WM_HSCROLL:
        {
            const HWND sourceControl =
                reinterpret_cast<HWND>(
                    lParam);

            if (sourceControl ==
                g_profileSlider)
            {
                const int sliderValue =
                    static_cast<int>(
                        SendMessageW(
                            g_profileSlider,
                            TBM_GETPOS,
                            0,
                            0));

                UpdateProfileSliderLabel();
                const int scrollCode = LOWORD(wParam);
                CommitProfileSaturation(sliderValue, scrollCode != TB_THUMBTRACK);
            }

            return 0;
        }

        case WM_COMMAND:
        {
            const int controlId =
                LOWORD(wParam);

            const int notificationCode =
                HIWORD(wParam);

            if (controlId == IDC_PROFILE_VALUE_EDIT &&
                notificationCode == EN_CHANGE &&
                !g_updatingProfileValueControl)
            {
                wchar_t valueText[8] = {};
                GetWindowTextW(
                    g_profileValueEdit,
                    valueText,
                    8);

                if (valueText[0] != L'\0')
                {
                    wchar_t* end = nullptr;
                    const long parsedValue =
                        wcstol(valueText, &end, 10);

                    if (end != valueText &&
                        *end == L'\0' &&
                        parsedValue >= SATURATION_MIN &&
                        parsedValue <= SATURATION_MAX)
                    {
                        CommitProfileSaturation(
                            static_cast<int>(parsedValue));
                    }
                }
                return 0;
            }

            if (controlId ==
                IDC_PROFILE_CLOSE &&
                notificationCode ==
                BN_CLICKED)
            {
                DestroyWindow(window);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
        {
            DestroyWindow(window);
            return 0;
        }

        case WM_DESTROY:
        {
            g_profileEditorWindow = nullptr;
            g_profileSlider = nullptr;
            g_profileValueLabel = nullptr;
            g_profileValueEdit = nullptr;
            g_editingProfileIndex = -1;

            return 0;
        }

        default:
        {
            break;
        }
        }

        return DefWindowProcW(
            window,
            message,
            wParam,
            lParam);
    }
    // Receives messages sent to the main window.
    LRESULT CALLBACK WindowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            const auto createData =
                reinterpret_cast<CREATESTRUCTW*>(
                    lParam);

            HINSTANCE instance =
                reinterpret_cast<HINSTANCE>(
                    createData->hInstance);

            if (!CreateMainControls(
                window,
                instance))
            {
                MessageBoxW(
                    window,
                    L"Could not create the GUI controls.",
                    L"ArcVibrance Error",
                    MB_OK | MB_ICONERROR);

                return -1;
            }
            ApplyWindowTheme(window);
            DragAcceptFiles(window, TRUE);

            if (!LoadGameProfiles())
            {
                MessageBoxW(
                    window,
                    L"Could not load the saved game profiles.",
                    L"ArcVibrance",
                    MB_OK | MB_ICONWARNING);
            }

            for (const GameProfile& profile :
                g_gameProfiles)
            {
                LoadExecutableIcon(
                    profile.executablePath);

                SendMessageW(
                    g_gameList,
                    LB_ADDSTRING,
                    0,
                    reinterpret_cast<LPARAM>(
                        profile.executableName.c_str()));
            }
            UpdateProfileEmptyState();
            if (!g_agentClient.EnsureRunning())
            {
                MessageBoxW(
                    window,
                    L"Could not start ArcVibrance.Agent.exe. Make sure it is next to ArcVibrance.exe.",
                    L"ArcVibrance Error",
                    MB_OK | MB_ICONERROR);
            }
            else
            {
                g_agentClient.GetStatus(g_agentStatus);
            }

            if (SetTimer(
                window,
                GAME_MONITOR_TIMER_ID,
                GAME_MONITOR_INTERVAL_MS,
                nullptr) == 0)
            {
                MessageBoxW(
                    window,
                    L"Could not start the foreground recovery timer.",
                    L"ArcVibrance Error",
                    MB_OK | MB_ICONERROR);

                return -1;
            }


            return 0;
        }

        case WM_ERASEBKGND:
        {
            RECT clientRect{};

            GetClientRect(
                window,
                &clientRect);

            DrawNeonBackground(reinterpret_cast<HDC>(wParam), clientRect, true);
            HPEN separator = CreatePen(PS_SOLID, 1, IsDarkThemeActive()
                ? RGB(23, 35, 58) : g_borderColor);
            HGDIOBJ oldPen = SelectObject(reinterpret_cast<HDC>(wParam), separator);
            MoveToEx(reinterpret_cast<HDC>(wParam), 219, 0, nullptr);
            LineTo(reinterpret_cast<HDC>(wParam), 219, clientRect.bottom);
            SelectObject(reinterpret_cast<HDC>(wParam), oldPen);
            DeleteObject(separator);

            return 1;
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC deviceContext =
                reinterpret_cast<HDC>(wParam);

            SetTextColor(
                deviceContext,
                g_textColor);

            SetBkColor(
                deviceContext,
                g_windowBackgroundColor);
            SetBkMode(deviceContext, TRANSPARENT);

            return reinterpret_cast<LRESULT>(
                GetWindowBackgroundBrush());
        }
        case WM_CTLCOLORLISTBOX:
        {
            HDC deviceContext = reinterpret_cast<HDC>(wParam);
            const HWND control = reinterpret_cast<HWND>(lParam);
            SetTextColor(deviceContext, g_textColor);
            if (control == g_gameList)
            {
                SetBkColor(deviceContext, g_windowBackgroundColor);
                return reinterpret_cast<LRESULT>(GetWindowBackgroundBrush());
            }
            SetBkColor(deviceContext, g_panelBackgroundColor);
            return reinterpret_cast<LRESULT>(GetPanelBackgroundBrush());
        }
        case WM_MEASUREITEM:
        {
            auto measureItem = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            if (measureItem != nullptr && measureItem->CtlID == IDC_THEME_COMBO)
            {
                measureItem->itemHeight = 28;
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM:
        {
            const auto drawItem =
                reinterpret_cast<DRAWITEMSTRUCT*>(
                    lParam);

            if (drawItem == nullptr)
            {
                break;
            }

            const int controlId =
                static_cast<int>(
                    drawItem->CtlID);

            if (controlId == IDC_BRAND_LOGO)
            {
                DrawBrandLogo(*drawItem);
                return TRUE;
            }
            if (controlId == IDC_BRAND_NAME)
            {
                DrawBrandName(*drawItem);
                return TRUE;
            }

            if (controlId == IDC_STATUS_DOT)
            {
                DrawStatusDot(*drawItem);
                return TRUE;
            }

            if (controlId == IDC_GAME_LIST)
            {
                DrawGameProfileItem(*drawItem);
                return TRUE;
            }

            if (controlId == IDC_AUTOSTART_CHECKBOX)
            {
                DrawChoiceControl(*drawItem, false);
                return TRUE;
            }

            if (controlId == IDC_CLOSE_MINIMIZE_RADIO ||
                controlId == IDC_CLOSE_EXIT_RADIO)
            {
                DrawChoiceControl(*drawItem, true);
                return TRUE;
            }

            if (controlId == IDC_THEME_COMBO)
            {
                DrawThemeComboItem(*drawItem);
                return TRUE;
            }

            if (controlId == IDC_ADD_GAME_BUTTON ||
                controlId == IDC_EDIT_GAME_BUTTON ||
                controlId == IDC_REMOVE_GAME_BUTTON ||
                controlId == IDC_RELOAD_AGENT_BUTTON ||
                controlId == IDC_PROFILE_CANCEL ||
                controlId == IDC_PROFILE_CLOSE ||
                controlId == IDC_NAV_PROFILES ||
                controlId == IDC_NAV_SETTINGS ||
                controlId == IDC_NAV_ABOUT ||
                controlId == IDC_TOGGLE_GAME_BUTTON)
            {
                DrawThemedButton(*drawItem);
                return TRUE;
            }

            break;
        }
        case WM_CTLCOLOREDIT:
        {
            HDC deviceContext = reinterpret_cast<HDC>(wParam);
            SetTextColor(deviceContext, g_textColor);
            SetBkColor(deviceContext, g_panelBackgroundColor);
            return reinterpret_cast<LRESULT>(GetPanelBackgroundBrush());
        }

        case WM_CTLCOLORBTN:
        {
            HDC deviceContext =
                reinterpret_cast<HDC>(wParam);

            SetTextColor(
                deviceContext,
                g_textColor);

            SetBkColor(
                deviceContext,
                g_windowBackgroundColor);

            SetBkMode(
                deviceContext,
                TRANSPARENT);

            return reinterpret_cast<LRESULT>(
                GetWindowBackgroundBrush());
        }
        case WM_HSCROLL:
        {
            const HWND sourceControl = reinterpret_cast<HWND>(lParam);
            if (sourceControl == g_profileSlider && g_editingProfileIndex >= 0)
            {
                const int sliderValue = static_cast<int>(SendMessageW(g_profileSlider, TBM_GETPOS, 0, 0));
                UpdateProfileSliderLabel();
                const int scrollCode = LOWORD(wParam);
                CommitProfileSaturation(sliderValue, scrollCode != TB_THUMBTRACK);
                return 0;
            }
            break;
        }

        case WM_COMMAND:
        {
            const int controlId =
                LOWORD(wParam);

            const int notificationCode =
                HIWORD(wParam);

            if (notificationCode == BN_CLICKED && controlId == IDC_NAV_PROFILES)
            {
                ShowMainPage(MainPage::Profiles);
                return 0;
            }
            if (notificationCode == BN_CLICKED && controlId == IDC_NAV_SETTINGS)
            {
                ShowMainPage(MainPage::Settings);
                return 0;
            }
            if (notificationCode == BN_CLICKED && controlId == IDC_NAV_ABOUT)
            {
                ShowMainPage(MainPage::About);
                return 0;
            }

            if (controlId == IDC_PROFILE_VALUE_EDIT && notificationCode == EN_CHANGE &&
                !g_updatingProfileValueControl && g_editingProfileIndex >= 0)
            {
                wchar_t valueText[8] = {};
                GetWindowTextW(g_profileValueEdit, valueText, 8);
                if (valueText[0] != L'\0')
                {
                    wchar_t* end = nullptr;
                    const long parsedValue = wcstol(valueText, &end, 10);
                    if (end != valueText && *end == L'\0' &&
                        parsedValue >= SATURATION_MIN && parsedValue <= SATURATION_MAX)
                        CommitProfileSaturation(static_cast<int>(parsedValue));
                }
                return 0;
            }
            if (controlId == IDC_PROFILE_CLOSE && notificationCode == BN_CLICKED &&
                g_editingProfileIndex >= 0)
            {
                if (!SaveGameProfiles())
                    MessageBoxW(window, L"The profile changed, but it could not be saved.",
                        L"ArcVibrance", MB_OK | MB_ICONWARNING);
                else
                {
                    g_originalProfileSaturation = g_gameProfiles[g_editingProfileIndex].saturationPercent;
                    g_agentClient.ReloadProfiles(&g_agentStatus);
                }
                InvalidateRect(g_gameList, nullptr, FALSE);
                return 0;
            }
            if (controlId == IDC_PROFILE_CANCEL && notificationCode == BN_CLICKED)
            {
                if (g_editingProfileIndex >= 0 && g_editingProfileIndex < static_cast<int>(g_gameProfiles.size()))
                {
                    g_gameProfiles[g_editingProfileIndex].saturationPercent = g_originalProfileSaturation;
                    InvalidateRect(g_gameList, nullptr, FALSE);
                }
                g_editingProfileIndex = -1;
                SetWindowTextW(g_profileNameLabel, L"Select a profile");
                SetWindowTextW(g_profilePathLabel,
                    L"Choose a game from the list to edit it here.");
                SendMessageW(g_profileSlider, TBM_SETPOS, TRUE, 100);
                SetNumericSpinnerValue(g_profileValueEdit, 100, false);
                if (g_toggleGameButton != nullptr)
                {
                    SendMessageW(g_toggleGameButton, BM_SETCHECK, BST_UNCHECKED, 0);
                    InvalidateRect(g_toggleGameButton, nullptr, FALSE);
                }
                InvalidateRect(g_profileEditorWindow, nullptr, FALSE);
                return 0;
            }

            if (controlId == IDC_THEME_COMBO && notificationCode == CBN_SELCHANGE)
            {
                const LRESULT selection = SendMessageW(g_themeCombo, CB_GETCURSEL, 0, 0);
                if (selection != CB_ERR)
                {
                    const ThemeMode mode = static_cast<ThemeMode>(selection);
                    if (SetThemeMode(mode))
                    {
                        ApplyThemeMode(mode);
                        ApplyWindowTheme(window);
                        if (g_themeCombo != nullptr)
                        {
                            SetWindowTheme(g_themeCombo,
                                IsDarkThemeActive() ? L"DarkMode_CFD" : L"CFD", nullptr);
                            InvalidateRect(g_themeCombo, nullptr, TRUE);
                        }
                        if (g_gameList != nullptr)
                            SetWindowTheme(g_gameList,
                                IsDarkThemeActive() ? L"DarkMode_Explorer" : L"Explorer", nullptr);
                        if (g_profilesPageHost != nullptr)
                            RedrawWindow(g_profilesPageHost, nullptr, nullptr,
                                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                        if (g_settingsPageHost != nullptr)
                            RedrawWindow(g_settingsPageHost, nullptr, nullptr,
                                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                        if (g_aboutPageHost != nullptr)
                            RedrawWindow(g_aboutPageHost, nullptr, nullptr,
                                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                        if (g_statusGroup != nullptr)
                            RedrawWindow(g_statusGroup, nullptr, nullptr,
                                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                        if (g_profileEditorWindow != nullptr && IsWindow(g_profileEditorWindow))
                        {
                            ApplyWindowTheme(g_profileEditorWindow);
                            if (g_profileSlider != nullptr)
                            {
                                ApplyWindowTheme(g_profileSlider);
                            }
                            if (g_profileValueEdit != nullptr)
                            {
                                InvalidateRect(g_profileValueEdit, nullptr, FALSE);
                            }
                            RedrawWindow(g_profileEditorWindow, nullptr, nullptr,
                                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
                        }
                    }
                    else
                    {
                        SendMessageW(g_themeCombo, CB_SETCURSEL,
                            static_cast<WPARAM>(GetThemeMode()), 0);
                    }
                }
                return 0;
            }

            if (controlId == IDC_RELOAD_AGENT_BUTTON &&
                notificationCode == BN_CLICKED)
            {
                AgentStatus refreshedStatus{};
                if (!g_agentClient.ReloadProfiles(&refreshedStatus))
                {
                    MessageBoxW(
                        window,
                        L"The background agent could not reload the profiles.",
                        L"ArcVibrance",
                        MB_OK | MB_ICONWARNING);
                }
                else
                {
                    g_agentStatus = refreshedStatus;
                    InvalidateRect(g_statusDot, nullptr, TRUE);
                    SetWindowTextW(g_statusGameLabel,
                        g_agentStatus.gameActive
                            ? g_agentStatus.activeExecutableName
                            : L"No configured game active");
                    SetWindowTextW(g_statusSaturationLabel,
                        (L"Applied saturation: " +
                         std::to_wstring(g_agentStatus.appliedSaturationPercent) + L"%").c_str());
                }
                return 0;
            }

            if (controlId == IDC_AUTOSTART_CHECKBOX &&
                notificationCode == BN_CLICKED)
            {
                const bool wasEnabled = SendMessageW(
                    g_autoStartCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
                const bool enabled = !wasEnabled;
                SendMessageW(g_autoStartCheckbox, BM_SETCHECK,
                    enabled ? BST_CHECKED : BST_UNCHECKED, 0);
                InvalidateRect(g_autoStartCheckbox, nullptr, FALSE);

                if (!SetAutoStartEnabled(enabled))
                {
                    MessageBoxW(
                        window,
                        L"Could not update the Windows startup setting.",
                        L"ArcVibrance Error",
                        MB_OK | MB_ICONERROR);

                    SendMessageW(
                        g_autoStartCheckbox,
                        BM_SETCHECK,
                        enabled ? BST_UNCHECKED : BST_CHECKED,
                        0);
                    InvalidateRect(g_autoStartCheckbox, nullptr, FALSE);
                }

                return 0;
            }

            if ((controlId == IDC_CLOSE_MINIMIZE_RADIO ||
                 controlId == IDC_CLOSE_EXIT_RADIO) &&
                notificationCode == BN_CLICKED)
            {
                const CloseBehavior behavior =
                    controlId == IDC_CLOSE_EXIT_RADIO
                        ? CloseBehavior::ExitCompletely
                        : CloseBehavior::MinimizeToTray;

                SendMessageW(g_closeMinimizeRadio, BM_SETCHECK,
                    behavior == CloseBehavior::MinimizeToTray ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessageW(g_closeExitRadio, BM_SETCHECK,
                    behavior == CloseBehavior::ExitCompletely ? BST_CHECKED : BST_UNCHECKED, 0);
                InvalidateRect(g_closeMinimizeRadio, nullptr, FALSE);
                InvalidateRect(g_closeExitRadio, nullptr, FALSE);

                if (!SetCloseBehavior(behavior))
                {
                    MessageBoxW(
                        window,
                        L"Could not save the close behavior setting.",
                        L"ArcVibrance Error",
                        MB_OK | MB_ICONERROR);

                    const CloseBehavior savedBehavior = GetCloseBehavior();
                    SendMessageW(g_closeMinimizeRadio, BM_SETCHECK,
                        savedBehavior == CloseBehavior::MinimizeToTray ? BST_CHECKED : BST_UNCHECKED, 0);
                    SendMessageW(g_closeExitRadio, BM_SETCHECK,
                        savedBehavior == CloseBehavior::ExitCompletely ? BST_CHECKED : BST_UNCHECKED, 0);
                    InvalidateRect(g_closeMinimizeRadio, nullptr, FALSE);
                    InvalidateRect(g_closeExitRadio, nullptr, FALSE);
                }

                return 0;
            }

            // Selecting or double-clicking a game loads the inline editor.
            if (controlId == IDC_GAME_LIST &&
                (notificationCode == LBN_SELCHANGE || notificationCode == LBN_DBLCLK))
            {
                const LRESULT selection =
                    SendMessageW(
                        g_gameList,
                        LB_GETCURSEL,
                        0,
                        0);

                if (selection != LB_ERR)
                {
                    OpenProfileEditor(
                        window,
                        static_cast<int>(selection));
                }

                return 0;
            }

            if (controlId == IDC_EDIT_GAME_BUTTON &&
                notificationCode == BN_CLICKED)
            {
                const LRESULT selection =
                    SendMessageW(
                        g_gameList,
                        LB_GETCURSEL,
                        0,
                        0);

                if (selection == LB_ERR)
                {
                    MessageBoxW(
                        window,
                        L"Select a game profile first.",
                        L"ArcVibrance",
                        MB_OK | MB_ICONINFORMATION);

                    return 0;
                }

                OpenProfileEditor(
                    window,
                    static_cast<int>(selection));

                return 0;
            }

            // Add game button
            if (controlId == IDC_ADD_GAME_BUTTON &&
                notificationCode == BN_CLICKED)
            {
                // existing Add Game code
                std::wstring selectedPath;

                if (SelectGameExecutable(
                    window,
                    selectedPath))
                {
                    AddGameProfile(
                        selectedPath);
                }

                return 0;
            }

            if (controlId == IDC_TOGGLE_GAME_BUTTON &&
                notificationCode == BN_CLICKED)
            {
                const LRESULT selection = SendMessageW(g_gameList, LB_GETCURSEL, 0, 0);
                if (selection == LB_ERR)
                {
                    MessageBoxW(window, L"Select a game profile first.", L"ArcVibrance",
                        MB_OK | MB_ICONINFORMATION);
                    return 0;
                }

                const std::size_t profileIndex = static_cast<std::size_t>(selection);
                const bool enabled = !g_gameProfiles[profileIndex].enabled;
                SetProfileEnabledState(profileIndex, enabled);
                if (g_toggleGameButton != nullptr)
                {
                    SendMessageW(g_toggleGameButton, BM_SETCHECK,
                        enabled ? BST_CHECKED : BST_UNCHECKED, 0);
                    InvalidateRect(g_toggleGameButton, nullptr, FALSE);
                }
                return 0;
            }

            // Remove game button
            if (controlId == IDC_REMOVE_GAME_BUTTON &&
                notificationCode == BN_CLICKED)
            {
                // existing Remove Game code
                if (controlId == IDC_REMOVE_GAME_BUTTON &&
                    notificationCode == BN_CLICKED)
                {
                    const LRESULT selection =
                        SendMessageW(
                            g_gameList,
                            LB_GETCURSEL,
                            0,
                            0);

                    if (selection == LB_ERR)
                    {
                        MessageBoxW(
                            window,
                            L"Select a game profile first.",
                            L"ArcVibrance",
                            MB_OK | MB_ICONINFORMATION);

                        return 0;
                    }

                    const int profileIndex =
                        static_cast<int>(selection);

                    InvalidateRect(g_statusDot, nullptr, TRUE);

                    if (g_editingProfileIndex ==
                        profileIndex &&
                        g_profileEditorWindow != nullptr)
                    {
                        ShowWindow(g_profileEditorWindow, SW_HIDE);
                        g_editingProfileIndex = -1;
                    }
                    RemoveExecutableIcon(
                        g_gameProfiles[
                            profileIndex]
                        .executablePath);
                    g_gameProfiles.erase(
                        g_gameProfiles.begin() +
                        profileIndex);

                    SendMessageW(
                        g_gameList,
                        LB_DELETESTRING,
                        selection,
                        0);
                    UpdateProfileEmptyState();
                    if (!SaveGameProfiles())
                    {
                        MessageBoxW(
                            window,
                            L"The profile was removed, but the change could not be saved.",
                            L"ArcVibrance",
                            MB_OK | MB_ICONWARNING);
                    }
                    else
                    {
                        g_agentClient.ReloadProfiles(&g_agentStatus);
                    }
                    return 0;
                }
            }

            break;
        }
        case WM_DROPFILES:
        {
            HDROP drop = reinterpret_cast<HDROP>(wParam);
            const UINT fileCount = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            bool foundExecutable = false;
            for (UINT index = 0; index < fileCount; ++index)
            {
                const UINT length = DragQueryFileW(drop, index, nullptr, 0);
                std::wstring path(length + 1, L'\0');
                DragQueryFileW(drop, index, path.data(), length + 1);
                path.resize(length);
                if (IsExecutableFile(path))
                {
                    foundExecutable = true;
                    AddGameProfile(path);
                }
            }
            DragFinish(drop);
            if (!foundExecutable)
                MessageBoxW(window, L"Only Windows executable (.exe) files can be added.",
                    L"ArcVibrance", MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        case WM_TIMER:
        {
            if (wParam != GAME_MONITOR_TIMER_ID)
            {
                break;
            }

            const AgentStatus previousStatus = g_agentStatus;
            AgentStatus refreshedStatus{};
            if (!g_agentClient.GetStatus(refreshedStatus))
            {
                g_agentStatus = {};
                SetWindowTextW(window, L"ArcVibrance - Agent unavailable");
                SetWindowTextW(g_statusGameLabel, L"Background agent unavailable");
                SetWindowTextW(g_statusSaturationLabel, L"Saturation status unavailable");
                InvalidateRect(g_statusDot, nullptr, TRUE);
                return 0;
            }

            g_agentStatus = refreshedStatus;
            const bool stateChanged =
                previousStatus.gameActive != g_agentStatus.gameActive ||
                previousStatus.appliedSaturationPercent != g_agentStatus.appliedSaturationPercent ||
                wcscmp(previousStatus.activeExecutableName, g_agentStatus.activeExecutableName) != 0;

            if (!stateChanged)
            {
                return 0;
            }

            InvalidateRect(g_statusDot, nullptr, TRUE);

            if (g_agentStatus.gameActive)
            {
                const std::wstring executableName = g_agentStatus.activeExecutableName;
                SetWindowTextW(window, (L"ArcVibrance - " + executableName + L" active").c_str());
                SetWindowTextW(g_statusGameLabel, (L"Active profile: " + executableName).c_str());
                SetWindowTextW(g_statusSaturationLabel,
                    (L"Applied saturation: " +
                     std::to_wstring(g_agentStatus.appliedSaturationPercent) + L"%").c_str());
            }
            else
            {
                SetWindowTextW(window, L"ArcVibrance");
                SetWindowTextW(g_statusGameLabel, L"No configured game active");
                SetWindowTextW(g_statusSaturationLabel, L"Desktop saturation: 100%");

            }

            return 0;
        }

        case WM_SETTINGCHANGE:
        {
            if (GetThemeMode() == ThemeMode::System)
            {
                ApplyThemeMode(ThemeMode::System);
                ApplyWindowTheme(window);
                if (g_themeCombo != nullptr)
                    SetWindowTheme(g_themeCombo,
                        IsDarkThemeActive() ? L"DarkMode_CFD" : L"CFD", nullptr);
                if (g_profilesPageHost != nullptr)
                    RedrawWindow(g_profilesPageHost, nullptr, nullptr,
                        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                if (g_settingsPageHost != nullptr)
                    RedrawWindow(g_settingsPageHost, nullptr, nullptr,
                        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                if (g_aboutPageHost != nullptr)
                    RedrawWindow(g_aboutPageHost, nullptr, nullptr,
                        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                if (g_gameList != nullptr)
                    SetWindowTheme(g_gameList,
                        IsDarkThemeActive() ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            }
            break;
        }

        case WM_DPICHANGED:
        {
            const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
            if (suggested != nullptr)
            {
                SetWindowPos(window, nullptr, suggested->left, suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        }

        case WM_SIZE:
        {
            if (wParam == SIZE_MINIMIZED)
            {
                ShowWindow(window, SW_HIDE);
                return 0;
            }

            break;
        }

        case WM_ARCVIBRANCE_FORCE_CLOSE_UI:
        {
            DestroyWindow(window);
            return 0;
        }

        case WM_CLOSE:
        {
            if (GetCloseBehavior() == CloseBehavior::MinimizeToTray)
            {
                ShowWindow(window, SW_HIDE);
                return 0;
            }

            // The agent owns IGCL and restores the desktop during shutdown.
            // Destroy the UI even if IPC fails so the close action remains deterministic.
            g_agentClient.ShutdownAgent();
            DestroyWindow(window);
            return 0;
        }

        case WM_DESTROY:
        {
            KillTimer(
                window,
                GAME_MONITOR_TIMER_ID);

            if (g_profileEditorWindow != nullptr &&
                IsWindow(g_profileEditorWindow))
            {
                ShowWindow(g_profileEditorWindow, SW_HIDE);
                g_editingProfileIndex = -1;
            }

            DragAcceptFiles(window, FALSE);
            if (g_pageTitleFont != nullptr)
            {
                DeleteObject(g_pageTitleFont);
                g_pageTitleFont = nullptr;
            }
            if (g_brandFont != nullptr)
            {
                DeleteObject(g_brandFont);
                g_brandFont = nullptr;
            }
            DestroyExecutableIconCache();
            g_friendlyNameCache.clear();
            PostQuitMessage(0);
            return 0;
        }

        default:
        {
            return DefWindowProcW(
                window,
                message,
                wParam,
                lParam);
        }
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
}
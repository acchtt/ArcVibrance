#include "GameMonitor.h"

#include <Windows.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

namespace
{
    std::wstring ToLower(
        std::wstring value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const wchar_t character)
            {
                return static_cast<wchar_t>(
                    std::towlower(character));
            });

        return value;
    }
}

std::wstring GameMonitor::GetForegroundExecutablePath()
{
    const HWND foregroundWindow =
        GetForegroundWindow();

    if (foregroundWindow == nullptr)
    {
        return L"";
    }

    DWORD processId = 0;

    GetWindowThreadProcessId(
        foregroundWindow,
        &processId);

    if (processId == 0)
    {
        return L"";
    }

    const HANDLE processHandle =
        OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            processId);

    if (processHandle == nullptr)
    {
        return L"";
    }

    std::vector<wchar_t> pathBuffer(32768);

    DWORD pathLength =
        static_cast<DWORD>(
            pathBuffer.size());

    const BOOL success =
        QueryFullProcessImageNameW(
            processHandle,
            0,
            pathBuffer.data(),
            &pathLength);

    CloseHandle(processHandle);

    if (!success)
    {
        return L"";
    }

    return std::wstring(
        pathBuffer.data(),
        pathLength);
}

std::wstring GameMonitor::GetExecutableName(
    const std::wstring& executablePath)
{
    const std::size_t slashPosition =
        executablePath.find_last_of(
            L"\\/");

    if (slashPosition == std::wstring::npos)
    {
        return executablePath;
    }

    return executablePath.substr(
        slashPosition + 1);
}

bool GameMonitor::PathsEqual(
    const std::wstring& firstPath,
    const std::wstring& secondPath)
{
    return ToLower(firstPath) ==
           ToLower(secondPath);
}
#pragma once

#include <string>

class GameMonitor
{
public:
    static std::wstring GetForegroundExecutablePath();
    static std::wstring GetExecutableName(
        const std::wstring& executablePath);

    static bool PathsEqual(
        const std::wstring& firstPath,
        const std::wstring& secondPath);
};
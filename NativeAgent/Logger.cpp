#include "Logger.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace ArcVibrance
{
namespace
{
std::mutex g_logMutex;
std::wofstream g_logFile;

std::wstring LevelName(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Info: return L"INFO";
    case LogLevel::Warning: return L"WARN";
    case LogLevel::Error: return L"ERROR";
    }
    return L"UNKNOWN";
}

std::filesystem::path GetLogPath()
{
    wchar_t localAppData[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return {};

    std::filesystem::path directory = std::filesystem::path(localAppData) / L"ArcVibrance" / L"Logs";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) return {};
    return directory / L"ArcVibrance.Agent.log";
}
}

bool InitializeLogging()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile.is_open()) return true;

    const auto path = GetLogPath();
    if (path.empty()) return false;
    g_logFile.open(path, std::ios::app);
    return g_logFile.is_open();
}

void ShutdownLogging()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile.is_open())
    {
        g_logFile.flush();
        g_logFile.close();
    }
}

void Log(LogLevel level, const std::wstring& message)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (!g_logFile.is_open()) return;

    SYSTEMTIME time{};
    GetLocalTime(&time);
    g_logFile << std::setfill(L'0')
              << time.wYear << L'-' << std::setw(2) << time.wMonth << L'-' << std::setw(2) << time.wDay
              << L' ' << std::setw(2) << time.wHour << L':' << std::setw(2) << time.wMinute << L':'
              << std::setw(2) << time.wSecond << L'.' << std::setw(3) << time.wMilliseconds
              << L" [" << LevelName(level) << L"] " << message << L'\n';
    g_logFile.flush();
}

void LogLastError(LogLevel level, const std::wstring& context, unsigned long errorCode)
{
    std::wstringstream stream;
    stream << context << L" (Win32 error " << errorCode << L")";
    Log(level, stream.str());
}
}

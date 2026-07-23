#pragma once

#include <string>

namespace ArcVibrance
{
enum class LogLevel
{
    Info,
    Warning,
    Error
};

bool InitializeLogging();
void ShutdownLogging();
void Log(LogLevel level, const std::wstring& message);
void LogLastError(LogLevel level, const std::wstring& context, unsigned long errorCode);
}

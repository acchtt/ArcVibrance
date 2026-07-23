#include "Settings.h"
#include "AppState.h"
#include "GameMonitor.h"
#include <utility>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <Windows.h>
#include <string>

namespace ArcVibrance
{
bool GetProfilesFilePath(
        std::filesystem::path& profilesPath)
    {
        wchar_t localAppDataPath[MAX_PATH] = {};

        const DWORD length =
            GetEnvironmentVariableW(
                L"LOCALAPPDATA",
                localAppDataPath,
                MAX_PATH);

        if (length == 0 ||
            length >= MAX_PATH)
        {
            return false;
        }

        std::filesystem::path settingsDirectory =
            std::filesystem::path(
                localAppDataPath) /
            L"ArcVibrance";

        std::error_code error;

        std::filesystem::create_directories(
            settingsDirectory,
            error);

        if (error)
        {
            return false;
        }

        profilesPath =
            settingsDirectory /
            L"profiles.txt";

        return true;
    }
bool GetApplicationPath(
    std::wstring& path)
{
    wchar_t buffer[MAX_PATH] = {};

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            buffer,
            MAX_PATH);

    if (length == 0 ||
        length >= MAX_PATH)
    {
        return false;
    }

    path.assign(buffer, length);
    return true;
}

bool IsAutoStartEnabled()
{
    HKEY key = nullptr;

    const LONG openResult =
        RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0,
            KEY_QUERY_VALUE,
            &key);

    if (openResult != ERROR_SUCCESS)
    {
        return false;
    }

    const LONG queryResult =
        RegQueryValueExW(
            key,
            AUTOSTART_VALUE_NAME,
            nullptr,
            nullptr,
            nullptr,
            nullptr);

    RegCloseKey(key);

    return queryResult == ERROR_SUCCESS;
}

bool SetAutoStartEnabled(
    bool enabled)
{
    HKEY key = nullptr;

    const LONG openResult =
        RegCreateKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0,
            nullptr,
            0,
            KEY_SET_VALUE,
            nullptr,
            &key,
            nullptr);

    if (openResult != ERROR_SUCCESS)
    {
        return false;
    }

    LONG result = ERROR_SUCCESS;

    if (enabled)
    {
        std::wstring applicationPath;

        if (!GetApplicationPath(
            applicationPath))
        {
            RegCloseKey(key);
            return false;
        }

        std::filesystem::path agentPath(applicationPath);
        agentPath.replace_filename(L"ArcVibrance.Agent.exe");
        const std::wstring command =
            L"\"" + agentPath.wstring() + L"\"";

        result =
            RegSetValueExW(
                key,
                AUTOSTART_VALUE_NAME,
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(
                    command.c_str()),
                static_cast<DWORD>(
                    (command.size() + 1) *
                    sizeof(wchar_t)));
    }
    else
    {
        result =
            RegDeleteValueW(
                key,
                AUTOSTART_VALUE_NAME);

        if (result == ERROR_FILE_NOT_FOUND)
        {
            result = ERROR_SUCCESS;
        }
    }

    RegCloseKey(key);

    return result == ERROR_SUCCESS;
}
namespace
{
constexpr wchar_t CLOSE_BEHAVIOR_KEY[] = L"Software\\ArcVibrance";
constexpr wchar_t CLOSE_BEHAVIOR_VALUE[] = L"CloseBehavior";
constexpr wchar_t THEME_MODE_VALUE[] = L"ThemeMode";
}


ThemeMode GetThemeMode()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, CLOSE_BEHAVIOR_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
    {
        return ThemeMode::System;
    }

    DWORD value = static_cast<DWORD>(ThemeMode::System);
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LONG result = RegQueryValueExW(key, THEME_MODE_VALUE, nullptr, &type,
        reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(key);

    if (result != ERROR_SUCCESS || type != REG_DWORD || size != sizeof(value) ||
        value > static_cast<DWORD>(ThemeMode::Dark))
    {
        return ThemeMode::System;
    }
    return static_cast<ThemeMode>(value);
}

bool SetThemeMode(ThemeMode mode)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, CLOSE_BEHAVIOR_KEY, 0, nullptr, 0,
        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
    {
        return false;
    }
    const DWORD value = static_cast<DWORD>(mode);
    const LONG result = RegSetValueExW(key, THEME_MODE_VALUE, 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

CloseBehavior GetCloseBehavior()
{
    HKEY key = nullptr;
    const LONG openResult = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        CLOSE_BEHAVIOR_KEY,
        0,
        KEY_QUERY_VALUE,
        &key);

    if (openResult != ERROR_SUCCESS)
    {
        return CloseBehavior::MinimizeToTray;
    }

    DWORD value = static_cast<DWORD>(CloseBehavior::MinimizeToTray);
    DWORD valueSize = sizeof(value);
    DWORD valueType = 0;

    const LONG queryResult = RegQueryValueExW(
        key,
        CLOSE_BEHAVIOR_VALUE,
        nullptr,
        &valueType,
        reinterpret_cast<BYTE*>(&value),
        &valueSize);

    RegCloseKey(key);

    if (queryResult != ERROR_SUCCESS ||
        valueType != REG_DWORD ||
        valueSize != sizeof(value) ||
        value > static_cast<DWORD>(CloseBehavior::ExitCompletely))
    {
        return CloseBehavior::MinimizeToTray;
    }

    return static_cast<CloseBehavior>(value);
}

bool SetCloseBehavior(CloseBehavior behavior)
{
    HKEY key = nullptr;
    const LONG createResult = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        CLOSE_BEHAVIOR_KEY,
        0,
        nullptr,
        0,
        KEY_SET_VALUE,
        nullptr,
        &key,
        nullptr);

    if (createResult != ERROR_SUCCESS)
    {
        return false;
    }

    const DWORD value = static_cast<DWORD>(behavior);
    const LONG setResult = RegSetValueExW(
        key,
        CLOSE_BEHAVIOR_VALUE,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&value),
        sizeof(value));

    RegCloseKey(key);
    return setResult == ERROR_SUCCESS;
}

bool SaveGameProfiles(const std::vector<GameProfile>& profiles)
{
    std::filesystem::path profilesPath;

    if (!GetProfilesFilePath(
        profilesPath))
    {
        return false;
    }

    const std::filesystem::path temporaryPath = profilesPath.wstring() + L".tmp";

    {
        std::wofstream outputFile(temporaryPath, std::ios::trunc);
        if (!outputFile.is_open())
        {
            return false;
        }

        outputFile << L"ArcVibranceProfiles 2\n";
        for (const GameProfile& profile : profiles)
        {
            outputFile << std::quoted(profile.executablePath)
                       << L' ' << profile.saturationPercent
                       << L' ' << (profile.enabled ? 1 : 0) << L'\n';
        }

        outputFile.flush();
        if (!outputFile.good())
        {
            outputFile.close();
            std::error_code removeError;
            std::filesystem::remove(temporaryPath, removeError);
            return false;
        }
    }

    // Replace only after a complete write so a crash cannot leave a truncated profile file.
    if (!MoveFileExW(temporaryPath.c_str(), profilesPath.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        std::error_code removeError;
        std::filesystem::remove(temporaryPath, removeError);
        return false;
    }

    return true;
}

bool LoadGameProfiles(std::vector<GameProfile>& profiles)
{
    std::filesystem::path profilesPath;

    if (!GetProfilesFilePath(
        profilesPath))
    {
        return false;
    }

    std::wifstream inputFile(
        profilesPath);

    if (!inputFile.is_open())
    {
        // The file does not exist on the first launch.
        return true;
    }

    std::wstring formatName;
    int formatVersion = 0;

    if (!(inputFile >>
        formatName >>
        formatVersion))
    {
        return false;
    }

    if (formatName !=
        L"ArcVibranceProfiles" ||
        (formatVersion != 1 && formatVersion != 2))
    {
        return false;
    }

    std::vector<GameProfile>
        loadedProfiles;

    std::wstring executablePath;
    int saturationPercent = 150;

    int enabledValue = 1;
    while (inputFile >> std::quoted(executablePath) >> saturationPercent)
    {
        enabledValue = 1;
        if (formatVersion >= 2 && !(inputFile >> enabledValue))
        {
            return false;
        }
        if (executablePath.empty())
        {
            continue;
        }

        saturationPercent =
            (saturationPercent <
                SATURATION_MIN)
            ? SATURATION_MIN
            : saturationPercent;

        saturationPercent =
            (saturationPercent >
                SATURATION_MAX)
            ? SATURATION_MAX
            : saturationPercent;

        GameProfile profile;

        profile.executablePath =
            executablePath;

        profile.executableName =
            GameMonitor::GetExecutableName(
                executablePath);

        profile.saturationPercent =
            saturationPercent;

        profile.enabled = enabledValue != 0;

        loadedProfiles.push_back(
            std::move(profile));
    }

    if (!inputFile.eof())
    {
        return false;
    }

    profiles =
        std::move(loadedProfiles);

    return true;
}

bool SaveGameProfiles()
{
    return SaveGameProfiles(g_gameProfiles);
}

bool LoadGameProfiles()
{
    return LoadGameProfiles(g_gameProfiles);
}

}

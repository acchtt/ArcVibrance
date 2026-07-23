#pragma once

#include <vector>
#include "GameProfile.h"

namespace ArcVibrance
{
    enum class ThemeMode
    {
        System = 0,
        Light = 1,
        Dark = 2
    };

    enum class CloseBehavior
    {
        MinimizeToTray = 0,
        ExitCompletely = 1
    };

    bool IsAutoStartEnabled();
    bool SetAutoStartEnabled(bool enabled);

    ThemeMode GetThemeMode();
    bool SetThemeMode(ThemeMode mode);

    CloseBehavior GetCloseBehavior();
    bool SetCloseBehavior(CloseBehavior behavior);

    bool LoadGameProfiles();
    bool SaveGameProfiles();
    bool LoadGameProfiles(std::vector<GameProfile>& profiles);
    bool SaveGameProfiles(const std::vector<GameProfile>& profiles);
}

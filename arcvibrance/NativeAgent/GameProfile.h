#pragma once

#include <string>

struct GameProfile
{
    std::wstring executablePath;
    std::wstring executableName;
    int saturationPercent = 150;
    bool enabled = true;
};
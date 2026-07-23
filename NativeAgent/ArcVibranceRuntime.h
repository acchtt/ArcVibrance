#pragma once

#include <Windows.h>
#include <memory>
#include <string>
#include <vector>

#include "GameProfile.h"
#include "IColorBackend.h"

namespace ArcVibrance
{
struct RuntimeStatus
{
    bool initialized = false;
    bool gameActive = false;
    int activeProfileIndex = -1;
    int appliedSaturationPercent = 100;
    std::wstring activeExecutablePath;
    std::wstring activeExecutableName;
};

class ArcVibranceRuntime
{
public:
    ArcVibranceRuntime();
    explicit ArcVibranceRuntime(std::unique_ptr<IColorBackend> backend);
    ~ArcVibranceRuntime();

    ArcVibranceRuntime(const ArcVibranceRuntime&) = delete;
    ArcVibranceRuntime& operator=(const ArcVibranceRuntime&) = delete;

    void AttachProfiles(std::vector<GameProfile>* profiles);

    bool Initialize();
    void Shutdown();

    bool HandleForegroundWindow(HWND foregroundWindow = nullptr);
    bool ReapplyActiveProfile();
    void OnProfileRemoved(int removedIndex);
    bool RestoreDesktop();

    const RuntimeStatus& GetStatus() const noexcept;
    const IColorBackend* GetBackend() const noexcept;
    bool IsProfileActive(int profileIndex) const noexcept;

private:
    int FindMatchingProfile(const std::wstring& executablePath) const;
    bool ApplyProfile(int profileIndex, const std::wstring& executablePath);

    std::unique_ptr<IColorBackend> backend_;
    std::vector<GameProfile>* profiles_ = nullptr;
    RuntimeStatus status_;
};
}

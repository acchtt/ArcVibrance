#include "IntelColorBackend.h"

namespace ArcVibrance
{
std::wstring_view IntelColorBackend::GetId() const noexcept
{
    return L"intel.igcl";
}

std::wstring_view IntelColorBackend::GetDisplayName() const noexcept
{
    return L"Intel Graphics Control Library";
}

bool IntelColorBackend::Initialize()
{
    if (initialized_)
    {
        return true;
    }

    initialized_ = igcl_.Initialize();
    return initialized_;
}

void IntelColorBackend::Shutdown() noexcept
{
    if (!initialized_)
    {
        return;
    }

    igcl_.Shutdown();
    initialized_ = false;
}

bool IntelColorBackend::IsInitialized() const noexcept
{
    return initialized_;
}

bool IntelColorBackend::SetSaturation(double saturation)
{
    return initialized_ && igcl_.SetSaturation(saturation);
}
}

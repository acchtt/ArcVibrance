#pragma once

#include "IColorBackend.h"
#include "IGCLManager.h"

namespace ArcVibrance
{
class IntelColorBackend final : public IColorBackend
{
public:
    std::wstring_view GetId() const noexcept override;
    std::wstring_view GetDisplayName() const noexcept override;

    bool Initialize() override;
    void Shutdown() noexcept override;
    bool IsInitialized() const noexcept override;
    bool SetSaturation(double saturation) override;

private:
    IGCLManager igcl_;
    bool initialized_ = false;
};
}

#pragma once

#include <string_view>

namespace ArcVibrance
{
// Hardware-neutral color control contract used by ArcVibranceRuntime.
// Implementations own their API lifetime and translate normalized values
// (1.0 = desktop/default saturation) to the vendor-specific API.
class IColorBackend
{
public:
    virtual ~IColorBackend() = default;

    IColorBackend(const IColorBackend&) = delete;
    IColorBackend& operator=(const IColorBackend&) = delete;

    virtual std::wstring_view GetId() const noexcept = 0;
    virtual std::wstring_view GetDisplayName() const noexcept = 0;

    virtual bool Initialize() = 0;
    virtual void Shutdown() noexcept = 0;
    virtual bool IsInitialized() const noexcept = 0;
    virtual bool SetSaturation(double saturation) = 0;

protected:
    IColorBackend() = default;
};
}

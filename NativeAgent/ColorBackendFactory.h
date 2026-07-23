#pragma once

#include <memory>

#include "IColorBackend.h"

namespace ArcVibrance
{
// Central backend selection point. Vendor detection and external plugin
// discovery can be added here without coupling ArcVibranceRuntime to them.
std::unique_ptr<IColorBackend> CreateDefaultColorBackend();
}

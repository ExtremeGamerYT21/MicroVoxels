#pragma once
#include "vulkan_context.hpp"
namespace micro {
// Generic GPU provider: final linear RGB and cached visible world-space positions.
// Position.w == 0 is background; the second image retains one generated frame of LOD history.
struct SurfaceSamples {
    Image &color;
    std::array<Image, 2> &positions;
    uint32_t width, height;
};
} // namespace micro

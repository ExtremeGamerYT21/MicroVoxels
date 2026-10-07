#pragma once
#include "vulkan_context.hpp"
namespace micro {
// Generic GPU provider: final RGB plus cached XYZ or depth and inverse VP.
// XYZ.w == 0 / depth == 1 is background. The second image retains LOD history.
struct SurfaceSamples {
    Image &color;
    std::array<Image, 2> &positions; // cached XYZ or D32 depth; interpretation is explicit
    bool depthOnly;
    uint32_t width, height;
};
} // namespace micro

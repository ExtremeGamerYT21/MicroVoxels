#pragma once
#include "vulkan_context.hpp"
namespace micro {
// GPU surface-provider contract: final linear RGB and valid world-space positions.
// The second position image carries one-frame LOD history. No normals/materials escape.
struct SurfaceSamples {
    Image &color;
    std::array<Image, 2> &positions;
    uint32_t width, height;
};
} // namespace micro

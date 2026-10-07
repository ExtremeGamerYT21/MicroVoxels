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
struct SourceVertex {
    glm::vec4 position, normal, albedo;
};
static_assert(sizeof(SourceVertex) == 48);
// Triangle-specific source stage. Normals/materials are consumed here to produce RGB;
// the final cube renderer still receives only centre/size and colour.
struct TriangleSurfaces {
    Image &ids, &shadow;
    Buffer &vertices;
    uint32_t triangleCount;
    glm::vec3 lower, upper;
};
} // namespace micro

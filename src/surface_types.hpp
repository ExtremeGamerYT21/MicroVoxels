#pragma once
#include <glm/glm.hpp>
namespace micro {
struct alignas(16) Frame {
    glm::mat4 vp, lightVP;
    glm::vec4 cameraTime, light, options;
    glm::mat4 inverseVP, previousInverseVP;
};
struct alignas(16) Parameters {
    glm::ivec4 extent;
    glm::vec4 config;
    glm::ivec4 flags;
    glm::ivec4 footprint; // radius, write limit, depth provider, tile/verify/isolated-timing bits
    glm::ivec4
        optimization; // legacy scan, packed instances, touched-clear/track-list bits, local dedup
};
static_assert(sizeof(Parameters) == 80 && sizeof(Frame) == 304);
} // namespace micro

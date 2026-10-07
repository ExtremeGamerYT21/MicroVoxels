#pragma once
#include <glm/glm.hpp>
namespace micro {
struct alignas(16) Frame {
    glm::mat4 vp, lightVP;
    glm::vec4 cameraTime, light, options;
};
struct alignas(16) Parameters {
    glm::ivec4 extent;
    glm::vec4 config;
    glm::ivec4 flags;
    glm::ivec4 footprint; // radius, write limit, renderer-only cull/index switches
    glm::vec4 cache; // monotonic time, hold seconds (0 disables), previous cell count, hold LOD
};
static_assert(sizeof(Parameters) == 80 && sizeof(Frame) == 176);
} // namespace micro

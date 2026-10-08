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
    glm::vec4 cache;    // monotonic time, hold seconds (0 disables), previous cell count, hold LOD
    glm::vec4 temporal; // current RGB weight (0 disables), trace step budget, source jitter XY
    glm::vec4 history;  // static camera + source pose, reserved
};
static_assert(sizeof(Parameters) == 112 && sizeof(Frame) == 176);
} // namespace micro

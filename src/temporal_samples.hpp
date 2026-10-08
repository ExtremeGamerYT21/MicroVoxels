#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
namespace micro {
inline glm::vec2 sourceJitter(uint32_t frame) {
    // Deterministic, zero-mean eight-phase subpixel cycle. No random samples,
    // camera-grid snapping or motion of cube transforms.
    constexpr std::array<std::array<float, 2>, 8> pattern{{{-.375f, -.25f},
                                                           {.125f, .25f},
                                                           {.375f, -.25f},
                                                           {-.125f, .25f},
                                                           {-.375f, .25f},
                                                           {.125f, -.25f},
                                                           {.375f, .25f},
                                                           {-.125f, -.25f}}};
    return {pattern[frame % 8][0], pattern[frame % 8][1]};
}
inline glm::vec3 temporalColor(glm::vec3 previous, glm::vec3 fresh, float currentWeight) {
    float t = std::clamp((glm::length(fresh - previous) - .10f) / .25f, 0.f, 1.f);
    float alpha = std::max(currentWeight, t * t * (3 - 2 * t));
    return glm::clamp(glm::mix(previous, fresh, alpha), glm::vec3(0), glm::vec3(1));
}
} // namespace micro

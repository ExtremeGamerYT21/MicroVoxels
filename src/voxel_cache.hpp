#pragma once
#include "lattice.hpp"
#include "surface_types.hpp"
#include <glm/glm.hpp>
#include <map>
namespace micro {
// CPU oracle for the GPU history policy. Uses only current source samples.
inline bool cacheSupported(const Cell &key, const glm::vec3 &center, float pitch,
                           const Parameters &p, const Frame &frame, const glm::vec4 *positions,
                           const std::map<std::array<int32_t, 3>, int> &regions) {
    if (p.flags.x > 1) {
        float span = float(1 << (p.flags.x - 1 - key[3]));
        std::array<int32_t, 3> root{int32_t(std::floor(key[0] / span)),
                                    int32_t(std::floor(key[1] / span)),
                                    int32_t(std::floor(key[2] / span))};
        auto found = regions.find(root);
        if (found == regions.end() || found->second != key[3])
            return false;
    }
    glm::vec4 clip = frame.vp * glm::vec4(center, 1);
    if (clip.w <= 0)
        return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (glm::any(glm::lessThan(ndc, glm::vec3(-1, -1, 0))) ||
        glm::any(glm::greaterThan(ndc, glm::vec3(1))))
        return false;
    glm::ivec2 pixel(glm::floor((glm::vec2(ndc) * .5f + .5f) * glm::vec2(p.extent)));
    for (int y = -3; y <= 3; ++y)
        for (int x = -3; x <= 3; ++x) {
            glm::ivec2 q = pixel + glm::ivec2(x, y);
            if (q.x < 0 || q.y < 0 || q.x >= p.extent.x || q.y >= p.extent.y)
                continue;
            const auto &hit = positions[q.y * p.extent.x + q.x];
            if (hit.w != 0 && glm::all(glm::lessThanEqual(glm::abs(glm::vec3(hit) - center),
                                                          glm::vec3(.65f * pitch))))
                return true;
        }
    return false;
}
} // namespace micro

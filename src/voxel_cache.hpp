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
        if ((found == regions.end() && p.temporal.x == 0) ||
            (found != regions.end() && found->second != key[3]))
            return false;
    }
    glm::vec4 clip = frame.vp * glm::vec4(center, 1);
    if (clip.w <= 0)
        return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (p.temporal.x == 0) {
        if (glm::any(glm::lessThan(ndc, glm::vec3(-1, -1, 0))) ||
            glm::any(glm::greaterThan(ndc, glm::vec3(1))))
            return false;
    } else {
        glm::vec3 w(frame.vp[0][3], frame.vp[1][3], frame.vp[2][3]);
        for (int axis = 0; axis < 3; axis++) {
            glm::vec3 row(frame.vp[0][axis], frame.vp[1][axis], frame.vp[2][axis]);
            float lower = axis == 2 ? 0.f : -1.f;
            float loRadius = .5f * pitch * glm::dot(glm::abs(row - lower * w), glm::vec3(1));
            float hiRadius = .5f * pitch * glm::dot(glm::abs(row - w), glm::vec3(1));
            if (clip[axis] < lower * clip.w - loRadius || clip[axis] > clip.w + hiRadius)
                return false;
        }
    }
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
    if (p.temporal.x == 0)
        return false;
    if (p.history.x != 0)
        return true;
    glm::vec3 depthAxis(frame.vp[0][3], frame.vp[1][3], frame.vp[2][3]);
    float nearDepth = clip.w - .8f * pitch * glm::length(depthAxis);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            glm::ivec2 q = pixel + glm::ivec2(x, y);
            if (q.x < 0 || q.y < 0 || q.x >= p.extent.x || q.y >= p.extent.y)
                return true;
            auto hit = positions[q.y * p.extent.x + q.x];
            if (hit.w == 0 || (frame.vp * glm::vec4(glm::vec3(hit), 1)).w >= nearDepth)
                return true;
        }
    return false;
}
} // namespace micro

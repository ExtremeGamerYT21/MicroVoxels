#pragma once
#include "footprint_overlap.hpp"
#include "surface_types.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace micro {
struct SurfaceFootprint {
    glm::vec3 dx{}, dy{};
    uint32_t rejected{};
    bool clamped{};
};
inline float nominalPixelSpan(const Parameters &p, const Frame &f, glm::vec3 hit) {
    float depth = std::abs((f.vp * glm::vec4(hit, 1)).w);
    glm::vec3 x(f.vp[0][0], f.vp[1][0], f.vp[2][0]), y(f.vp[0][1], f.vp[1][1], f.vp[2][1]);
    return std::max(2 * depth / (p.extent.x * glm::length(x)),
                    2 * depth / (p.extent.y * glm::length(y)));
}
inline SurfaceFootprint sourceFootprint(const glm::vec4 *positions, uint32_t id,
                                        const Parameters &p, const Frame &f) {
    SurfaceFootprint result;
    glm::vec3 hit(positions[id]);
    float nominal = nominalPixelSpan(p, f, hit), maximum = std::min(.25f, 8 * nominal);
    float depth = (f.vp * glm::vec4(hit, 1)).w;
    int x = int(id % uint32_t(p.extent.x)), y = int(id / uint32_t(p.extent.x));
    for (int axis = 0; axis < 2; axis++) {
        std::array<glm::vec3, 2> steps{};
        std::array<float, 2> lengths{};
        std::array<bool, 2> valid{};
        for (int i = 0; i < 2; i++) {
            int sign = i == 0 ? -1 : 1;
            int nx = x + (axis == 0 ? sign : 0), ny = y + (axis == 1 ? sign : 0);
            if (nx < 0 || ny < 0 || nx >= p.extent.x || ny >= p.extent.y)
                continue;
            auto neighbor = positions[ny * p.extent.x + nx];
            if (neighbor.w == 0)
                continue;
            glm::vec3 delta = glm::vec3(neighbor) - hit;
            float separation = glm::dot(delta, delta);
            float jump = std::abs((f.vp * glm::vec4(glm::vec3(neighbor), 1)).w - depth);
            if (separation > maximum * maximum ||
                jump > std::max(4 * nominal, .025f * std::abs(depth))) {
                result.rejected++;
                continue;
            }
            steps[i] = delta * float(sign);
            lengths[i] = separation;
            valid[i] = true;
        }
        glm::vec3 step(0);
        if (valid[0] && valid[1]) {
            int near = lengths[0] <= lengths[1] ? 0 : 1, far = 1 - near;
            if (lengths[far] > 4 * lengths[near]) {
                result.rejected++;
                step = steps[near];
            } else if (glm::dot(steps[0], steps[1]) < .5f * std::sqrt(lengths[0] * lengths[1])) {
                result.rejected += 2;
            } else {
                step = steps[near];
            }
        } else {
            step = valid[0] ? steps[0] : (valid[1] ? steps[1] : glm::vec3(0));
        }
        (axis == 0 ? result.dx : result.dy) = step;
    }
    return result;
}
inline SurfaceFootprint boundedFootprint(const glm::vec4 *positions, uint32_t id,
                                         const Parameters &p, const Frame &f, float h) {
    if (f.options.z == 0 || p.footprint.x == 0)
        return {};
    auto result = sourceFootprint(positions, id, p, f);
    auto extent = .5f * (glm::abs(result.dx) + glm::abs(result.dy));
    float longest = std::max({extent.x, extent.y, extent.z}), maximum = p.footprint.x * h;
    if (longest > maximum) {
        result.dx *= maximum / longest;
        result.dy *= maximum / longest;
        result.clamped = true;
    }
    return result;
}
inline bool referenceFootprintBox(glm::vec3 hit, const SurfaceFootprint &f, glm::vec3 center,
                                  float h) {
    glm::vec3 p = (hit - center) / h, x = .5f * f.dx / h, y = .5f * f.dy / h;
    std::array<Point3, 4> polygon{};
    int i = 0;
    for (glm::vec3 v : {p - x - y, p + x - y, p + x + y, p - x + y})
        polygon[i++] = {v.x, v.y, v.z};
    return clippedFootprintBox(polygon);
}
} // namespace micro

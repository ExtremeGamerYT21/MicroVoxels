#pragma once
#include "source_world.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
namespace micro {
struct DistanceNode {
    glm::vec4 low, high;
    glm::uvec4 links; // left, right, first triangle, triangle count (leaves only)
};
static_assert(sizeof(DistanceNode) == 48);
// The bounds contain every source wind pose. Only animated vertices change on
// the GPU; this immutable distance-query accelerator needs no per-frame refit.
struct SourceDistanceBvh {
    std::vector<DistanceNode> nodes;
    std::vector<uint32_t> triangles;
    explicit SourceDistanceBvh(const SourceWorld &world) {
        triangles.resize(world.triangleCount());
        std::iota(triangles.begin(), triangles.end(), 0u);
        std::vector<glm::vec3> lows(triangles.size()), highs(triangles.size()),
            centers(triangles.size());
        for (uint32_t t : triangles) {
            glm::vec3 lo(std::numeric_limits<float>::max()), hi(-lo);
            for (int k = 0; k < 3; k++) {
                const auto &v = world.vertices[t * 3 + k];
                float y = std::max(v.position.y - .5f, 0.f);
                glm::vec3 motion(.13f * std::abs(v.material.x) * y * y, 0,
                                 .055f * std::abs(v.material.x) * y * y);
                lo = glm::min(lo, v.position - motion);
                hi = glm::max(hi, v.position + motion);
            }
            lows[t] = lo - glm::vec3(.00001f);
            highs[t] = hi + glm::vec3(.00001f);
            centers[t] = (lo + hi) * .5f;
        }
        auto build = [&](auto &&self, uint32_t first, uint32_t count) -> uint32_t {
            glm::vec3 lo(std::numeric_limits<float>::max()), hi(-lo);
            for (uint32_t i = first; i < first + count; i++) {
                lo = glm::min(lo, lows[triangles[i]]);
                hi = glm::max(hi, highs[triangles[i]]);
            }
            uint32_t index = uint32_t(nodes.size());
            nodes.push_back({glm::vec4(lo, 0), glm::vec4(hi, 0), {0, 0, first, count}});
            if (count > 4) {
                glm::vec3 span = hi - lo;
                int axis = span.x >= span.y && span.x >= span.z ? 0 : span.y >= span.z ? 1 : 2;
                uint32_t half = count / 2;
                std::nth_element(triangles.begin() + first, triangles.begin() + first + half,
                                 triangles.begin() + first + count, [&](uint32_t a, uint32_t b) {
                                     return centers[a][axis] == centers[b][axis]
                                                ? a < b
                                                : centers[a][axis] < centers[b][axis];
                                 });
                uint32_t left = self(self, first, half),
                         right = self(self, first + half, count - half);
                nodes[index].links = {left, right, 0, 0};
            }
            return index;
        };
        if (!triangles.empty())
            build(build, 0, uint32_t(triangles.size()));
    }
};
} // namespace micro

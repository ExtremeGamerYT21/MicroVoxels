#pragma once
#include "lattice.hpp"
#include "surface_samples.hpp"
#include "triangle_overlap.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <set>
namespace micro {
inline bool referenceOverlap(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 center, float size) {
    std::array<Point3, 3> triangle{};
    int i = 0;
    for (auto vertex : {a, b, c}) {
        // Round the local coordinate transformation to float, then independently clip in double.
        glm::vec3 p = (vertex - center) / size;
        triangle[i++] = {p.x, p.y, p.z};
    }
    return clippedTriangleBox(triangle);
}
inline bool referenceRegionInView(const Parameters &p, const Frame &f, glm::ivec3 root) {
    std::array<bool, 6> outside{true, true, true, true, true, true};
    float h = p.config.x * 32.f;
    for (int i = 0; i < 8; i++) {
        auto corner = (glm::vec3(root) + glm::vec3(i & 1, (i >> 1) & 1, (i >> 2) & 1)) * h;
        glm::vec4 q = f.vp * glm::vec4(corner, 1);
        outside[0] = outside[0] && q.x < -q.w;
        outside[1] = outside[1] && q.x > q.w;
        outside[2] = outside[2] && q.y < -q.w;
        outside[3] = outside[3] && q.y > q.w;
        outside[4] = outside[4] && q.z < 0;
        outside[5] = outside[5] && q.z > q.w;
    }
    return std::none_of(outside.begin(), outside.end(), [](bool b) { return b; });
}
// Find the closest surface point via plane projection plus independent segment comparisons.
inline glm::vec3 referenceBarycentric(glm::vec3 p, glm::vec3 af, glm::vec3 bf, glm::vec3 cf) {
    glm::dvec3 a(af), b(bf), c(cf), point(p), ab = b - a, ac = c - a;
    double aa = glm::dot(ab, ab), cc = glm::dot(ac, ac), bb = glm::dot(ab, ac);
    double d = glm::dot(point - a, ab), e = glm::dot(point - a, ac), denom = aa * cc - bb * bb;
    double v = (cc * d - bb * e) / denom, w = (aa * e - bb * d) / denom;
    glm::dvec3 bary(1 - v - w, v, w);
    if (bary.x >= 0 && bary.y >= 0 && bary.z >= 0)
        return glm::vec3(bary);
    double best = 1e100;
    std::array<glm::dvec3, 3> vertices{a, b, c};
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        auto edge = vertices[j] - vertices[i];
        double t = std::clamp(glm::dot(point - vertices[i], edge) / glm::dot(edge, edge), 0.0, 1.0);
        auto delta = vertices[i] + t * edge - point;
        double dist = glm::dot(delta, delta);
        if (dist < best) {
            best = dist;
            bary = glm::dvec3(0);
            bary[i] = 1 - t;
            bary[j] = t;
        }
    }
    return glm::vec3(bary);
}
inline glm::vec3 referenceShade(const Frame &f, glm::vec3 point, glm::vec3 normal, glm::vec3 albedo,
                                float gloss, const float *shadow, glm::vec3 &lower,
                                glm::vec3 &upper) {
    auto n = glm::normalize(normal), l = glm::normalize(glm::vec3(f.light)),
         v = glm::normalize(glm::vec3(f.cameraTime) - point);
    if (glm::dot(n, v) < 0)
        n = -n;
    float ndl = std::max(glm::dot(n, l), 0.f), visibility = 1, low = 1, high = 1;
    if (f.options.x > 1.5f) {
        glm::vec4 q = f.lightVP * glm::vec4(point, 1);
        glm::vec3 pos = glm::vec3(q) / q.w;
        glm::vec2 uv = glm::vec2(pos) * .5f + .5f;
        if (uv.x >= 0 && uv.x <= 1 && uv.y >= 0 && uv.y <= 1 && pos.z >= 0 && pos.z <= 1) {
            visibility = low = high = 0;
            float depth = pos.z - std::max(.0007f, .002f * (1 - ndl));
            for (int y = -1; y <= 1; y++)
                for (int x = -1; x <= 1; x++) {
                    float px = uv.x * 1024 + x, py = uv.y * 1024 + y;
                    int sx = std::clamp(int(std::floor(px)), 0, 1023),
                        sy = std::clamp(int(std::floor(py)), 0, 1023);
                    if (depth <= shadow[sy * 1024 + sx])
                        visibility += 1.f / 9.f;
                    // A discontinuous depth/texel comparison can change under a few float ULPs.
                    // Bound only those ambiguous comparisons; all other shading stays strict.
                    bool certain = true, possible = false;
                    for (int iy = std::clamp(int(std::floor(py - .001f)), 0, 1023);
                         iy <= std::clamp(int(std::floor(py + .001f)), 0, 1023); iy++)
                        for (int ix = std::clamp(int(std::floor(px - .001f)), 0, 1023);
                             ix <= std::clamp(int(std::floor(px + .001f)), 0, 1023); ix++) {
                            float z = shadow[iy * 1024 + ix];
                            certain = certain && depth + .000002f <= z;
                            possible = possible || depth - .000002f <= z;
                        }
                    if (certain)
                        low += 1.f / 9.f;
                    if (possible)
                        high += 1.f / 9.f;
                }
        }
    }
    auto base = albedo;
    if (gloss < 0) {
        float check = std::fmod(std::floor(point.x * 1.5f) + std::floor(point.z * 1.5f), 2.f);
        if (check < 0)
            check += 2.f;
        base *= .68f + .32f * check;
    }
    auto shade = [&](float vis) {
        if (f.options.x <= .5f)
            return base;
        float spec =
            std::pow(std::max(glm::dot(n, glm::normalize(l + v)), 0.f), std::max(gloss, 8.f)) *
            vis * .75f;
        auto rgb = base * (.22f + 1.4f * ndl * vis) + glm::vec3(1, .93f, .82f) * spec;
        return glm::vec3(1) - glm::exp(-rgb * f.light.w);
    };
    lower = shade(low);
    upper = shade(high);
    return shade(visibility);
}
inline void verifyTriangles(const Parameters &p, const Frame &f, const SourceVertex *vertices,
                            const uint32_t *ids, const uint32_t *roots, const float *shadow,
                            const Voxel *instances, const Counters &counts,
                            std::map<std::array<int32_t, 3>, int> &history) {
    if (counts.vertexCount != 36 || counts.firstVertex || counts.firstInstance || counts.dropped ||
        counts.rootDropped)
        throw std::runtime_error(
            "Triangle verification requires valid indirect data and no capacity drops");
    std::set<uint32_t> visible;
    uint32_t hits = 0;
    for (int i = 0; i < p.extent.x * p.extent.y; i++)
        if (ids[i]) {
            if (ids[i] > uint32_t(f.options.w))
                throw std::runtime_error("Source triangle ID out of range");
            visible.insert(ids[i] - 1);
            hits++;
        }
    if (hits != counts.hits)
        throw std::runtime_error("Triangle source visibility count mismatch");
    std::map<std::array<int32_t, 3>, int> current;
    float rh = p.config.x * 32.f;
    for (int z = 0; z < p.gridExtent.z; z++)
        for (int y = 0; y < p.gridExtent.y; y++)
            for (int x = 0; x < p.gridExtent.x; x++) {
                glm::ivec3 r = glm::ivec3(p.gridMin) + glm::ivec3(x, y, z);
                std::array<int32_t, 3> key{r.x, r.y, r.z};
                auto old = history.find(key);
                int lod = chooseLod(
                    glm::distance((glm::vec3(r) + .5f) * rh, glm::vec3(f.cameraTime)), p.config.y,
                    p.flags.x, old == history.end() ? -1 : old->second, p.config.z);
                uint32_t index = uint32_t((z * p.gridExtent.y + y) * p.gridExtent.x + x);
                if (roots[index * 2] != index + 1 || roots[index * 2 + 1] != uint32_t(lod))
                    throw std::runtime_error(
                        "Triangle world-region LOD differs from CPU reference");
                current.emplace(key, lod);
            }
    struct Expected {
        uint64_t r{}, g{}, b{}, count{};
        float depth = 1e30f;
        uint32_t triangle = UINT32_MAX;
        glm::vec3 closest{}, closestLower{}, closestUpper{};
        glm::dvec3 lower{}, upper{};
    };
    std::map<Cell, Expected> expected;
    uint32_t contributions = 0;
    for (uint32_t tri : visible) {
        const auto &av = vertices[tri * 3], &bv = vertices[tri * 3 + 1],
                   &cv = vertices[tri * 3 + 2];
        glm::vec3 a(av.position), b(bv.position), c(cv.position);
        auto normal = glm::cross(b - a, c - a);
        if (glm::dot(normal, normal) < 1e-20f)
            continue;
        auto lo = glm::min(a, glm::min(b, c)), hi = glm::max(a, glm::max(b, c));
        glm::ivec3 begin =
            glm::max(glm::ivec3(glm::floor(lo / rh - .0001f)), glm::ivec3(p.gridMin));
        glm::ivec3 end = glm::min(glm::ivec3(glm::floor(hi / rh + .0001f)),
                                  glm::ivec3(p.gridMin + p.gridExtent) - 1);
        for (int rz = begin.z; rz <= end.z; rz++)
            for (int ry = begin.y; ry <= end.y; ry++)
                for (int rx = begin.x; rx <= end.x; rx++) {
                    glm::ivec3 root(rx, ry, rz);
                    if (!referenceRegionInView(p, f, root) ||
                        !referenceOverlap(a, b, c, (glm::vec3(root) + .5f) * rh, rh))
                        continue;
                    int lod = current.at({rx, ry, rz}), span = 1 << (5 - lod);
                    float h = std::ldexp(p.config.x, lod);
                    auto first = glm::max(glm::ivec3(glm::floor(lo / h - .0001f)), root * span);
                    auto last =
                        glm::min(glm::ivec3(glm::floor(hi / h + .0001f)), (root + 1) * span - 1);
                    for (int z = first.z; z <= last.z; z++)
                        for (int y = first.y; y <= last.y; y++)
                            for (int x = first.x; x <= last.x; x++) {
                                glm::vec3 center = (glm::vec3(x, y, z) + .5f) * h;
                                if (!referenceOverlap(a, b, c, center, h))
                                    continue;
                                glm::vec3 w = referenceBarycentric(center, a, b, c);
                                glm::vec3 point = w.x * a + w.y * b + w.z * c;
                                glm::vec3 lower, upper;
                                auto rgb = referenceShade(
                                    f, point,
                                    w.x * glm::vec3(av.normal) + w.y * glm::vec3(bv.normal) +
                                        w.z * glm::vec3(cv.normal),
                                    w.x * glm::vec3(av.albedo) + w.y * glm::vec3(bv.albedo) +
                                        w.z * glm::vec3(cv.albedo),
                                    w.x * av.normal.w + w.y * bv.normal.w + w.z * cv.normal.w,
                                    shadow, lower, upper);
                                auto &ref = expected[{x, y, z, lod}];
                                ref.lower += glm::dvec3(lower);
                                ref.upper += glm::dvec3(upper);
                                contributions++;
                                ref.count++;
                                ref.r += uint32_t(std::round(std::clamp(rgb.r, 0.f, 1.f) * 1023));
                                ref.g += uint32_t(std::round(std::clamp(rgb.g, 0.f, 1.f) * 1023));
                                ref.b += uint32_t(std::round(std::clamp(rgb.b, 0.f, 1.f) * 1023));
                                float depth = glm::distance(point, glm::vec3(f.cameraTime));
                                if (depth < ref.depth ||
                                    (depth == ref.depth && tri < ref.triangle)) {
                                    ref.depth = depth;
                                    ref.triangle = tri;
                                    ref.closest = rgb;
                                    ref.closestLower = lower;
                                    ref.closestUpper = upper;
                                }
                            }
                }
    }
    if (expected.size() != counts.instanceCount || contributions != counts.pad)
        throw std::runtime_error(
            "Triangle-box GPU occupancy differs from independent polygon clipping: CPU " +
            std::to_string(expected.size()) + " cells/" + std::to_string(contributions) +
            " contributions, GPU " + std::to_string(counts.instanceCount) + " cells/" +
            std::to_string(counts.pad) + " contributions");
    std::array<uint32_t, 8> lodCounts{};
    for (uint32_t i = 0; i < counts.instanceCount; i++) {
        const auto &v = instances[i];
        int lod = int(std::round(std::log2(v.centerSize.w / (p.config.x * p.config.w))));
        auto key = cell({v.centerSize.x, v.centerSize.y, v.centerSize.z}, p.config.x, lod);
        auto found = expected.find(key);
        if (found == expected.end())
            throw std::runtime_error("Unexpected or duplicate triangle cell");
        auto cen = center(key, p.config.x);
        for (int axis = 0; axis < 3; axis++)
            if (std::abs(v.centerSize[axis] - cen[axis]) > .00005f)
                throw std::runtime_error("Triangle lattice centre mismatch");
        auto &ref = found->second;
        auto wanted =
            p.flags.y ? glm::vec3(ref.r, ref.g, ref.b) / (1023.f * float(ref.count)) : ref.closest;
        auto lower = p.flags.y ? glm::vec3(ref.lower / double(ref.count)) : ref.closestLower;
        auto upper = p.flags.y ? glm::vec3(ref.upper / double(ref.count)) : ref.closestUpper;
        for (int axis = 0; axis < 3; axis++)
            if (std::abs(v.rgba[axis] - wanted[axis]) > .0031f &&
                (v.rgba[axis] < lower[axis] - .0031f || v.rgba[axis] > upper[axis] + .0031f))
                throw std::runtime_error(
                    "Triangle source RGB mismatch at " + std::to_string(key[0]) + "," +
                    std::to_string(key[1]) + "," + std::to_string(key[2]) + " channel " +
                    std::to_string(axis) + ": GPU " + std::to_string(v.rgba[axis]) + ", CPU " +
                    std::to_string(wanted[axis]) + ", rounding interval " +
                    std::to_string(lower[axis]) + ".." + std::to_string(upper[axis]));
        if (v.rgba.w != 1)
            throw std::runtime_error("Unexpected triangle cell opacity");
        lodCounts[lod]++;
        expected.erase(found);
    }
    for (int i = 0; i < 8; i++)
        if (lodCounts[i] != counts.perLod[i])
            throw std::runtime_error("Triangle LOD counter mismatch");
    history = std::move(current);
}
} // namespace micro

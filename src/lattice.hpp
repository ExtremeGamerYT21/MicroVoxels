#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace micro {
using Cell = std::array<int32_t, 4>;
inline Cell cell(std::array<float, 3> p, float base, int lod) {
    if (!(base > 0) || lod < 0 || lod > 7)
        throw std::invalid_argument("Invalid lattice settings");
    float h = std::ldexp(base, lod);
    return {int32_t(std::floor(p[0] / h)), int32_t(std::floor(p[1] / h)),
            int32_t(std::floor(p[2] / h)), lod};
}
inline std::array<float, 3> center(Cell c, float base) {
    float h = std::ldexp(base, c[3]);
    return {(c[0] + 0.5f) * h, (c[1] + 0.5f) * h, (c[2] + 0.5f) * h};
}
inline int chooseLod(float distance, float threshold, int levels, int previous = -1,
                     float hysteresis = .10f) {
    int lod = previous;
    if (lod < 0)
        return std::clamp(int(std::floor(std::log2(std::max(distance / threshold, 1.f)))) +
                              (distance >= threshold ? 1 : 0),
                          0, levels - 1);
    lod = std::clamp(lod, 0, levels - 1);
    while (lod > 0 && distance < std::ldexp(threshold, lod - 1) * (1 - hysteresis))
        --lod;
    while (lod < levels - 1 && distance > std::ldexp(threshold, lod) * (1 + hysteresis))
        ++lod;
    return lod;
}
inline int chooseCoverageLod(float footprint, float base, int levels, int previous = -1,
                             float hysteresis = .10f) {
    int lod = std::clamp(std::max(previous, 0), 0, levels - 1);
    while (lod > 0 && footprint < std::ldexp(base, lod - 1) * (1 - hysteresis))
        --lod;
    while (lod < levels - 1 && footprint > std::ldexp(base, lod))
        ++lod;
    return lod;
}
} // namespace micro

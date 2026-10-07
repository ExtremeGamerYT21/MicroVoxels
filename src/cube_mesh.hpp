#pragma once
#include <array>
#include <cstdint>
namespace micro {
// Corner bits are XYZ. Preserve the legacy triangle order and diagonals for the
// eight-corner, two-sided path, including depth ties between adjoining cells.
inline constexpr std::array<uint16_t, 36> CubeCornerIndices = {
    1, 5, 7, 1, 7, 3, 0, 4, 6, 0, 6, 2,
    2, 3, 7, 2, 7, 6, 0, 1, 5, 0, 5, 4,
    4, 5, 7, 4, 7, 6, 0, 1, 3, 0, 3, 2};
// Four vertices per face allow the near-plane safeguard to reverse an individual
// face's winding. Faces +X, +Y and -Z need the opposite legacy winding.
inline constexpr std::array<uint16_t, 36> CubeFaceIndices = [] {
    std::array<uint16_t, 36> indices{};
    for (int f = 0; f < 6; ++f) {
        bool reverse = f == 0 || f == 2 || f == 5;
        std::array<int, 6> quad = reverse ? std::array<int, 6>{0, 2, 1, 0, 3, 2}
                                        : std::array<int, 6>{0, 1, 2, 0, 2, 3};
        for (int i = 0; i < 6; ++i)
            indices[f * 6 + i] = uint16_t(f * 4 + quad[i]);
    }
    return indices;
}();
} // namespace micro

#include "lattice.hpp"
#include "packed_voxel.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
int main() {
    using namespace micro;
    constexpr int32_t values[] = {-524288, -524287, -4097, -4096, -1,     0,     1,
                                  255,     256,     4095,  4096,  524286, 524287};
    for (int32_t x : values)
        for (int32_t y : values)
            for (int32_t z : values)
                for (int32_t lod = 0; lod < 8; lod++) {
                    Cell key{x, y, z, lod};
                    auto packed = packVoxel(key, {0, .5f, 1, 1});
                    require(unpackCell(packed) == key, "Signed coordinates/LOD lost bits");
                    auto rgb = unpackColor(packed);
                    require(rgb == glm::vec4(0, .5f, 1, 1), "Exact half values changed");
                }
    auto golden = packVoxel({1, 256, -1, 5}, {1, 1, 1, 1});
    require(golden.low == 0x10000001u && golden.high == 0x5fffff00u,
            "CPU encoding disagrees with the shader's two-word layout");
    float maxError = 0;
    for (int i = 0; i <= 1023; i++) {
        float value = float(i) / 1023;
        auto rgb = unpackColor(packVoxel({-1, -2, -3, 0}, glm::vec4(value, value, value, 1)));
        maxError = std::max(maxError, std::abs(rgb.r - value));
        require(rgb.a == 1 && std::abs(rgb.r - value) <= 1.f / 4096,
                "Packed colour exceeded half-float rounding bounds");
    }
    for (int lod = 0; lod < 6; lod++) {
        Cell key{-171, 37, 142, lod};
        require(center(unpackCell(packVoxel(key, {0, 0, 0, 1})), .0057f) == center(key, .0057f),
                "Packed cells changed world-grid centres");
    }
    std::cout << "Signed key boundaries, word layout, world centres and RGBA16F passed; max RGB "
              << maxError << '\n';
}

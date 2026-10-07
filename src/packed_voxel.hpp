#pragma once
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
namespace micro {
// A 63-bit exact signed XYZ/LOD key plus RGBA16F, using four portable uint32 words.
struct PackedVoxel {
    uint32_t low, high, rg, ba;
};
static_assert(sizeof(PackedVoxel) == 16);
inline std::array<int32_t, 4> unpackCell(const PackedVoxel &v) {
    auto signed20 = [](uint32_t x) {
        x &= 0xfffffu;
        return int32_t(x ^ 0x80000u) - 0x80000;
    };
    return {signed20(v.low), signed20((v.low >> 20) | ((v.high & 255u) << 12)),
            signed20(v.high >> 8), int32_t((v.high >> 28) & 7u)};
}
inline PackedVoxel packVoxel(const std::array<int32_t, 4> &k, glm::vec4 rgb) {
    uint32_t x = uint32_t(k[0]) & 0xfffffu, y = uint32_t(k[1]) & 0xfffffu,
             z = uint32_t(k[2]) & 0xfffffu;
    return {x | (y << 20), (y >> 12) | (z << 8) | (uint32_t(k[3]) << 28),
            glm::packHalf2x16(glm::vec2(rgb)), glm::packHalf2x16(glm::vec2(rgb.z, 1))};
}
inline glm::vec4 unpackColor(const PackedVoxel &v) {
    auto rg = glm::unpackHalf2x16(v.rg), ba = glm::unpackHalf2x16(v.ba);
    return {rg.x, rg.y, ba.x, ba.y};
}
} // namespace micro

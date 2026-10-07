// No shaderInt64 requirement: a 63-bit key is encoded in two uint32 words.
uvec2 packCell(ivec4 cell) {
    uvec3 c = uvec3(cell.xyz) & uvec3(0xfffffu);
    return uvec2(c.x | (c.y << 20u), (c.y >> 12u) | (c.z << 8u) | (uint(cell.w) << 28u));
}
int signed20(uint x) {
    return int((x & 0xfffffu) ^ 0x80000u) - 0x80000;
}
ivec4 unpackCell(uvec2 key) {
    return ivec4(signed20(key.x), signed20((key.x >> 20u) | ((key.y & 255u) << 12u)),
                 signed20(key.y >> 8u), int((key.y >> 28u) & 7u));
}

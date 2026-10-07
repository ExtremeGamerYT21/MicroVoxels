const uint CACHE_OWNER = 0x80000000u;
const uint CACHE_CAPACITY = 262144u;
struct Voxel {
    vec4 centerSize, rgba;
};
layout(set = 0, binding = 12, std430) readonly buffer CachedInstances {
    Voxel cachedVoxels[];
};
layout(set = 0, binding = 13, std430) readonly buffer CachedSeen {
    float cachedSeen[];
};
ivec4 cachedKey(uint index) {
    Voxel v = cachedVoxels[index];
    int lod = int(round(log2(v.centerSize.w / (params.config.x * params.config.w))));
    return voxelKey(v.centerSize.xyz, uint(lod));
}
ivec4 resolvedKey(uint owner) {
    if ((owner & CACHE_OWNER) != 0u)
        return cachedKey(owner & ~CACHE_OWNER);
    ivec2 p = ownerPixel(owner);
    ivec4 key = voxelKey(texelFetch(positions, p, 0).xyz, imageLoad(lods, p).r);
    key.xyz += ownerOffset(owner);
    return key;
}

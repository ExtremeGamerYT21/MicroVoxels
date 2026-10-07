const uint ROOT_CAPACITY = 262144u;
struct Root {
    uint owner;
    uint lod;
};
uint hashKey(ivec4 k) {
    uvec4 v = uvec4(k) * uvec4(73856093u, 19349663u, 83492791u, 2654435761u);
    uint h = v.x ^ v.y ^ v.z ^ v.w;
    h ^= h >> 16;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
}
ivec3 rootCell(vec3 p) {
    return ivec3(floor(p / (params.config.x * exp2(float(params.flags.x - 1)))));
}
vec3 rootCenter(ivec3 c) {
    return (vec3(c) + .5) * (params.config.x * exp2(float(params.flags.x - 1)));
}
ivec4 voxelKey(vec3 p, uint lod) {
    return ivec4(ivec3(floor(p / (params.config.x * exp2(float(lod))))), int(lod));
}
ivec2 pixel(uint id) {
    return ivec2(int(id % uint(params.extent.x)), int(id / uint(params.extent.x)));
}
int distanceLod(float d, int old) {
    if (old < 0)
        return clamp(int(floor(log2(max(d / params.config.y, 1.0)))) +
                         (d >= params.config.y ? 1 : 0),
                     0, params.flags.x - 1);
    int lod = clamp(old, 0, params.flags.x - 1);
    while (lod > 0 && d < params.config.y * exp2(float(lod - 1)) * (1 - params.config.z))
        lod--;
    while (lod < params.flags.x - 1 &&
           d > params.config.y * exp2(float(lod)) * (1 + params.config.z))
        lod++;
    return lod;
}
int coverageLod(float footprint, int old) {
    int lod = clamp(max(old, 0), 0, params.flags.x - 1);
    while (lod > 0 && footprint < params.config.x * exp2(float(lod - 1)) * (1 - params.config.z))
        lod--;
    while (lod < params.flags.x - 1 && footprint > params.config.x * exp2(float(lod)))
        lod++;
    return lod;
}

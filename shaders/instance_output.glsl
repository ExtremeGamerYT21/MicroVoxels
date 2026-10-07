#include "instance_data.glsl"
layout(set = 0, binding = 10, std430) writeonly buffer Instances {
    uint instanceWords[];
};
void storeInstance(uint index, ivec4 cell, vec3 rgb) {
    if (params.optimization.y != 0) {
        uint i = index * 4u;
        uvec2 key = packCell(cell);
        instanceWords[i] = key.x;
        instanceWords[i + 1u] = key.y;
        instanceWords[i + 2u] = packHalf2x16(rgb.rg);
        instanceWords[i + 3u] = packHalf2x16(vec2(rgb.b, 1));
    } else {
        float h = params.config.x * exp2(float(cell.w));
        vec4 centerSize = vec4((vec3(cell.xyz) + .5) * h, h * params.config.w);
        uint i = index * 8u;
        for (uint a = 0u; a < 4u; a++)
            instanceWords[i + a] = floatBitsToUint(centerSize[a]);
        for (uint a = 0u; a < 3u; a++)
            instanceWords[i + 4u + a] = floatBitsToUint(rgb[a]);
        instanceWords[i + 7u] = floatBitsToUint(1.0);
    }
}

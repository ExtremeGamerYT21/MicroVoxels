#include "instance_data.glsl"
struct Voxel {
    vec4 centerSize, rgba;
};
layout(set = 0, binding = 10, std430) readonly buffer Instances {
    uint instanceWords[];
};
Voxel loadVoxel(uint index) {
    Voxel voxel;
    if (params.optimization.y != 0) {
        uint i = index * 4u;
        ivec4 cell = unpackCell(uvec2(instanceWords[i], instanceWords[i + 1u]));
        float h = params.config.x * exp2(float(cell.w));
        voxel.centerSize = vec4((vec3(cell.xyz) + .5) * h, h * params.config.w);
        vec2 rg = unpackHalf2x16(instanceWords[i + 2u]), ba = unpackHalf2x16(instanceWords[i + 3u]);
        voxel.rgba = vec4(rg, ba);
    } else {
        uint i = index * 8u;
        for (uint a = 0u; a < 4u; a++)
            voxel.centerSize[a] = uintBitsToFloat(instanceWords[i + a]);
        for (uint a = 0u; a < 4u; a++)
            voxel.rgba[a] = uintBitsToFloat(instanceWords[i + 4u + a]);
    }
    return voxel;
}
const vec3 normals[6] = vec3[](vec3(1, 0, 0), vec3(-1, 0, 0), vec3(0, 1, 0), vec3(0, -1, 0),
                               vec3(0, 0, 1), vec3(0, 0, -1));
const vec2 corners[6] =
    vec2[](vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1));
vec3 cubeVertex(out vec4 color, out vec3 faceDirection) {
    int face = gl_VertexIndex / 6;
    vec3 n = normals[face], u, v;
    if (face < 2) {
        u = vec3(0, 0, 1);
        v = vec3(0, 1, 0);
    } else if (face < 4) {
        u = vec3(1, 0, 0);
        v = vec3(0, 0, 1);
    } else {
        u = vec3(1, 0, 0);
        v = vec3(0, 1, 0);
    }
    vec2 c = corners[gl_VertexIndex % 6];
    Voxel voxel = loadVoxel(gl_InstanceIndex);
    color = voxel.rgba;
    faceDirection = n;
    if (params.config.w == 1.0) {
        // Construct shared corners from integer grid boundaries. Independent centre
        // +/- half-width arithmetic can round adjoining faces to different positions.
        vec3 cell = floor(voxel.centerSize.xyz / voxel.centerSize.w);
        return (cell + .5 + .5 * (n + u * c.x + v * c.y)) * voxel.centerSize.w;
    }
    return voxel.centerSize.xyz + voxel.centerSize.w * .5 * (n + u * c.x + v * c.y);
}

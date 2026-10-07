struct Voxel {
    vec4 centerSize;
    vec4 rgba;
};
layout(set = 0, binding = 10, std430) readonly buffer Instances {
    Voxel voxels[];
};
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
    Voxel voxel = voxels[gl_InstanceIndex];
    color = voxel.rgba;
    faceDirection = n;
    return voxel.centerSize.xyz + voxel.centerSize.w * .5 * (n + u * c.x + v * c.y);
}

#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
#include "cube_draw.glsl"
layout(location = 0) flat out vec4 color;
void main() {
    // Eight shared corners; the fixed 36-index mesh preserves legacy triangles.
    int i = gl_VertexIndex;
    vec3 corner = vec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
    Voxel voxel = voxels[gl_InstanceIndex];
    color = voxel.rgba;
    gl_Position = frame.viewProjection * vec4(gridCorner(voxel, corner), 1);
}

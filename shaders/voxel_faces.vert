#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
#include "cube_draw.glsl"
layout(location = 0) flat out vec4 color;
void main() {
    vec3 normal;
    gl_Position = frame.viewProjection * vec4(cubeFaceVertex(color, normal), 1);
}

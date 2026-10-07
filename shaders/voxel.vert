#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
#include "cube.glsl"
layout(location = 0) flat out vec4 color;
void main() {
    vec3 direction;
    vec3 p = cubeVertex(color, direction);
    gl_Position = frame.viewProjection * vec4(p, 1);
}

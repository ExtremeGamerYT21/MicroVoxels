#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
#include "cube.glsl"
layout(location = 0) flat out vec4 color;
layout(location = 1) flat out vec3 faceNormal;
void main() {
    vec3 p = cubeVertex(color, faceNormal);
    gl_Position = frame.viewProjection * vec4(p, 1);
}

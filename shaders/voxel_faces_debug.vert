#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
#include "cube_draw.glsl"
layout(location = 0) flat out vec4 color;
layout(location = 1) flat out vec3 faceNormal;
void main() {
    gl_Position = frame.viewProjection * vec4(cubeFaceVertex(color, faceNormal), 1);
}

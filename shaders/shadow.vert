#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location = 0) in vec3 position;
layout(location = 3) in vec2 material;
void main() {
    gl_Position = frame.lightViewProjection * vec4(animatePosition(position, material.x), 1);
}

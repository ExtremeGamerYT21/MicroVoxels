#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 material;
layout(location = 0) out vec3 worldPosition;
layout(location = 1) out vec3 worldNormal;
layout(location = 2) out vec3 albedo;
layout(location = 3) out float gloss;
void main() {
    worldPosition = animatePosition(position, material.x);
    worldNormal = animateNormal(normal, position, material.x);
    albedo = color;
    gloss = material.y;
    gl_Position = frame.viewProjection * vec4(worldPosition, 1);
}

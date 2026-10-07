#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
#include "source_shading.glsl"
layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec3 albedo;
layout(location = 3) in float gloss;
layout(location = 0) out vec4 shadedColor;
layout(location = 1) out vec4 hitPosition;
void main() {
    shadedColor = vec4(shadeSurface(worldPosition, worldNormal, albedo, gloss), 1);
    hitPosition = vec4(worldPosition, 1);
}

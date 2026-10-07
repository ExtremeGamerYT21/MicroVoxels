#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location = 0) flat in vec4 color;
layout(location = 1) flat in vec3 faceNormal;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = color;
    outColor.rgb *= .18 + .82 * max(dot(faceNormal, normalize(frame.light.xyz)), 0);
}

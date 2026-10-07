#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
#include "source_vertex.glsl"
layout(set = 0, binding = 3, std430) readonly buffer AnimatedVertices {
    SourceVertex vertices[];
};
void main() {
    gl_Position = frame.lightViewProjection * vec4(vertices[gl_VertexIndex].position.xyz, 1);
}

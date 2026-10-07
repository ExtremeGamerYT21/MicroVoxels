#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
#include "source_vertex.glsl"
layout(set = 0, binding = 3, std430) readonly buffer AnimatedVertices {
    SourceVertex vertices[];
};
layout(location = 0) out vec3 worldPosition;
layout(location = 1) out vec3 worldNormal;
layout(location = 2) out vec3 albedo;
layout(location = 3) out float gloss;
void main() {
    SourceVertex v = vertices[gl_VertexIndex];
    worldPosition = v.position.xyz;
    worldNormal = v.normal.xyz;
    albedo = v.albedo.xyz;
    gloss = v.normal.w;
    gl_Position = frame.viewProjection * vec4(worldPosition, 1);
}

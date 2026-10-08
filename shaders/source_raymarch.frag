#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
#include "source_vertex.glsl"
#include "source_shading.glsl"
layout(set = 0, binding = 3, std430) readonly buffer AnimatedVertices {
    SourceVertex vertices[];
};
#include "source_distance.glsl"
layout(set = 0, binding = 6, std430) buffer TraceStats {
    uint rays, steps, traceHits, exhausted;
};
layout(location = 0) out vec4 shadedColor;
layout(location = 1) out vec4 hitPosition;
void main() {
    atomicAdd(rays, 1u);
    vec2 xy = 2 * (gl_FragCoord.xy + params.temporal.zw) / vec2(params.extent.xy) - 1;
    mat4 inverse = inverse(frame.viewProjection);
    vec4 near4 = inverse * vec4(xy, 0, 1), far4 = inverse * vec4(xy, 1, 1);
    vec3 nearPoint = near4.xyz / near4.w, farPoint = far4.xyz / far4.w;
    vec3 direction = normalize(farPoint - nearPoint), origin = frame.cameraTime.xyz;
    float t = distance(nearPoint, origin), far = distance(farPoint, origin);
    uint used = 0u, triangle = 0u;
    vec3 weights;
    bool found = false;
    for (uint i = 0u; i < uint(params.temporal.y) && t < far; i++) {
        used++;
        float d = sceneDistance(origin + direction * t, triangle, weights);
        if (d <= .0001) {
            found = true;
            break;
        }
        t += max(d * .9, .00002);
    }
    atomicAdd(steps, used);
    if (!found) {
        if (t < far) atomicAdd(exhausted, 1u);
        discard;
    }
    atomicAdd(traceHits, 1u);
    uint first = triangle * 3u;
    SourceVertex a = vertices[first], b = vertices[first + 1u], c = vertices[first + 2u];
    vec3 position = origin + direction * t;
    vec3 normal = a.normal.xyz * weights.x + b.normal.xyz * weights.y + c.normal.xyz * weights.z;
    vec3 albedo = a.albedo.xyz * weights.x + b.albedo.xyz * weights.y + c.albedo.xyz * weights.z;
    float gloss = dot(weights, vec3(a.normal.w, b.normal.w, c.normal.w));
    vec4 clip = frame.viewProjection * vec4(position, 1);
    gl_FragDepth = clip.z / clip.w;
    hitPosition = vec4(position, 1);
    shadedColor = vec4(shadeSurface(position, normal, albedo, gloss), 1);
}

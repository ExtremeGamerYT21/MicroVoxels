#include "source_vertex.glsl"
#define SHADOW_BINDING 15
#include "source_shading.glsl"
layout(set = 0, binding = 12, std430) readonly buffer SourceVertices {
    SourceVertex vertices[];
};
layout(set = 0, binding = 13, std430) buffer VisibleTriangles {
    uint visible[];
};
struct Candidate {
    ivec4 key;
    uvec4 source;
};
#ifndef CANDIDATE_ACCESS
#define CANDIDATE_ACCESS readonly
#endif
layout(set = 0, binding = 14, std430) CANDIDATE_ACCESS buffer Candidates {
    Candidate candidates[];
};
const float CELL_EPSILON = .0001;
float regionSize() {
    return params.config.x * 32.0;
}
uint regionIndex(ivec3 root) {
    ivec3 p = root - params.gridMin.xyz;
    return uint((p.z * params.gridExtent.y + p.y) * params.gridExtent.x + p.x);
}
ivec3 regionAt(uint id) {
    uint x = uint(params.gridExtent.x), y = uint(params.gridExtent.y);
    return params.gridMin.xyz + ivec3(int(id % x), int((id / x) % y), int(id / (x * y)));
}
bool axisSeparates(vec3 axis, vec3 a, vec3 b, vec3 c) {
    vec3 p = vec3(dot(axis, a), dot(axis, b), dot(axis, c));
    float radius = (.5 + CELL_EPSILON) * dot(abs(axis), vec3(1));
    return min(p.x, min(p.y, p.z)) > radius || max(p.x, max(p.y, p.z)) < -radius;
}
// Separating-axis triangle–AABB test: 3 box axes, the triangle plane and 9 edge cross axes.
// Coordinates are relative to the geometric cell, before the visual splat enlargement.
bool triangleBox(vec3 a, vec3 b, vec3 c, vec3 center, float size) {
    a = (a - center) / size;
    b = (b - center) / size;
    c = (c - center) / size;
    vec3 lo = min(a, min(b, c)), hi = max(a, max(b, c));
    if (any(greaterThan(lo, vec3(.5 + CELL_EPSILON))) ||
        any(lessThan(hi, vec3(-.5 - CELL_EPSILON))))
        return false;
    vec3 edges[3] = vec3[3](b - a, c - b, a - c);
    if (axisSeparates(cross(edges[0], edges[1]), a, b, c))
        return false;
    for (int e = 0; e < 3; e++) {
        vec3 f = edges[e];
        if (axisSeparates(vec3(0, -f.z, f.y), a, b, c) ||
            axisSeparates(vec3(f.z, 0, -f.x), a, b, c) ||
            axisSeparates(vec3(-f.y, f.x, 0), a, b, c))
            return false;
    }
    return true;
}
// Conservative region culling only; no pixel samples decide individual cell occupancy.
bool regionInView(ivec3 root) {
    bool outside[6] = bool[6](true, true, true, true, true, true);
    float h = regionSize();
    for (int i = 0; i < 8; i++) {
        vec3 corner = (vec3(root) + vec3(i & 1, (i >> 1) & 1, (i >> 2) & 1)) * h;
        vec4 q = frame.viewProjection * vec4(corner, 1);
        outside[0] = outside[0] && q.x < -q.w;
        outside[1] = outside[1] && q.x > q.w;
        outside[2] = outside[2] && q.y < -q.w;
        outside[3] = outside[3] && q.y > q.w;
        outside[4] = outside[4] && q.z < 0;
        outside[5] = outside[5] && q.z > q.w;
    }
    for (int i = 0; i < 6; i++)
        if (outside[i])
            return false;
    return true;
}
vec3 cellBarycentric(vec3 p, vec3 a, vec3 b, vec3 c) {
    vec3 ab = b - a, ac = c - a, ap = p - a;
    float d1 = dot(ab, ap), d2 = dot(ac, ap);
    if (d1 <= 0 && d2 <= 0)
        return vec3(1, 0, 0);
    vec3 bp = p - b;
    float d3 = dot(ab, bp), d4 = dot(ac, bp);
    if (d3 >= 0 && d4 <= d3)
        return vec3(0, 1, 0);
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) {
        float v = d1 / (d1 - d3);
        return vec3(1 - v, v, 0);
    }
    vec3 cp = p - c;
    float d5 = dot(ab, cp), d6 = dot(ac, cp);
    if (d6 >= 0 && d5 <= d6)
        return vec3(0, 0, 1);
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) {
        float w = d2 / (d2 - d6);
        return vec3(1 - w, 0, w);
    }
    float va = d3 * d6 - d5 * d4;
    if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return vec3(0, 1 - w, w);
    }
    float sum = va + vb + vc;
    return vec3(va, vb, vc) / sum;
}
vec3 candidatePoint(uint id) {
    Candidate candidate = candidates[id - 1u];
    uint first = candidate.source.x * 3u;
    vec3 a = vertices[first].position.xyz, b = vertices[first + 1u].position.xyz,
         c = vertices[first + 2u].position.xyz;
    float h = params.config.x * exp2(float(candidate.key.w));
    vec3 w = cellBarycentric((vec3(candidate.key.xyz) + .5) * h, a, b, c);
    return w.x * a + w.y * b + w.z * c;
}
void candidateSurface(uint id, out vec3 point, out vec3 rgb) {
    Candidate candidate = candidates[id - 1u];
    uint first = candidate.source.x * 3u;
    SourceVertex a = vertices[first], b = vertices[first + 1u], c = vertices[first + 2u];
    float h = params.config.x * exp2(float(candidate.key.w));
    vec3 center = (vec3(candidate.key.xyz) + .5) * h;
    vec3 w = cellBarycentric(center, a.position.xyz, b.position.xyz, c.position.xyz);
    point = w.x * a.position.xyz + w.y * b.position.xyz + w.z * c.position.xyz;
    rgb = shadeSurface(point, w.x * a.normal.xyz + w.y * b.normal.xyz + w.z * c.normal.xyz,
                       w.x * a.albedo.xyz + w.y * b.albedo.xyz + w.z * c.albedo.xyz,
                       w.x * a.normal.w + w.y * b.normal.w + w.z * c.normal.w);
}

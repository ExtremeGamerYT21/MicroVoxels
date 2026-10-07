// Five source-buffer samples; no mesh IDs, source normals or material records.
layout(set = 0, binding = 1) uniform sampler2D positions;
struct Footprint {
    vec3 dx, dy;
    uint rejected;
    bool clamped;
};
float nominalPixelSpan(vec3 hit) {
    float depth = abs((frame.viewProjection * vec4(hit, 1)).w);
    vec3 x =
        vec3(frame.viewProjection[0][0], frame.viewProjection[1][0], frame.viewProjection[2][0]);
    vec3 y =
        vec3(frame.viewProjection[0][1], frame.viewProjection[1][1], frame.viewProjection[2][1]);
    return max(2 * depth / (float(params.extent.x) * length(x)),
               2 * depth / (float(params.extent.y) * length(y)));
}
vec3 surfaceStep(ivec2 p, ivec2 axis, vec3 hit, inout uint rejected) {
    vec3 steps[2];
    float lengths[2];
    bool valid[2] = bool[2](false, false);
    float nominal = nominalPixelSpan(hit);
    float maximum = min(.25, 8 * nominal);
    float depth = (frame.viewProjection * vec4(hit, 1)).w;
    for (int i = 0; i < 2; i++) {
        int sign = i == 0 ? -1 : 1;
        ivec2 q = p + sign * axis;
        if (any(lessThan(q, ivec2(0))) || any(greaterThanEqual(q, params.extent.xy)))
            continue;
        vec4 neighbor = texelFetch(positions, q, 0);
        if (neighbor.w == 0)
            continue;
        vec3 delta = neighbor.xyz - hit;
        float distance2 = dot(delta, delta);
        float jump = abs((frame.viewProjection * vec4(neighbor.xyz, 1)).w - depth);
        if (distance2 > maximum * maximum || jump > max(4 * nominal, .025 * abs(depth))) {
            rejected++;
            continue;
        }
        steps[i] = delta * float(sign);
        lengths[i] = distance2;
        valid[i] = true;
    }
    if (valid[0] && valid[1]) {
        int near = lengths[0] <= lengths[1] ? 0 : 1, far = 1 - near;
        // A short, continuous side can survive an object edge. Comparable opposing
        // directions are unreliable, for example an isolated foreground pixel.
        if (lengths[far] > 4 * lengths[near]) {
            rejected++;
            return steps[near];
        }
        if (dot(steps[0], steps[1]) < .5 * sqrt(lengths[0] * lengths[1])) {
            rejected += 2u;
            return vec3(0);
        }
        return steps[near];
    }
    return valid[0] ? steps[0] : (valid[1] ? steps[1] : vec3(0));
}
Footprint rawFootprint(ivec2 p, vec3 hit) {
    Footprint f;
    f.rejected = 0u;
    f.clamped = false;
    f.dx = surfaceStep(p, ivec2(1, 0), hit, f.rejected);
    f.dy = surfaceStep(p, ivec2(0, 1), hit, f.rejected);
    return f;
}
Footprint boundedFootprint(ivec2 p, vec3 hit, float h) {
    Footprint f;
    f.dx = f.dy = vec3(0);
    f.rejected = 0u;
    f.clamped = false;
    if (frame.options.z == 0 || params.footprint.x == 0)
        return f;
    f = rawFootprint(p, hit);
    vec3 extent = .5 * (abs(f.dx) + abs(f.dy));
    float longest = max(extent.x, max(extent.y, extent.z));
    float maximum = float(params.footprint.x) * h;
    if (longest > maximum) {
        f.dx *= maximum / longest;
        f.dy *= maximum / longest;
        f.clamped = true;
    }
    return f;
}
// SAT for a source-pixel parallelogram (also handles line/point fallbacks).
bool footprintAxis(vec3 axis, vec3 center, vec3 dx, vec3 dy) {
    float radius = .5001 * dot(abs(axis), vec3(1)) + abs(dot(axis, dx)) + abs(dot(axis, dy));
    return abs(dot(axis, center)) > radius;
}
bool footprintBox(vec3 hit, Footprint f, vec3 center, float h) {
    vec3 p = (hit - center) / h, x = .5 * f.dx / h, y = .5 * f.dy / h;
    if (any(greaterThan(abs(p), vec3(.5001) + abs(x) + abs(y))))
        return false;
    if (footprintAxis(cross(x, y), p, x, y))
        return false;
    for (int i = 0; i < 2; i++) {
        vec3 edge = i == 0 ? x : y;
        if (footprintAxis(vec3(0, -edge.z, edge.y), p, x, y) ||
            footprintAxis(vec3(edge.z, 0, -edge.x), p, x, y) ||
            footprintAxis(vec3(-edge.y, edge.x, 0), p, x, y))
            return false;
    }
    return true;
}
// Immutable owner = source pixel plus one of 125 offsets in [-2,2]^3.
// Competing invocations recover the complete key without a publication lock.
uint ownerId(ivec2 p, ivec3 offset) {
    uvec3 q = uvec3(offset + 2);
    uint k = (q.z * 5u + q.y) * 5u + q.x;
    return uint(p.y * params.extent.x + p.x) * 125u + k + 1u;
}
ivec2 ownerPixel(uint id) {
    return pixel((id - 1u) / 125u);
}
ivec3 ownerOffset(uint id) {
    uint k = (id - 1u) % 125u;
    return ivec3(int(k % 5u), int((k / 5u) % 5u), int(k / 25u)) - 2;
}

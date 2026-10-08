struct DistanceNode {
    vec4 low, high;
    uvec4 links;
};
layout(set = 0, binding = 4, std430) readonly buffer DistanceNodes {
    DistanceNode nodes[];
};
layout(set = 0, binding = 5, std430) readonly buffer DistanceTriangles {
    uint triangles[];
};
// Closest point, not a ray/triangle intersection. The union of these unsigned
// surface distances is a distance field, including open grass ribbons.
vec3 triangleWeights(vec3 p, vec3 a, vec3 b, vec3 c) {
    vec3 ab = b - a, ac = c - a, ap = p - a;
    float d1 = dot(ab, ap), d2 = dot(ac, ap);
    if (d1 <= 0 && d2 <= 0) return vec3(1, 0, 0);
    vec3 bp = p - b;
    float d3 = dot(ab, bp), d4 = dot(ac, bp);
    if (d3 >= 0 && d4 <= d3) return vec3(0, 1, 0);
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) {
        float v = d1 / (d1 - d3);
        return vec3(1 - v, v, 0);
    }
    vec3 cp = p - c;
    float d5 = dot(ab, cp), d6 = dot(ac, cp);
    if (d6 >= 0 && d5 <= d6) return vec3(0, 0, 1);
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
    float inverse = 1 / (va + vb + vc);
    float v = vb * inverse, w = vc * inverse;
    return vec3(1 - v - w, v, w);
}
float boxDistance2(vec3 p, uint node) {
    vec3 d = max(max(nodes[node].low.xyz - p, p - nodes[node].high.xyz), vec3(0));
    return dot(d, d);
}
float sceneDistance(vec3 p, out uint triangle, out vec3 weights) {
    uint stack[64];
    int size = 1;
    stack[0] = 0u;
    float best = 1e30;
    triangle = 0u;
    weights = vec3(1, 0, 0);
    while (size > 0) {
        uint index = stack[--size];
        if (boxDistance2(p, index) > best)
            continue;
        DistanceNode node = nodes[index];
        if (node.links.w > 0u) {
            for (uint i = 0u; i < node.links.w; i++) {
                uint t = triangles[node.links.z + i], first = t * 3u;
                vec3 a = vertices[first].position.xyz, b = vertices[first + 1u].position.xyz,
                     c = vertices[first + 2u].position.xyz;
                vec3 w = triangleWeights(p, a, b, c);
                vec3 delta = p - (a * w.x + b * w.y + c * w.z);
                float d = dot(delta, delta);
                if (d < best || (d == best && t < triangle)) {
                    best = d;
                    triangle = t;
                    weights = w;
                }
            }
        } else {
            uint left = node.links.x, right = node.links.y;
            float a = boxDistance2(p, left), b = boxDistance2(p, right);
            // Median-split tree depth is <32 for the demo. A 64-entry stack
            // bounds traversal without recursion or source geometry changes.
            if (a < b) {
                if (b <= best) stack[size++] = right;
                if (a <= best) stack[size++] = left;
            } else {
                if (a <= best) stack[size++] = left;
                if (b <= best) stack[size++] = right;
            }
        }
    }
    return sqrt(best);
}

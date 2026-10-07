layout(set = 0, binding = 1) uniform sampler2D positions;
layout(set = 0, binding = 2) uniform sampler2D previousPositions;
vec4 reconstructPosition(ivec2 p, float depth, mat4 inverse) {
    if (depth >= 1.0)
        return vec4(0);
    vec2 ndc = 2.0 * (vec2(p) + .5) / vec2(params.extent.xy) - 1.0;
    precise vec4 q = inverse * vec4(ndc, depth, 1);
    precise vec3 world = q.xyz / q.w;
    return vec4(world, 1);
}
vec4 sourcePosition(ivec2 p) {
    vec4 sampleValue = texelFetch(positions, p, 0);
    return params.footprint.z != 0
               ? reconstructPosition(p, sampleValue.r, frame.inverseViewProjection)
               : sampleValue;
}
vec4 previousPosition(ivec2 p) {
    vec4 sampleValue = texelFetch(previousPositions, p, 0);
    return params.footprint.z != 0
               ? reconstructPosition(p, sampleValue.r, frame.previousInverseViewProjection)
               : sampleValue;
}
#ifdef SURFACE_TILE
// 8x8 output pixels with a one-pixel halo. Only the depth variant uses the tile.
shared vec4 positionTile[100];
void prepareSurfaceTile() {
    if (params.footprint.z != 0 && (params.footprint.w & 1) != 0) {
        for (uint i = gl_LocalInvocationIndex; i < 100u; i += 64u) {
            ivec2 p = ivec2(gl_WorkGroupID.xy) * 8 + ivec2(int(i % 10u), int(i / 10u)) - 1;
            positionTile[i] =
                any(lessThan(p, ivec2(0))) || any(greaterThanEqual(p, params.extent.xy))
                    ? vec4(0)
                    : sourcePosition(p);
        }
        barrier();
    }
}
vec4 footprintPosition(ivec2 p) {
    if (params.footprint.z != 0 && (params.footprint.w & 1) != 0) {
        ivec2 q = p - ivec2(gl_WorkGroupID.xy) * 8 + 1;
        return positionTile[q.y * 10 + q.x];
    }
    return sourcePosition(p);
}
#else
vec4 footprintPosition(ivec2 p) {
    return sourcePosition(p);
}
#endif

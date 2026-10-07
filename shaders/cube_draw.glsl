// Rendering helpers only: no source visibility, source topology or lighting.
#include "cube.glsl"

vec3 gridCorner(Voxel voxel, vec3 offset) {
    if (params.config.w == 1.0) {
        vec3 cell = floor(voxel.centerSize.xyz / voxel.centerSize.w);
        return (cell + offset) * voxel.centerSize.w;
    }
    return voxel.centerSize.xyz + voxel.centerSize.w * (offset - .5);
}

// Hardware culling normally removes exit faces. They are visible in the legacy
// two-sided draw if the near plane clips a cube's entry surface (also when the
// camera is inside a frozen cube). Keep those faces by reversing their winding.
// The world-aligned cube bounds give a conservative near-plane test directly.
bool preserveExitFace(Voxel voxel, vec3 n) {
    vec3 center = gridCorner(voxel, vec3(.5));
    float halfSize = voxel.centerSize.w * .5;
    vec4 nearPlane = vec4(frame.viewProjection[0][2], frame.viewProjection[1][2],
                          frame.viewProjection[2][2], frame.viewProjection[3][2]);
    float z = dot(nearPlane, vec4(center, 1));
    float extent = halfSize * dot(abs(nearPlane.xyz), vec3(1));
    float tolerance = 1e-5 * max(1.0, abs(z) + extent);
    return z <= extent + tolerance &&
           dot(n, frame.cameraTime.xyz - center) <= halfSize;
}

vec3 cubeFaceVertex(out vec4 color, out vec3 faceNormal) {
    bool indexed = params.footprint.w != 0;
    // Keep divisors constant so the compiler can use shifts / reciprocal multiplies.
    int face = indexed ? gl_VertexIndex / 4 : gl_VertexIndex / 6;
    int corner = indexed ? gl_VertexIndex % 4 : gl_VertexIndex % 6;
    const vec2 quad[4] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, 1));
    // The indexed face mesh already has outward winding. Correct the legacy
    // faces here without altering triangle diagonals or draw order.
    if (!indexed && (face == 0 || face == 2 || face == 5) && corner % 3 != 0)
        corner = (corner / 3) * 3 + 3 - corner % 3;
    vec2 c = indexed ? quad[corner] : corners[corner];
    vec3 n = normals[face], u, v;
    if (face < 2) { u = vec3(0, 0, 1); v = vec3(0, 1, 0); }
    else if (face < 4) { u = vec3(1, 0, 0); v = vec3(0, 0, 1); }
    else { u = vec3(1, 0, 0); v = vec3(0, 1, 0); }
    Voxel voxel = voxels[gl_InstanceIndex];
    if (params.footprint.z != 0 && preserveExitFace(voxel, n))
        c = c.yx; // reverse winding while retaining the original quad diagonal
    color = voxel.rgba;
    faceNormal = n;
    return gridCorner(voxel, .5 + .5 * (n + u * c.x + v * c.y));
}

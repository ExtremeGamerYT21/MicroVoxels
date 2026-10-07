layout(set = 0, binding = 0, std140) uniform Frame {
    mat4 viewProjection;
    mat4 lightViewProjection;
    vec4 cameraTime;
    vec4 light;
    vec4 options;
}
frame;
layout(push_constant) uniform Parameters {
    ivec4 extent;    // sample width, sample height, hash capacity, max instances
    vec4 config;     // base size, first LOD distance, hysteresis, splat scale
    ivec4 flags;     // LOD count, average (1) / closest (0), cube-light debug, view mode
    ivec4 footprint; // radius, write limit, renderer-only cull/index switches
}
params;
vec3 animatePosition(vec3 p, float weight) {
    float y = max(p.y - .5, 0.0);
    return p + vec3(.13 * weight * y * y * sin(frame.cameraTime.w * 1.4), 0,
                    .055 * weight * y * y * sin(frame.cameraTime.w * 1.1 + .6));
}
vec3 animateNormal(vec3 n, vec3 p, float weight) {
    float y = max(p.y - .5, 0.0);
    float dx = .26 * weight * y * sin(frame.cameraTime.w * 1.4);
    float dz = .11 * weight * y * sin(frame.cameraTime.w * 1.1 + .6);
    return normalize(vec3(n.x, n.y - dx * n.x - dz * n.z, n.z));
}

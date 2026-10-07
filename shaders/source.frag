#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(set = 0, binding = 1) uniform sampler2D shadowMap;
layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec3 albedo;
layout(location = 3) in float gloss;
layout(location = 0) out vec4 shadedColor;
layout(location = 1) out vec4 hitPosition;
void main() {
    vec3 n = normalize(worldNormal);
    vec3 l = normalize(frame.light.xyz), v = normalize(frame.cameraTime.xyz - worldPosition);
    if (dot(n, v) < 0)
        n = -n; // two-sided leaves; smooth source normals stay continuous
    float ndl = max(dot(n, l), 0), visibility = 1;
    if (frame.options.x > 1.5) {
        vec4 q = frame.lightViewProjection * vec4(worldPosition, 1);
        vec3 s = q.xyz / q.w;
        vec2 uv = s.xy * .5 + .5;
        if (all(greaterThanEqual(uv, vec2(0))) && all(lessThanEqual(uv, vec2(1))) && s.z >= 0 &&
            s.z <= 1) {
            visibility = 0;
            vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
            float bias = max(.0007, .002 * (1 - ndl));
            for (int y = -1; y <= 1; y++)
                for (int x = -1; x <= 1; x++)
                    visibility +=
                        s.z - bias <= texture(shadowMap, uv + vec2(x, y) * texel).r ? 1.0 / 9.0 : 0;
        }
    }
    vec3 base = albedo;
    if (gloss < 0) {
        float check = mod(floor(worldPosition.x * 1.5) + floor(worldPosition.z * 1.5), 2);
        base *= mix(.68, 1.0, check);
    }
    vec3 rgb = base;
    if (frame.options.x > .5) {
        float spec = pow(max(dot(n, normalize(l + v)), 0), max(gloss, 8)) * visibility * .75;
        rgb = base * (.22 + 1.4 * ndl * visibility) + vec3(1, .93, .82) * spec;
        rgb = vec3(1) - exp(-rgb * frame.light.w);
    }
    shadedColor = vec4(rgb, 1);
    hitPosition = vec4(worldPosition, 1);
}

#version 450
layout(set = 0, binding = 7) uniform sampler2D colors;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;
void main() {
    vec4 c = texture(colors, uv);
    outColor = c.a > 0 ? c : vec4(.035, .055, .09, 1);
}

#version 410
uniform sampler2D prev;
uniform sampler2D curr;
uniform float decay;
in vec2 vTexCoord;
out vec4 fragColor;
void main() {
    vec2 uv = vTexCoord * 0.998 + 0.001;
    vec3 a = texture(prev, uv).rgb * decay;
    vec3 b = texture(curr, vTexCoord).rgb;
    fragColor = vec4(max(a, b * 0.85) + b * 0.15, 1.0);
}

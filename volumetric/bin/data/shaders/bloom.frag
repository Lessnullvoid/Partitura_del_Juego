#version 410
uniform sampler2D src;
uniform float threshold;
in vec2 vTexCoord;
out vec4 fragColor;
void main() {
    vec3 c = texture(src, vTexCoord).rgb;
    float l = max(max(c.r, c.g), c.b);
    fragColor = vec4(c * step(threshold, l), 1.0);
}

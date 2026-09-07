#version 410
uniform sampler2D src;
uniform float exposure;
uniform float gammaVal;
in vec2 vTexCoord;
out vec4 fragColor;
void main() {
    vec3 c = texture(src, vTexCoord).rgb * exposure;
    c = c / (c + vec3(1.0));
    c = pow(max(c, vec3(0.0)), vec3(1.0 / gammaVal));
    fragColor = vec4(c, 1.0);
}

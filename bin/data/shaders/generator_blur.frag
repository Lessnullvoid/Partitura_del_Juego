#version 410

uniform sampler2DRect tex0;
uniform vec2 direction;

in vec2 vTexcoord;
out vec4 fragColor;

void main() {
    vec4 color = texture(tex0, vTexcoord) * 0.227027;
    color += texture(tex0, vTexcoord + direction * 1.384615) * 0.316216;
    color += texture(tex0, vTexcoord - direction * 1.384615) * 0.316216;
    color += texture(tex0, vTexcoord + direction * 3.230769) * 0.070270;
    color += texture(tex0, vTexcoord - direction * 3.230769) * 0.070270;
    fragColor = color;
}

#version 150

in vec4 vColor;
out vec4 fragColor;

void main() {
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float r = dot(uv, uv);
    if (r > 1.0)
        discard;
    float falloff = exp(-r * 2.8);
    fragColor = vec4(vColor.rgb, vColor.a * falloff);
}

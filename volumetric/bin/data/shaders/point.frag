#version 150

in vec4 vColor;
out vec4 fragColor;

void main() {
    vec2 d = gl_PointCoord * 2.0 - 1.0;
    float r = dot(d, d);
    if (r > 1.0)
        discard;
    float core = exp(-5.0 * r);
    float halo = exp(-1.8 * r) * 0.35;
    float intensity = core + halo;
    fragColor = vec4(vColor.rgb * intensity, clamp(vColor.a * intensity, 0.0, 1.0));
}

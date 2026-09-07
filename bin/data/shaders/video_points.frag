#version 410

in vec4 pointColor;
out vec4 fragColor;

void main() {
    if (pointColor.a <= 0.001)
        discard;
    vec2 q = gl_PointCoord * 2.0 - 1.0;
    float radius2 = dot(q, q);
    if (radius2 > 1.0)
        discard;
    float core = exp(-4.5 * radius2);
    float halo = exp(-1.5 * radius2) * 0.2;
    float a = pointColor.a * (core + halo);
    fragColor = vec4(pointColor.rgb * (core + halo), a);
}

#version 410
uniform sampler2D srcPos;
uniform sampler2D srcVel;
uniform sampler2D srcTarget;
uniform float drag;
uniform float attract;
uniform float impulse;
uniform float dt;
uniform vec2 res;
uniform int writeVel;
out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy / res;
    vec4 p = texture(srcPos, uv);
    vec4 v = texture(srcVel, uv);
    vec4 t = texture(srcTarget, uv);
    vec3 force = (t.xyz - p.xyz) * attract;
    force.y += 0.015 * sin(uv.x * 36.0 + t.w);
    float alive = t.a;
    v.xyz = v.xyz * (1.0 - drag * dt) + force * dt + vec3(impulse * t.w * alive, 0.0, 0.0);
    p.xyz += v.xyz * dt;
    p.xyz = mix(p.xyz, t.xyz, 1.0 - alive);
    if (writeVel == 1)
        fragColor = vec4(v.xyz, 1.0);
    else
        fragColor = vec4(p.xyz, alive);
}

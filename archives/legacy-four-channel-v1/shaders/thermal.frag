#version 410

// Thermal camera (Ironbow / FLIR) colormap.
// Input:  raw color video texture (same as bw.frag input).
// Computes Rec.709 luminance internally, applies tonal controls, then colormaps.

uniform sampler2DRect tex0;
uniform float time;

// Tonal controls (same defaults as bw.frag)
uniform float brightness;      // -0.5 .. 0.5     default 0.0
uniform float contrast;        // 0.5 .. 3.0      default 1.3
uniform float gamma;           // 0.3 .. 2.2      default 0.95
uniform float sCurve;          // 0 .. 1          default 0.55

in  vec2 vTexCoord;
out vec4 fragColor;

// --- Ironbow palette ----------------------------------------------------------
// FLIR Ironbow: black → deep violet → dark crimson → orange → yellow → white
vec3 ironbow(float t) {
    t = clamp(t, 0.0, 1.0);

    const vec3 c0 = vec3(0.000, 0.000, 0.000);   // black  (cold)
    const vec3 c1 = vec3(0.110, 0.040, 0.320);   // deep violet
    const vec3 c2 = vec3(0.520, 0.022, 0.190);   // dark crimson
    const vec3 c3 = vec3(0.920, 0.215, 0.020);   // orange
    const vec3 c4 = vec3(1.000, 0.840, 0.060);   // yellow
    const vec3 c5 = vec3(1.000, 1.000, 1.000);   // white  (hot)

    float s = t * 5.0;
    int   i = int(s);
    float f = fract(s);

    if (i == 0) return mix(c0, c1, f);
    if (i == 1) return mix(c1, c2, f);
    if (i == 2) return mix(c2, c3, f);
    if (i == 3) return mix(c3, c4, f);
    return      mix(c4, c5, f);
}

float sCurveMap(float t) {
    return t * t * (3.0 - 2.0 * t);
}

// --- Scanline / sensor grid artifact -----------------------------------------
float sensorGrid(vec2 uv) {
    float h = mod(floor(uv.y * 0.5 + time * 0.3), 2.0);
    return 1.0 - h * 0.025;
}

void main() {
    vec4 raw = texture(tex0, vTexCoord);

    // Rec.709 luminance from color input
    float luma = dot(raw.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Tonal controls (match bw.frag pipeline)
    luma = pow(clamp(luma, 0.001, 1.0), gamma);
    luma = clamp((luma - 0.5) * contrast + 0.5 + brightness, 0.0, 1.0);
    luma = mix(luma, sCurveMap(luma), sCurve);

    // Sensor grid artifact
    luma *= sensorGrid(vTexCoord);

    // Subtle sensor noise
    float n = fract(sin(dot(vTexCoord, vec2(127.1, 311.7))) * 43758.5453 + time * 3.7) * 0.014 - 0.007;
    luma = clamp(luma + n, 0.0, 1.0);

    vec3 color = ironbow(luma);

    fragColor = vec4(color, 1.0);
}

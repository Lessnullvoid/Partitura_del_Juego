#version 410

uniform sampler2DRect tex0;

// -- Tonal controls --
uniform float brightness;     // -0.5 .. 0.5     default 0.0
uniform float contrast;       // 0.5 .. 3.0      default 1.3
uniform float gamma;          // 0.3 .. 2.2      default 0.95
uniform float sCurve;         // 0 .. 1          default 0.55  (film S-curve)

// -- Analog texture --
uniform float grain;          // 0 .. 0.15       default 0.035
uniform float vignette;       // 0 .. 1.0        default 0.32
uniform float time;           // elapsed seconds — animates grain

// -- Stylised tools (off by default) --
uniform float threshold;      // 0 = off, >0 = hard B&W cut
uniform float posterizeLevels;// ≥48 = off, <48 = posterise

in vec2 vTexCoord;
out vec4 fragColor;

// --- Helpers ---
float noise(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Hermite S-curve: lifts shadows slightly, compresses highlights
float sCurveMap(float t) {
    return t * t * (3.0 - 2.0 * t);
}

void main() {
    vec4 col = texture(tex0, vTexCoord);

    // Rec.709 luma (perceptually accurate)
    float luma = dot(col.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Gamma
    luma = pow(clamp(luma, 0.001, 1.0), gamma);

    // Contrast + brightness
    luma = clamp((luma - 0.5) * contrast + 0.5 + brightness, 0.0, 1.0);

    // Film tonal curve — blends linear with hermite to give rich midtones
    luma = mix(luma, sCurveMap(luma), sCurve);

    // Vignette — elliptical, wider horizontally to match portrait aspect
    if (vignette > 0.001) {
        ivec2 sz = textureSize(tex0);
        vec2  uv = vTexCoord / vec2(sz) * 2.0 - 1.0;  // -1..1 normalised
        uv.x    *= 0.65;  // elliptical: gentler on sides than top/bottom
        float v  = 1.0 - dot(uv, uv) * vignette;
        luma    *= clamp(v, 0.0, 1.0);
    }

    // Posterise — only fires when levels is kept low intentionally
    if (posterizeLevels < 48.0) {
        float lvl = max(posterizeLevels, 2.0);
        luma = floor(luma * lvl) / (lvl - 1.0);
    }

    // Hard threshold (graphic-score mode) — off when threshold ≤ 0.02
    if (threshold > 0.02) {
        luma = smoothstep(threshold - 0.06, threshold + 0.06, luma);
    }

    // Animated film grain — different pattern each frame
    if (grain > 0.001) {
        float g = (noise(vTexCoord * 0.41 + vec2(time * 17.3, time * 9.1)) - 0.5) * grain;
        luma = clamp(luma + g, 0.0, 1.0);
    }

    fragColor = vec4(vec3(luma), 1.0);
}

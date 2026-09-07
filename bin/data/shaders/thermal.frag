#version 410

// Mapa de color de cámara térmica (Ironbow / FLIR).
// Entrada: textura de vídeo en color sin procesar (igual que la entrada de bw.frag).
// Calcula la luminancia Rec.709 internamente, aplica controles tonales y luego el mapa de color.

uniform sampler2DRect tex0;
uniform float time;

// Controles tonales (mismos valores por defecto que bw.frag)
uniform float brightness;      // -0.5 .. 0.5     valor por defecto 0.0
uniform float contrast;        // 0.5 .. 3.0      valor por defecto 1.3
uniform float gamma;           // 0.3 .. 2.2      valor por defecto 0.95
uniform float sCurve;          // 0 .. 1          valor por defecto 0.55

in  vec2 vTexCoord;
out vec4 fragColor;

// --- Paleta Ironbow ----------------------------------------------------------
// FLIR Ironbow: negro → violeta profundo → carmesí oscuro → naranja → amarillo → blanco
vec3 ironbow(float t) {
    t = clamp(t, 0.0, 1.0);

    const vec3 c0 = vec3(0.000, 0.000, 0.000);   // negro  (frío)
    const vec3 c1 = vec3(0.110, 0.040, 0.320);   // violeta profundo
    const vec3 c2 = vec3(0.520, 0.022, 0.190);   // carmesí oscuro
    const vec3 c3 = vec3(0.920, 0.215, 0.020);   // naranja
    const vec3 c4 = vec3(1.000, 0.840, 0.060);   // amarillo
    const vec3 c5 = vec3(1.000, 1.000, 1.000);   // blanco  (caliente)

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

// --- Artefacto de scanline / rejilla de sensor -----------------------------------------
float sensorGrid(vec2 uv) {
    float h = mod(floor(uv.y * 0.5 + time * 0.3), 2.0);
    return 1.0 - h * 0.025;
}

void main() {
    vec4 raw = texture(tex0, vTexCoord);

    // Luminancia Rec.709 a partir de la entrada en color
    float luma = dot(raw.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Controles tonales (coinciden con el pipeline de bw.frag)
    luma = pow(clamp(luma, 0.001, 1.0), gamma);
    luma = clamp((luma - 0.5) * contrast + 0.5 + brightness, 0.0, 1.0);
    luma = mix(luma, sCurveMap(luma), sCurve);

    // Artefacto de rejilla de sensor
    luma *= sensorGrid(vTexCoord);

    // Ruido sutil de sensor
    float n = fract(sin(dot(vTexCoord, vec2(127.1, 311.7))) * 43758.5453 + time * 3.7) * 0.014 - 0.007;
    luma = clamp(luma + n, 0.0, 1.0);

    vec3 color = ironbow(luma);

    fragColor = vec4(color, 1.0);
}

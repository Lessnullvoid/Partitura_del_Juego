#version 410

uniform sampler2DRect tex0;

// -- Controles tonales --
uniform float brightness;     // -0.5 .. 0.5     valor por defecto 0.0
uniform float contrast;       // 0.5 .. 3.0      valor por defecto 1.3
uniform float gamma;          // 0.3 .. 2.2      valor por defecto 0.95
uniform float sCurve;         // 0 .. 1          valor por defecto 0.55  (curva S fílmica)

// -- Textura analógica --
uniform float grain;          // 0 .. 0.15       valor por defecto 0.035
uniform float vignette;       // 0 .. 1.0        valor por defecto 0.32
uniform float time;           // segundos transcurridos — anima el grano

// -- Herramientas estilizadas (desactivadas por defecto) --
uniform float threshold;      // 0 = desactivado, >0 = corte duro B&N
uniform float posterizeLevels;// ≥48 = desactivado, <48 = posteriza

in vec2 vTexCoord;
out vec4 fragColor;

// --- Auxiliares ---
float noise(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Curva S de Hermite: realza ligeramente las sombras, comprime las altas luces
float sCurveMap(float t) {
    return t * t * (3.0 - 2.0 * t);
}

void main() {
    vec4 col = texture(tex0, vTexCoord);

    // Luma Rec.709 (perceptualmente precisa)
    float luma = dot(col.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Gamma
    luma = pow(clamp(luma, 0.001, 1.0), gamma);

    // Contraste + brillo
    luma = clamp((luma - 0.5) * contrast + 0.5 + brightness, 0.0, 1.0);

    // Curva tonal fílmica — mezcla lineal con hermite para obtener medios tonos ricos
    luma = mix(luma, sCurveMap(luma), sCurve);

    // Viñeta — elíptica, más ancha en horizontal para coincidir con el formato retrato
    if (vignette > 0.001) {
        ivec2 sz = textureSize(tex0);
        vec2  uv = vTexCoord / vec2(sz) * 2.0 - 1.0;  // -1..1 normalizado
        uv.x    *= 0.65;  // elíptica: más suave en los laterales que arriba/abajo
        float v  = 1.0 - dot(uv, uv) * vignette;
        luma    *= clamp(v, 0.0, 1.0);
    }

    // Posteriza — solo se activa cuando los niveles se mantienen bajos a propósito
    if (posterizeLevels < 48.0) {
        float lvl = max(posterizeLevels, 2.0);
        luma = floor(luma * lvl) / (lvl - 1.0);
    }

    // Umbral duro (modo partitura gráfica) — desactivado cuando threshold ≤ 0.02
    if (threshold > 0.02) {
        luma = smoothstep(threshold - 0.06, threshold + 0.06, luma);
    }

    // Grano de película animado — patrón distinto en cada fotograma
    if (grain > 0.001) {
        float g = (noise(vTexCoord * 0.41 + vec2(time * 17.3, time * 9.1)) - 0.5) * grain;
        luma = clamp(luma + g, 0.0, 1.0);
    }

    fragColor = vec4(vec3(luma), 1.0);
}

#version 410

// Generador de ruido analógico: ruido procedural multicapa a pantalla completa.
//
// Capas de diseño (de baja a alta frecuencia):
//   1. Bandas de interferencia – ondas horizontales lentas de luminancia (UV global,
//                            continuas a través de todas las ventanas de canal)
//   2. VHS jitter          – desplazamiento horizontal por scanline
//   3. Grano fino          – nieve de TV por píxel de cambio rápido
//   4. Pérdida de señal    – franjas horizontales aleatorias claras/oscuras
//   5. CRT scanlines       – oscurecimiento de filas alternas
//
// UV global: gx = (uv.x + channelIndex) / channelCount proyecta el rango X
// local [0,1] de cada canal sobre su porción del muro completo, de modo que
// las formas a gran escala (capas 1-2) aparecen continuas en la instalación física.

uniform vec2  resolution;     // dimensiones en píxeles del FBO
uniform float time;           // tiempo transcurrido en segundos (reloj de pared, igual para todos los canales)
uniform float intensity;      // 0-1  cuán denso/brillante es el ruido
uniform float density;        // 0-1  0 = sobre todo bandas, 1 = sobre todo grano
uniform float envelope;       // 0-1  fundido de capítulo (Aparición ... Disolución)
uniform int   channelIndex;   // índice basado en 0 de este canal en el muro
uniform int   channelCount;   // número total de canales

out vec4 fragColor;

// ============================================================
//  Primitivas hash
// ============================================================

// Hash de Wang (aritmética entera; sin problemas de precisión en coma flotante)
uint wangHash(uint s) {
    s ^= s >> 16u;
    s *= 0x45d9f3bu;
    s ^= s >> 16u;
    return s;
}

// Produce [0, 1) a partir de una coordenada de píxel 2D + semilla uint.
float pixelRand(vec2 fc, uint seed) {
    uint ix = uint(max(fc.x, 0.0));
    uint iy = uint(max(fc.y, 0.0));
    uint h  = wangHash(ix * 1640531513u ^ iy * 2246822519u ^ seed);
    return float(h) * (1.0 / 4294967296.0);
}

// Hash en dominio de coma flotante (usado para la retícula de ruido suave)
float fhash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// ============================================================
//  Value noise 2D suave + FBM
// ============================================================

float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);   // curva smoothstep
    return mix(
        mix(fhash(i),              fhash(i + vec2(1.0, 0.0)), u.x),
        mix(fhash(i + vec2(0.0, 1.0)), fhash(i + vec2(1.0, 1.0)), u.x),
        u.y
    );
}

// FBM de 4 octavas: usado para patrones de interferencia a gran escala.
float fbm4(vec2 p) {
    float v  = 0.5000 * vnoise(p);
          v += 0.2500 * vnoise(p * 2.03  + vec2(5.20, 1.30));
          v += 0.1250 * vnoise(p * 4.11  + vec2(2.80, 8.50));
          v += 0.0625 * vnoise(p * 8.21  + vec2(7.60, 3.20));
    return v * (1.0 / 0.9375);   // normaliza la suma a ~[0, 1]
}

// ============================================================
//  main
// ============================================================

void main() {
    // UV normalizado: (0,0) = esquina superior izquierda del FBO, (1,1) = inferior derecha.
    // El origen de gl_FragCoord es inferior izquierda, así que se invierte Y.
    vec2 pix = gl_FragCoord.xy;
    vec2 uv  = vec2(pix.x, resolution.y - pix.y) / resolution;

    // --- Coordenada global del muro -----------------------------------------
    // Proyecta la X local [0, 1] de cada canal sobre su porción única del muro.
    int   nc = max(1, channelCount);
    float gx = (uv.x + float(channelIndex)) / float(nc);

    // --- Anclas temporales (cuantizadas para simular cadencias analógicas) ----------
    float bandT = time * 0.055;                  // velocidad lenta de deriva de las bandas
    float jT8   = floor(time * 8.0)  * 0.125;   // tick de jitter a 8 fps
    float gT24  = floor(time * 24.0);            // semilla de grano a 24 fps

    // ---- Capa 1: Bandas de interferencia (global, entre ventanas) --------------
    // FBM muestreado en el espacio horizontal global para que los patrones cubran todo el muro.
    float bx    = gx * 3.5 + bandT * 0.4;
    float by    = uv.y * 1.8 + bandT;
    float bands = fbm4(vec2(bx, by));
    bands = pow(clamp(bands, 0.0, 1.0), 1.6);   // aumenta el contraste

    // ---- Capa 2: jitter de scanline VHS ------------------------------------
    // Cada fila se desplaza en X una cantidad aleatoria que varía lentamente en el tiempo.
    float rowF    = uv.y * resolution.y;
    float scanRow = floor(rowF);

    float jAmt    = mix(0.018, 0.065, intensity);
    float jSlow   = (vnoise(vec2(scanRow * 0.03, time * 1.3)) - 0.5) * jAmt;
    float jFast   = (vnoise(vec2(scanRow * 0.20, time * 11.0)) - 0.5) * jAmt * 0.35;

    // Glitch duro a nivel de fila: una pequeña fracción de filas recibe un desplazamiento grande.
    uint  rowSeed = wangHash(uint(scanRow) ^ uint(jT8 * 5003.0));
    float hardFrac = float(rowSeed) * (1.0 / 4294967296.0);
    float jHard   = (vnoise(vec2(scanRow * 0.7, time * 25.0)) - 0.5)
                    * jAmt * 2.5 * step(0.93, hardFrac);

    float jitter = jSlow + jFast + jHard;

    // Se vuelven a muestrear las bandas en la X global desplazada por el jitter (crea el
    // aspecto característico de «columna inclinada» de los errores de tracking analógico).
    float jGx    = clamp(gx + jitter, 0.0, 1.0);
    float jBands = fbm4(vec2(jGx * 3.5 + bandT * 0.4, uv.y * 1.8 + bandT));
    jBands = pow(clamp(jBands, 0.0, 1.0), 1.6);

    // ---- Capa 3: Grano (nieve de TV por píxel) ----------------------------
    uint frameSeed  = uint(gT24) * 7919u;
    float grain     = pixelRand(pix,          frameSeed);
    float coarse    = pixelRand(floor(pix * 0.5), frameSeed ^ 0x55aau);

    // ---- Capa 4a: Bandas gruesas de pérdida de señal ----------------------------------
    // Franjas horizontales claras aleatorias a ~40 filas de resolución.
    float dropRow  = floor(uv.y * 42.0 + time * 5.2);
    uint  dropHash = wangHash(uint(dropRow) * 31u ^ uint(jT8 * 7013.0));
    float dropout  = step(0.94, float(dropHash) * (1.0 / 4294967296.0));

    // ---- Capa 4b: Pérdidas finas de una sola scanline -------------------------
    uint  fineHash    = wangHash(uint(scanRow) * 53u ^ uint(time * 22.0) * 3u);
    float dropoutFine = step(0.978, float(fineHash) * (1.0 / 4294967296.0)) * 0.55;

    // ---- Capa 5: Oscurecimiento horizontal de scanlines CRT ----------------------
    float scanline = 1.0 - 0.16 * step(0.5, fract(rowF * 0.5));

    // ---- Capa 6: Roll vertical lento (artefacto de pérdida de sincronía) ----------------
    // Aparece con poca frecuencia; cuando está activo muestra una banda horizontal clara que
    // trepa hacia arriba, simulando la pérdida de la señal de sincronía vertical.
    float rollT   = floor(time * 0.4);
    uint  rollSeed = wangHash(uint(rollT) * 137u);
    float rollOn  = step(0.75, float(rollSeed) * (1.0 / 4294967296.0));
    float rollY   = fract(uv.y - time * 0.28 + float(rollSeed) * (1.0 / 4294967296.0) * 0.6);
    float roll    = smoothstep(0.06, 0.0, rollY) * 0.7 * rollOn;

    // ---- Composición -------------------------------------------------------
    // El parámetro density interpola entre «bandas» (0) y «grano» (1).
    float grainMix = clamp(density * 0.65, 0.0, 0.65);
    float base     = mix(jBands, grain * 0.85 + coarse * 0.15, grainMix);
    base = base + dropout * 0.68 + dropoutFine + roll;
    base *= scanline;

    // Aplica intensity y envelope.
    float level = base * (0.38 + intensity * 0.62) * envelope;
    level = clamp(level, 0.0, 1.0);

    fragColor = vec4(vec3(level), 1.0);
}

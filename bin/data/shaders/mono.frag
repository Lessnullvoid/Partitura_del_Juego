#version 410

// Puerta final de escala de grises para el búfer de la partitura. Toda ruta que llega a las
// ventanas de presentación a través de GraphicScore pasa por aquí, de modo que no sobrevive
// croma de fotogramas de vídeo, rampas térmicas ni paletas heredadas.
//
// u_blackLevel > 0: corrección de niveles aplicada antes de la inversión de fusión
// (applyPolarity) del código llamante. Aplasta el resplandor de fondo por debajo de blackLevel
// a negro puro para que la inversión posterior produzca un blanco verdadero 255,255,255.
// El renderizado normal usa u_blackLevel = 0.0 (sin corrección).

uniform sampler2DRect tex0;
uniform float u_blackLevel;  // 0.0 = desactivado; ~0.07 recomendado para la inversión del generador

in vec2 vTexCoord;
out vec4 fragColor;

void main() {
    vec4 source = texture(tex0, vTexCoord);
    float luma = dot(source.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Corrección de niveles: reasigna linealmente [blackLevel, 1] a [0, 1].
    // Los valores iguales o inferiores a blackLevel se recortan a 0 (negro puro).
    if (u_blackLevel > 0.001) {
        luma = clamp((luma - u_blackLevel) / (1.0 - u_blackLevel), 0.0, 1.0);
    }

    fragColor = vec4(vec3(luma), source.a);
}

#version 410

// Shader de vértices mínimo de paso directo para el quad a pantalla completa del ruido analógico.
// El shader de fragmentos lo calcula todo a partir de gl_FragCoord, así que no hay que
// reenviar coordenadas de textura.

uniform mat4 modelViewProjectionMatrix;

in vec4 position;

void main() {
    gl_Position = modelViewProjectionMatrix * position;
}

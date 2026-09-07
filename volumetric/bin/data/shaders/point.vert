#version 150

uniform mat4 modelViewProjectionMatrix;
uniform mat4 modelViewMatrix;
uniform float pointSize;

in vec4 position;
in vec4 color;

out vec4 vColor;

void main() {
    vColor = color;
    vec4 viewPos = modelViewMatrix * position;
    float dist = max(0.1, length(viewPos.xyz));
    gl_PointSize = clamp(pointSize * (5.5 / dist), 1.5, 48.0);
    gl_Position = modelViewProjectionMatrix * position;
}

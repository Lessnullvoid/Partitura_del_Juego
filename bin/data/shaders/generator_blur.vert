#version 410

uniform mat4 modelViewProjectionMatrix;

in vec4 position;
in vec2 texcoord;
out vec2 vTexcoord;

void main() {
    vTexcoord = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}

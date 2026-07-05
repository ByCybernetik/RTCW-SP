#version 450

layout(set = 0, binding = 0) uniform sampler2D uTex;

layout(location = 0) in vec2 inTC;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = texture(uTex, inTC) * inColor;
}

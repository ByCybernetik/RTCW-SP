#version 450

layout(push_constant) uniform PC {
    mat4 mvp;
} pc;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inTC;
layout(location = 4) in vec4 inColor;

layout(location = 0) out vec2 outTC;
layout(location = 1) out vec4 outColor;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    outTC = inTC;
    outColor = inColor;
}

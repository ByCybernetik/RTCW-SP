#version 450

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 params0;
    vec4 params1;
    vec4 params2;
    vec4 params3;
    vec4 params4;
    vec4 params5;
    vec4 params6;
    vec4 params7;
    vec4 params8;
    vec4 params9;
    vec4 params10;
    vec4 params11;
    vec4 params12;
    vec4 params13;
    vec4 params14;
    vec4 params15;
} pc;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec2 inLightmapCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inColor;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec2 vLightmapCoord;
layout(location = 2) out vec4 vColor;
layout(location = 3) out vec3 vWorldPos;
layout(location = 4) out float vNormalZFadeAlpha;

float evalWave(float waveFunc, float x) {
    float t = fract(x);
    int func = int(waveFunc + 0.5);
    if (func == 1) {
        return (t < 0.5) ? (t * 4.0 - 1.0) : (3.0 - t * 4.0);
    }
    if (func == 2) {
        return t * 2.0 - 1.0;
    }
    if (func == 3) {
        return 1.0 - t * 2.0;
    }
    return sin(x * 6.28318530718);
}

float q3SinTable(float index) {
    int i = int(index * 1024.0) & 1023;
    return sin(float(i) * 6.28318530718 / 1023.0);
}

vec2 applyTcMod(vec2 uv, vec3 worldPos) {
    vec2 outUv = uv;

    if (pc.params4.z > 0.5) {
        outUv = vec2(
            outUv.x * pc.params3.x + outUv.y * pc.params3.y,
            outUv.x * pc.params3.z + outUv.y * pc.params3.w
        ) + pc.params4.xy;
    }

    outUv *= pc.params6.xy;

    vec2 scrollOffset = pc.params6.zw * pc.params8.y;
    scrollOffset -= floor(scrollOffset);
    outUv += scrollOffset;

    if (abs(pc.params8.x) > 0.001) {
        float angle = radians(pc.params8.x * pc.params8.y);
        float c = cos(angle);
        float s = sin(angle);
        vec2 p = outUv - vec2(0.5);
        outUv = vec2(p.x * c - p.y * s, p.x * s + p.y * c) + vec2(0.5);
    }

    if (pc.params4.w > 0.5) {
        float now = pc.params5.z + pc.params8.y * pc.params5.w;
        float amp = pc.params5.y;
        float turbS = (worldPos.x + worldPos.z) * (1.0 / 128.0) * 0.125 + now;
        float turbT = worldPos.y * (1.0 / 128.0) * 0.125 + now;
        outUv.s += q3SinTable(turbS) * amp;
        outUv.t += q3SinTable(turbT) * amp;
    }

    if (pc.params11.y > 0.5) {
        float w = pc.params10.y + pc.params10.z * evalWave(pc.params10.x, pc.params10.w + pc.params8.y * pc.params11.x);
        w = max(abs(w), 0.001);
        outUv = (outUv - vec2(0.5)) / w + vec2(0.5);
    }

    return outUv;
}

void main() {
    vec2 uv = (pc.params8.z > 0.5) ? inLightmapCoord : inTexCoord;
    vec3 pos = inPos;

    if (pc.params11.w > 0.5 && pc.params12.y > 0.5) {
        float radiusWorld = 4096.0;
        float h = max(pc.params12.x, 1.0);
        /* inPos includes the view origin; the cloud formula needs the direction from the viewer. */
        vec3 skyVec = inPos - pc.params14.xyz;
        float d2 = dot(skyVec, skyVec);
        if (d2 > 0.001) {
            float z = skyVec.z;
            float x2 = skyVec.x * skyVec.x;
            float y2 = skyVec.y * skyVec.y;
            float z2 = z * z;
            float inside = z2 * radiusWorld * radiusWorld +
                           2.0 * x2 * radiusWorld * h + x2 * h * h +
                           2.0 * y2 * radiusWorld * h + y2 * h * h +
                           2.0 * z2 * radiusWorld * h + z2 * h * h;
            float p = (1.0 / (2.0 * d2)) * (-2.0 * z * radiusWorld + 2.0 * sqrt(max(inside, 0.0)));
            vec3 v = normalize(skyVec * p + vec3(0.0, 0.0, radiusWorld));
            uv = vec2(acos(clamp(v.x, -1.0, 1.0)), acos(clamp(v.y, -1.0, 1.0)));
        }
    }

    if (pc.params11.w < 0.5 && pc.params2.w > 0.5 && pc.params2.z > 0.5) {
        float waveDiv = max(pc.params1.x, 1.0);
        float dist = length(inPos);
        float wave = evalWave(pc.params1.y, pc.params2.x + pc.params8.y * pc.params2.y + dist / waveDiv);
        float offset = pc.params1.z + pc.params1.w * wave;
        pos += normalize(inNormal) * offset;
    }

    gl_Position = pc.mvp * vec4(pos, 1.0);
    if (pc.params11.w > 0.5) {
        gl_Position.z = gl_Position.w;
    }
    if (pc.params7.w > 0.5) {
        vec3 n = normalize(inNormal);
        vec3 viewer = normalize(pc.params14.xyz - pos);
        float d = dot(n, viewer);
        vec3 reflected = n * 2.0 * d - viewer;
        uv = vec2(0.5 + reflected.y * 0.5, 0.5 - reflected.z * 0.5);
    }
    vTexCoord = (pc.params14.w > 0.1 && pc.params14.w < 0.5) ? uv : applyTcMod(uv, pos);
    vLightmapCoord = inLightmapCoord;
    vColor = inColor;
    vWorldPos = pos;

    float normalZFadeAlpha = 1.0;
    if (pc.params15.w > 0.0) {
        vec3 worldUp = normalize(pc.params15.xyz);
        float d = dot(normalize(inNormal), worldUp);
        float lowest = pc.params12.x;
        float highest = pc.params12.y;
        if (d < highest && d > lowest) {
            float range = highest - lowest;
            float mid = lowest + range * 0.5;
            if (d < mid) {
                normalZFadeAlpha = (d - lowest) / (range * 0.5);
            } else {
                normalZFadeAlpha = 1.0 - (d - mid) / (range * 0.5);
            }
        } else {
            normalZFadeAlpha = 0.0;
        }
    }
    vNormalZFadeAlpha = normalZFadeAlpha;
}

#version 450

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout(set = 1, binding = 0) uniform UBO {
    vec4 params[21];
} ubo;

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
layout(location = 5) out float vFogFactor;

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

    if (ubo.params[4].z > 0.5) {
        outUv = vec2(
            outUv.x * ubo.params[3].x + outUv.y * ubo.params[3].y,
            outUv.x * ubo.params[3].z + outUv.y * ubo.params[3].w
        ) + ubo.params[4].xy;
    }

    outUv *= ubo.params[6].xy;

    vec2 scrollOffset = ubo.params[6].zw * ubo.params[8].y;
    scrollOffset -= floor(scrollOffset);
    outUv += scrollOffset;

    if (abs(ubo.params[8].x) > 0.001) {
        float angle = radians(ubo.params[8].x * ubo.params[8].y);
        float c = cos(angle);
        float s = sin(angle);
        vec2 p = outUv - vec2(0.5);
        outUv = vec2(p.x * c - p.y * s, p.x * s + p.y * c) + vec2(0.5);
    }

    if (ubo.params[4].w > 0.5) {
        float now = ubo.params[5].z + ubo.params[8].y * ubo.params[5].w;
        float amp = ubo.params[5].y;
        float turbS = (worldPos.x + worldPos.z) * (1.0 / 128.0) * 0.125 + now;
        float turbT = worldPos.y * (1.0 / 128.0) * 0.125 + now;
        outUv.s += q3SinTable(turbS) * amp;
        outUv.t += q3SinTable(turbT) * amp;
    }

    if (ubo.params[11].y > 0.5) {
        float w = ubo.params[10].y + ubo.params[10].z * evalWave(ubo.params[10].x, ubo.params[10].w + ubo.params[8].y * ubo.params[11].x);
        w = max(abs(w), 0.001);
        outUv = (outUv - vec2(0.5)) / w + vec2(0.5);
    }

    return outUv;
}

void main() {
    vec2 uv = (ubo.params[8].z > 0.5) ? inLightmapCoord : inTexCoord;
    vec3 pos = inPos;

    if (ubo.params[11].w > 0.5 && ubo.params[12].y > 0.5) {
        float radiusWorld = 4096.0;
        float h = max(ubo.params[12].x, 1.0);
        /* inPos includes the view origin; the cloud formula needs the direction from the viewer. */
        vec3 skyVec = inPos - ubo.params[14].xyz;
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

    if (ubo.params[11].w < 0.5 && ubo.params[2].w > 0.5 && ubo.params[2].z > 0.5 && ubo.params[2].y >= 0.0) {
        float waveDiv = max(ubo.params[1].x, 1.0);
        float dist = length(inPos);
        float wave = evalWave(ubo.params[1].y, ubo.params[2].x + ubo.params[8].y * ubo.params[2].y + dist / waveDiv);
        float offset = ubo.params[1].z + ubo.params[1].w * wave;
        pos += normalize(inNormal) * offset;
    }

    /* Negative-frequency wave deformation: used by entityOnFire shaders.
     * In OpenGL this deforms vertices along the fire-rise direction, scaled by
     * the dot product with the normal.  params[2][1] holds the signed frequency,
     * params[2][3] holds the world-space Z scale factor, and params[15].xyz
     * holds the model-space fire-rise direction (already normalised for
     * alphaGen normalzfade). */
    if (ubo.params[11].w < 0.5 && ubo.params[2].y < 0.0 && ubo.params[2].w > 0.0) {
        bool inverse = false;
        float freq = -ubo.params[2].y;
        if (freq > 999.0) {
            inverse = true;
            freq -= 999.0;
        }

        float off = (inPos.x + inPos.y + inPos.z) * ubo.params[1].x;
        float wave = evalWave(ubo.params[1].y, ubo.params[2].x + ubo.params[8].y * freq + off);
        float scale = ubo.params[1].z + ubo.params[1].w * wave;

        vec3 up = ubo.params[15].xyz * ubo.params[2].w;
        float upLen = length(up);
        if (upLen > 0.001) {
            up /= upLen;
            float d = dot(up, normalize(inNormal));
            if (d * scale > 0.0) {
                if (inverse) scale = -scale;
                pos += up * d * scale;
            }
        }
    }

    gl_Position = pc.mvp * vec4(pos, 1.0);
    if (ubo.params[11].w > 0.5) {
        gl_Position.z = gl_Position.w;
    }
    if (ubo.params[7].w > 0.5) {
        vec3 n = normalize(inNormal);
        vec3 viewer = normalize(ubo.params[14].xyz - pos);
        float d = dot(n, viewer);
        vec3 reflected = n * 2.0 * d - viewer;
        uv = vec2(0.5 + reflected.y * 0.5, 0.5 - reflected.z * 0.5);
    }
    vec2 finalUv;
    vec4 finalColor;
    if (ubo.params[16].w > 3.5) {
        /* Volumetric fog pass: compute fog texture coordinates exactly like
         * RB_CalcFogTexCoords and use the fog color as the vertex color. */
        float s = dot(pos, ubo.params[18].xyz) + ubo.params[18].w;
        float eyeT = dot(ubo.params[14].xyz, ubo.params[19].xyz) + ubo.params[19].w;
        float t = dot(pos, ubo.params[19].xyz) + ubo.params[19].w;
        bool eyeOutside = eyeT < 0.0;
        if (eyeOutside) {
            if (t < 1.0) {
                t = 1.0 / 32.0;
            } else {
                t = 1.0 / 32.0 + 30.0 / 32.0 * t / (t - eyeT);
            }
        } else {
            if (t < 0.0) {
                t = 1.0 / 32.0;
            } else {
                t = 31.0 / 32.0;
            }
        }
        finalUv = vec2(s, t);
        finalColor = vec4(ubo.params[16].xyz, 1.0);
    } else {
        finalUv = (ubo.params[14].w > 0.1 && ubo.params[14].w < 0.5) ? uv : applyTcMod(uv, pos);
        finalColor = inColor;
    }
    vTexCoord = finalUv;
    vLightmapCoord = inLightmapCoord;
    vColor = finalColor;
    vWorldPos = pos;

    /* Distance fog factor computed per-vertex to match OpenGL's per-vertex fog
     * coordinate. The scalar is perspective-correct interpolated, just like GL_FOG.
     * params16.w holds the distance-fog mode (1=linear, 2=exp, 3=exp2); keep at
     * 1.0 when distance fog is off or during the volumetric fog pass
     * (params16.w == 4).
     * params20.xyz is the eye-space forward axis and .w selects the NV fog
     * distance metric (0=GL_EYE_RADIAL_NV, 1=GL_EYE_PLANE_ABSOLUTE_NV,
     * 2=GL_EYE_PLANE). */
    vFogFactor = 1.0;
    if (ubo.params[17].w > 0.5 && ubo.params[16].w > 0.0 && ubo.params[16].w < 4.0) {
        vec3 eyeVec = pos - ubo.params[14].xyz;
        float dist;
        int distMode = int(ubo.params[20].w + 0.5);
        if (distMode == 1) {
            dist = abs(dot(eyeVec, ubo.params[20].xyz));
        } else if (distMode == 2) {
            dist = dot(eyeVec, ubo.params[20].xyz);
        } else {
            dist = length(eyeVec);
        }
        if (ubo.params[16].w < 1.5) {
            /* GL_LINEAR */
            vFogFactor = clamp((ubo.params[17].y - dist) / (ubo.params[17].y - ubo.params[17].x), 0.0, 1.0);
        } else if (ubo.params[16].w < 2.5) {
            /* GL_EXP */
            vFogFactor = exp(-ubo.params[17].z * dist);
        } else {
            /* GL_EXP2 */
            float d = ubo.params[17].z * dist;
            vFogFactor = exp(-d * d);
        }
    }

    float normalZFadeAlpha = 1.0;
    if (ubo.params[15].w > 0.0) {
        vec3 worldUp = normalize(ubo.params[15].xyz);
        float d = dot(normalize(inNormal), worldUp);
        float lowest = ubo.params[12].x;
        float highest = ubo.params[12].y;
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

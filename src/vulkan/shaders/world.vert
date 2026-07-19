#version 450

layout(push_constant) uniform PushConstants {
    uint mvpIndex;
    uint drawParamIndex;
    uint boneSet;       /* pad[0]: MDS bone set (bind offset); unused in VS */
    uint meshShortMode; /* pad[1]: 1 = MD3 shorts in loc 9/10 */
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
} pc;

layout(set = 1, binding = 0) uniform ViewUBO {
    mat4 mvps[256];
    /* Per-draw slots: drawParamIndex * 9 + {0..8}.
     * Sized for VK_VIEW_MAX_DRAW_PARAMS (2048) slots. */
    vec4 drawParams[18432];
} view;

/* One MDS bone set (128 bones × 3 vec4 mat3x4), selected via dynamic offset. */
layout(set = 1, binding = 1) uniform BoneUBO {
    vec4 bones[384];
} boneUbo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec2 inLightmapCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inColor;
/* Binding 1: MD3 old frame (a.xyz/b.xyz) or MDS weights (a..d = off+w). */
layout(location = 5) in vec4 inMeshA;
layout(location = 6) in vec4 inMeshB;
layout(location = 7) in vec4 inMeshC;
layout(location = 8) in vec4 inMeshD;
/* MD3 device-local shorts (bindings 2/3). */
layout(location = 9) in ivec4 inShortNew;
layout(location = 10) in ivec4 inShortOld;

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
    vec3 nrm = inNormal;

    /* MD3 shorts: decode lat/long normals matching tr.sinTable (1024 entries). */
    if (pc.meshShortMode != 0u) {
        float scale = 1.0 / 64.0;
        vec3 posNew = vec3(float(inShortNew.x), float(inShortNew.y), float(inShortNew.z)) * scale;
        int lat = (inShortNew.w >> 8) & 255;
        int lng = inShortNew.w & 255;
        lat *= 4;
        lng *= 4;
        float sLat = sin(float(lat & 1023) * 6.28318530718 / 1023.0);
        float cLat = sin(float((lat + 256) & 1023) * 6.28318530718 / 1023.0);
        float sLng = sin(float(lng & 1023) * 6.28318530718 / 1023.0);
        float cLng = sin(float((lng + 256) & 1023) * 6.28318530718 / 1023.0);
        vec3 nrmNew = vec3(cLat * sLng, sLat * sLng, cLng);

        float meshBacklerp = view.drawParams[pc.drawParamIndex * 9u + 8u].w;
        if (meshBacklerp > 0.001) {
            vec3 posOld = vec3(float(inShortOld.x), float(inShortOld.y), float(inShortOld.z)) * scale;
            int latO = (inShortOld.w >> 8) & 255;
            int lngO = inShortOld.w & 255;
            latO *= 4;
            lngO *= 4;
            float sLatO = sin(float(latO & 1023) * 6.28318530718 / 1023.0);
            float cLatO = sin(float((latO + 256) & 1023) * 6.28318530718 / 1023.0);
            float sLngO = sin(float(lngO & 1023) * 6.28318530718 / 1023.0);
            float cLngO = sin(float((lngO + 256) & 1023) * 6.28318530718 / 1023.0);
            vec3 nrmOld = vec3(cLatO * sLngO, sLatO * sLngO, cLngO);
            float newW = 1.0 - meshBacklerp;
            pos = posOld * meshBacklerp + posNew * newW;
            nrm = normalize(nrmOld * meshBacklerp + nrmNew * newW);
        } else {
            pos = posNew;
            nrm = nrmNew;
        }
    } else if (view.drawParams[pc.drawParamIndex * 9u + 7u].w > 0.5) {
        int b0 = int(inLightmapCoord.x + 0.5);
        int b1 = int(inLightmapCoord.y + 0.5);
        int b2 = int(inColor.r * 255.0 + 0.5);
        int b3 = int(inColor.g * 255.0 + 0.5);
        vec3 skinned = vec3(0.0);

        if (inMeshA.w > 0.0) {
            int bi = b0 * 3;
            vec4 r0 = boneUbo.bones[bi];
            vec4 r1 = boneUbo.bones[bi + 1];
            vec4 r2 = boneUbo.bones[bi + 2];
            vec3 o = inMeshA.xyz;
            skinned += inMeshA.w * vec3(
                dot(r0.xyz, o) + r0.w,
                dot(r1.xyz, o) + r1.w,
                dot(r2.xyz, o) + r2.w);
            /* Normal from first weight's bone only (matches CPU). */
            nrm = vec3(dot(r0.xyz, inNormal), dot(r1.xyz, inNormal), dot(r2.xyz, inNormal));
        }
        if (inMeshB.w > 0.0) {
            int bi = b1 * 3;
            vec4 r0 = boneUbo.bones[bi];
            vec4 r1 = boneUbo.bones[bi + 1];
            vec4 r2 = boneUbo.bones[bi + 2];
            vec3 o = inMeshB.xyz;
            skinned += inMeshB.w * vec3(
                dot(r0.xyz, o) + r0.w,
                dot(r1.xyz, o) + r1.w,
                dot(r2.xyz, o) + r2.w);
        }
        if (inMeshC.w > 0.0) {
            int bi = b2 * 3;
            vec4 r0 = boneUbo.bones[bi];
            vec4 r1 = boneUbo.bones[bi + 1];
            vec4 r2 = boneUbo.bones[bi + 2];
            vec3 o = inMeshC.xyz;
            skinned += inMeshC.w * vec3(
                dot(r0.xyz, o) + r0.w,
                dot(r1.xyz, o) + r1.w,
                dot(r2.xyz, o) + r2.w);
        }
        if (inMeshD.w > 0.0) {
            int bi = b3 * 3;
            vec4 r0 = boneUbo.bones[bi];
            vec4 r1 = boneUbo.bones[bi + 1];
            vec4 r2 = boneUbo.bones[bi + 2];
            vec3 o = inMeshD.xyz;
            skinned += inMeshD.w * vec3(
                dot(r0.xyz, o) + r0.w,
                dot(r1.xyz, o) + r1.w,
                dot(r2.xyz, o) + r2.w);
        }
        pos = skinned;
        nrm = normalize(nrm);
    } else {
        /* MD3/MDC frame lerp: old*backlerp + new*(1-backlerp). Slot 8.w. */
        float meshBacklerp = view.drawParams[pc.drawParamIndex * 9u + 8u].w;
        if (meshBacklerp > 0.001) {
            float newW = 1.0 - meshBacklerp;
            vec3 posOld = inMeshA.xyz;
            vec3 nrmOld = inMeshB.xyz;
            pos = posOld * meshBacklerp + inPos * newW;
            nrm = normalize(nrmOld * meshBacklerp + inNormal * newW);
        }
    }

    if (pc.params11.w > 0.5 && pc.params12.z > 0.0) {
        /* Static sky/cloud VBO: unit-cube directions → world positions. */
        pos = inPos * pc.params12.z + pc.params14.xyz;
    }

    if (pc.params11.w > 0.5 && pc.params12.y > 0.5) {
        float radiusWorld = 4096.0;
        float h = max(pc.params12.x, 1.0);
        /* inPos includes the view origin; the cloud formula needs the direction from the viewer. */
        vec3 skyVec = pos - pc.params14.xyz;
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

    if (pc.params11.w < 0.5 && pc.params2.w > 0.5 &&
        pc.params2.z > 0.5 && pc.params2.z < 1.5 && pc.params2.y >= 0.0) {
        /* DEFORM_WAVE: displace along normal. */
        float waveDiv = max(pc.params1.x, 1.0);
        float dist = length(pos);
        float wave = evalWave(pc.params1.y, pc.params2.x + pc.params8.y * pc.params2.y + dist / waveDiv);
        float offset = pc.params1.z + pc.params1.w * wave;
        pos += normalize(nrm) * offset;
    }

    if (pc.params11.w < 0.5 && pc.params2.z > 2.5 && pc.params2.z < 3.5) {
        /* DEFORM_BULGE: sin(st.s * width + time * speed) * height along normal. */
        float scale = sin(uv.x * pc.params1.x + pc.params8.y * pc.params1.y) * pc.params1.w;
        pos += normalize(nrm) * scale;
    }

    if (pc.params11.w < 0.5 && pc.params2.z > 3.5 && pc.params2.z < 4.5) {
        /* DEFORM_MOVE: entire surface along moveVector * wave(time). */
        float w = evalWave(pc.params1.w, pc.params2.x + pc.params8.y * pc.params2.y);
        float scale = pc.params8.w + pc.params2.w * w;
        pos += pc.params1.xyz * scale;
    }

    /* Negative-frequency wave deformation: used by entityOnFire shaders.
     * In OpenGL this deforms vertices along the fire-rise direction, scaled by
     * the dot product with the normal.  params[2][1] holds the signed frequency,
     * params[2][3] holds the world-space Z scale factor, and params[15].xyz
     * holds the model-space fire-rise direction (already normalised for
     * alphaGen normalzfade). */
    if (pc.params11.w < 0.5 && pc.params2.z > 0.5 && pc.params2.z < 1.5 &&
        pc.params2.y < 0.0 && pc.params2.w > 0.0) {
        bool inverse = false;
        float freq = -pc.params2.y;
        if (freq > 999.0) {
            inverse = true;
            freq -= 999.0;
        }

        float off = (pos.x + pos.y + pos.z) * pc.params1.x;
        float wave = evalWave(pc.params1.y, pc.params2.x + pc.params8.y * freq + off);
        float scale = pc.params1.z + pc.params1.w * wave;

        vec3 up = view.drawParams[pc.drawParamIndex * 9u + 0u].xyz * pc.params2.w;
        float upLen = length(up);
        if (upLen > 0.001) {
            up /= upLen;
            float d = dot(up, normalize(nrm));
            if (d * scale > 0.0) {
                if (inverse) scale = -scale;
                pos += up * d * scale;
            }
        }
    }

    gl_Position = view.mvps[pc.mvpIndex] * vec4(pos, 1.0);
    if (pc.params11.w > 0.5) {
        gl_Position.z = gl_Position.w;
    }
    if (pc.params7.w > 0.5) {
        vec3 n = normalize(nrm);
        vec3 viewer = normalize(pc.params14.xyz - pos);
        float d = dot(n, viewer);
        vec3 reflected = n * 2.0 * d - viewer;
        uv = vec2(0.5 + reflected.y * 0.5, 0.5 - reflected.z * 0.5);
    }
    vec2 finalUv;
    vec4 finalColor;
    if (view.drawParams[pc.drawParamIndex * 9u + 1u].w > 3.5) {
        /* Volumetric fog pass: compute fog texture coordinates exactly like
         * RB_CalcFogTexCoords and use the fog color as the vertex color. */
        float s = dot(pos, view.drawParams[pc.drawParamIndex * 9u + 3u].xyz) + view.drawParams[pc.drawParamIndex * 9u + 3u].w;
        float eyeT = dot(pc.params14.xyz, view.drawParams[pc.drawParamIndex * 9u + 4u].xyz) + view.drawParams[pc.drawParamIndex * 9u + 4u].w;
        float t = dot(pos, view.drawParams[pc.drawParamIndex * 9u + 4u].xyz) + view.drawParams[pc.drawParamIndex * 9u + 4u].w;
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
        finalColor = vec4(view.drawParams[pc.drawParamIndex * 9u + 1u].xyz, 1.0);
    } else {
        /* UI pics set params14.w ≈ 0.2 as a fragment marker; still run tcMod
         * so console/scroll shaders animate (CPU ApplyPicTexMods is skipped
         * when the stage uses the world pipeline). */
        finalUv = applyTcMod(uv, pos);
        finalColor = inColor;
        /* Entity mesh lighting (CGEN_LIGHTING_DIFFUSE): ambient + max(0,N·L)*directed. */
        if (view.drawParams[pc.drawParamIndex * 9u + 6u].w > 0.5) {
            vec3 amb = view.drawParams[pc.drawParamIndex * 9u + 6u].xyz;
            vec3 directed = view.drawParams[pc.drawParamIndex * 9u + 7u].xyz;
            vec3 lightDir = view.drawParams[pc.drawParamIndex * 9u + 8u].xyz;
            float incoming = max(dot(normalize(nrm), lightDir), 0.0);
            finalColor = vec4(min(amb + incoming * directed, vec3(1.0)), inColor.a);
        }
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
    if (view.drawParams[pc.drawParamIndex * 9u + 2u].w > 0.5 && view.drawParams[pc.drawParamIndex * 9u + 1u].w > 0.0 && view.drawParams[pc.drawParamIndex * 9u + 1u].w < 4.0) {
        vec3 eyeVec = pos - pc.params14.xyz;
        float dist;
        int distMode = int(view.drawParams[pc.drawParamIndex * 9u + 5u].w + 0.5);
        if (distMode == 1) {
            dist = abs(dot(eyeVec, view.drawParams[pc.drawParamIndex * 9u + 5u].xyz));
        } else if (distMode == 2) {
            dist = dot(eyeVec, view.drawParams[pc.drawParamIndex * 9u + 5u].xyz);
        } else {
            dist = length(eyeVec);
        }
        if (view.drawParams[pc.drawParamIndex * 9u + 1u].w < 1.5) {
            /* GL_LINEAR */
            vFogFactor = clamp((view.drawParams[pc.drawParamIndex * 9u + 2u].y - dist) / (view.drawParams[pc.drawParamIndex * 9u + 2u].y - view.drawParams[pc.drawParamIndex * 9u + 2u].x), 0.0, 1.0);
        } else if (view.drawParams[pc.drawParamIndex * 9u + 1u].w < 2.5) {
            /* GL_EXP */
            vFogFactor = exp(-view.drawParams[pc.drawParamIndex * 9u + 2u].z * dist);
        } else {
            /* GL_EXP2 */
            float d = view.drawParams[pc.drawParamIndex * 9u + 2u].z * dist;
            vFogFactor = exp(-d * d);
        }
    }

    float normalZFadeAlpha = 1.0;
    if (view.drawParams[pc.drawParamIndex * 9u + 0u].w > 0.0) {
        vec3 worldUp = normalize(view.drawParams[pc.drawParamIndex * 9u + 0u].xyz);
        float d = dot(normalize(nrm), worldUp);
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

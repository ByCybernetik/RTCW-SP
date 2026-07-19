#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec2 vLightmapCoord;
layout(location = 2) in vec4 vColor;
layout(location = 3) in vec3 vWorldPos;
layout(location = 4) in float vNormalZFadeAlpha;
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D uBaseTex;
layout(binding = 1) uniform sampler2D uLightmapTex;
layout(push_constant) uniform PushConstants {
    uint mvpIndex;
    uint drawParamIndex;
    uint boneSet;
    uint meshShortMode;
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
    vec4 drawParams[18432];
} view;

layout(set = 1, binding = 2) uniform DlightUBO {
    uvec4 header; /* .x = count */
    vec4 posRadius[32];
    vec4 color[32];
} dlightUbo;

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
    if (func == 4) {
        return t < 0.5 ? 1.0 : -1.0;
    }
    return sin(x * 6.28318530718);
}

/* Replicates R_FogFactor from the GL renderer using the same
 * fog table curve (exp == 0.5, i.e. sqrt). */
float calcFogDensity(vec3 worldPos) {
    float s = dot(worldPos, view.drawParams[pc.drawParamIndex * 9u + 3u].xyz) + view.drawParams[pc.drawParamIndex * 9u + 3u].w;
    float eyeT = dot(pc.params14.xyz, view.drawParams[pc.drawParamIndex * 9u + 4u].xyz) + view.drawParams[pc.drawParamIndex * 9u + 4u].w;
    float t = dot(worldPos, view.drawParams[pc.drawParamIndex * 9u + 4u].xyz) + view.drawParams[pc.drawParamIndex * 9u + 4u].w;
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

    s -= 1.0 / 512.0;
    if (s < 0.0) {
        return 0.0;
    }
    if (t < 1.0 / 32.0) {
        return 0.0;
    }
    if (t < 31.0 / 32.0) {
        s *= (t - 1.0 / 32.0) / (30.0 / 32.0);
    }
    s = min(s * 8.0, 1.0);
    return sqrt(s);
}

void main() {
    if (pc.params14.w > 0.5) {
        /* Multi-light pass: mask in boneSet (uint). uBaseTex is tr.dlightImage. */
        uint mask = pc.boneSet;
        uint count = dlightUbo.header.x;
        vec3 accum = vec3(0.0);
        uint i;

        for (i = 0u; i < count && i < 32u; i++) {
            if ((mask & (1u << i)) == 0u) {
                continue;
            }
            vec3 dist = dlightUbo.posRadius[i].xyz - vWorldPos;
            float radius = max(dlightUbo.posRadius[i].w, 1.0);
            vec2 dlightUv = vec2(0.5) + dist.xy / radius;
            if (dlightUv.x < 0.0 || dlightUv.x > 1.0 || dlightUv.y < 0.0 || dlightUv.y > 1.0) {
                continue;
            }
            float z = abs(dist.z);
            if (z > radius) {
                continue;
            }
            float modulate = (z < radius * 0.5) ? 1.0 : 2.0 * (radius - z) / radius;
            vec3 falloff = texture(uBaseTex, dlightUv).rgb;
            accum += dlightUbo.color[i].xyz * modulate * falloff;
        }
        if (dot(accum, accum) < 0.0001) {
            discard;
        }
        outColor = vec4(accum, 1.0);
        return;
    }

    /* UI / StretchPic via world pipeline (params14.w ≈ 0.2): match 2d_frag —
     * always modulate by vertex color (identityLight, waveform, const, etc.).
     * Without this, CGEN_IDENTITY_LIGHTING colors baked into verts are ignored
     * and console/background shaders look ~2× too bright. */
    if (pc.params14.w > 0.1 && pc.params14.w < 0.5) {
        vec4 tex = texture(uBaseTex, vTexCoord);
        vec4 color = tex * vColor;
        if (pc.params0.z > 0.5) {
            float ref = pc.params0.w;
            int func = int(pc.params9.x + 0.5);
            float alpha = color.a;
            bool discardPixel = false;
            if (func == 1) {
                if (alpha <= 0.0) discardPixel = true;
            } else if (func == 2) {
                if (alpha >= ref) discardPixel = true;
            } else if (func == 3) {
                if (alpha < ref - 0.001) discardPixel = true;
            } else {
                if (alpha <= ref) discardPixel = true;
            }
            if (discardPixel) discard;
        }
        outColor = color;
        return;
    }

    // Portal/mirror clip plane: discard fragments behind the portal surface.
    // params13 is zeroed for non-portal views, so this is skipped normally.
    if (dot(pc.params13.xyz, pc.params13.xyz) > 0.001) {
        if (dot(vWorldPos, pc.params13.xyz) - pc.params13.w < 0.0) {
            discard;
        }
    }

    /* Always sample the fog image (GL_CLAMP edge = densest fog). The previous
     * s>1 / t-out-of-range → alpha=1 shortcut painted solid fog color on any
     * bad coord and washed MD3/MDS white when fog UVs were wrong. */
    if (view.drawParams[pc.drawParamIndex * 9u + 1u].w > 3.5) {
        float fogAlpha = texture(uBaseTex, vTexCoord).a;
        outColor = vec4(vColor.rgb, vColor.a * fogAlpha);
        return;
    }

    vec4 base4 = texture(uBaseTex, vTexCoord);
    /* Lightmaps are already scaled during upload (R_ColorShiftLightingBytes),
     * matching OpenGL's GL_MODULATE path, so no extra *2.0 factor is needed. */
    vec3 lm = (pc.params7.z > 0.5) ? vec3(1.0) : texture(uLightmapTex, vLightmapCoord).rgb;
    bool sourceAlphaDecal = pc.params9.z > 0.5;
    bool skyMode = pc.params11.w > 0.5;
    vec3 vertexRgb = (pc.params7.x > 0.5) ? vColor.rgb : vec3(1.0);
    float vertexAlpha = (pc.params7.y > 0.5) ? vColor.a : 1.0;
    if (view.drawParams[pc.drawParamIndex * 9u + 0u].w > 0.0) {
        vertexAlpha *= vNormalZFadeAlpha * view.drawParams[pc.drawParamIndex * 9u + 0u].w;
    }
    if (skyMode) {
        int rgbMode = int(pc.params1.x + 0.5);
        if (rgbMode == 1) {
            vertexRgb *= pc.params1.yzw;
        } else if (rgbMode == 2) {
            float wave = pc.params2.y + pc.params2.z * evalWave(pc.params2.x, pc.params2.w + pc.params8.y * pc.params8.w);
            vertexRgb *= vec3(max(wave, 0.0));
        }
    }
    if (sourceAlphaDecal) {
        vertexRgb = max(vertexRgb, vec3(0.85));
        vertexAlpha = sqrt(clamp(vertexAlpha, 0.0, 1.0));
    }
    vec3 lit = base4.rgb * lm * vertexRgb;
    if (skyMode && pc.params3.w > 0.0) {
        lit = mix(lit, pc.params3.xyz, clamp(pc.params3.w, 0.0, 0.75));
    } else if (pc.params11.z > 0.5) {
        vec3 fogColor = vec3(0.42, 0.58, 0.52);
        lit = mix(lit, fogColor, clamp(pc.params9.w, 0.0, 0.45));
    }

    /* Volumetric fog modulation for translucent surfaces inside fog volumes.
     * params17.w encodes the mode: 2 = RGB, 3 = RGBA, 4 = ALPHA. */
    float fogModFactor = 1.0;
    int fogModMode = 0;
    if (view.drawParams[pc.drawParamIndex * 9u + 2u].w > 1.5) {
        fogModMode = int(view.drawParams[pc.drawParamIndex * 9u + 2u].w + 0.5);
        fogModFactor = 1.0 - calcFogDensity(vWorldPos);
        if (fogModMode == 2 || fogModMode == 3) {
            lit *= fogModFactor;
        }
    }

    /* Distance fog matching OpenGL GL_FOG. Evaluate the fog equation per
     * fragment from world position so large outdoor brushes are not
     * under-fogged (lerping the factor / radial length is too clear). */
    if (view.drawParams[pc.drawParamIndex * 9u + 2u].w > 0.5 && view.drawParams[pc.drawParamIndex * 9u + 1u].w > 0.0 && view.drawParams[pc.drawParamIndex * 9u + 1u].w < 4.0) {
        vec3 eyeVec = vWorldPos - pc.params14.xyz;
        float fogDist;
        float fogFactor;
        float fogStart = view.drawParams[pc.drawParamIndex * 9u + 2u].x;
        float fogEnd = view.drawParams[pc.drawParamIndex * 9u + 2u].y;
        float fogDensity = view.drawParams[pc.drawParamIndex * 9u + 2u].z;
        float mode = view.drawParams[pc.drawParamIndex * 9u + 1u].w;
        int distMode = int(view.drawParams[pc.drawParamIndex * 9u + 5u].w + 0.5);

        if (distMode == 1) {
            fogDist = abs(dot(eyeVec, view.drawParams[pc.drawParamIndex * 9u + 5u].xyz));
        } else if (distMode == 2) {
            fogDist = dot(eyeVec, view.drawParams[pc.drawParamIndex * 9u + 5u].xyz);
        } else {
            fogDist = length(eyeVec);
        }

        if (mode < 1.5) {
            fogFactor = clamp((fogEnd - fogDist) / (fogEnd - fogStart), 0.0, 1.0);
        } else if (mode < 2.5) {
            fogFactor = exp(-fogDensity * fogDist);
        } else {
            float d = fogDensity * fogDist;
            fogFactor = exp(-d * d);
        }
        lit = mix(view.drawParams[pc.drawParamIndex * 9u + 1u].xyz, lit, fogFactor);
    }

    if (pc.params12.z > 0.5) {
        float range = max(pc.params12.w, 1.0);
        float portalAlpha = clamp(length(vWorldPos - pc.params14.xyz) / range, 0.0, 1.0);
        vertexAlpha *= portalAlpha;
    }
    float alpha = clamp(base4.a * vertexAlpha * pc.params0.y, 0.0, 1.0);
    /* Always write the texture alpha so that later stages using
       ONE_MINUS_DST_ALPHA / DST_ALPHA can read the correct value.
       For blended stages this is already required; for opaque stages
       it is harmless because blending is disabled. */
    float outAlpha = alpha;

    if (fogModMode == 3 || fogModMode == 4) {
        outAlpha *= fogModFactor;
    }

    if (pc.params0.z > 0.5) {
        float ref = pc.params0.w;
        int func = int(pc.params9.x + 0.5);
        bool discardPixel = false;

        if (func == 1) {
            if (alpha <= 0.0) discardPixel = true;
        } else if (func == 2) {
            if (alpha >= ref) discardPixel = true;
        } else if (func == 3) {
            if (alpha < ref - 0.001) discardPixel = true;
        } else {
            if (alpha <= ref) discardPixel = true;
        }

        if (discardPixel) discard;
    }

    if (pc.params0.x > 0.5) {
        outColor = vec4(alpha, alpha, alpha, 1.0);
    } else if (skyMode) {
        outColor = vec4(lit, outAlpha);
    } else if (sourceAlphaDecal) {
        outColor = vec4(lit, outAlpha);
    } else {
        outColor = vec4(lit, outAlpha);
    }
}

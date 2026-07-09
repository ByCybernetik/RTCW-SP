#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec2 vLightmapCoord;
layout(location = 2) in vec4 vColor;
layout(location = 3) in vec3 vWorldPos;
layout(location = 4) in float vNormalZFadeAlpha;
layout(location = 5) in float vFogFactor;
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D uBaseTex;
layout(binding = 1) uniform sampler2D uLightmapTex;
layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout(set = 1, binding = 0) uniform UBO {
    vec4 params[21];
} ubo;

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
    float s = dot(worldPos, ubo.params[18].xyz) + ubo.params[18].w;
    float eyeT = dot(ubo.params[14].xyz, ubo.params[19].xyz) + ubo.params[19].w;
    float t = dot(worldPos, ubo.params[19].xyz) + ubo.params[19].w;
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
    if (ubo.params[14].w > 0.5) {
        vec3 dist = ubo.params[1].xyz - vWorldPos;
        float radius = max(ubo.params[1].w, 1.0);
        vec2 dlightUv = vec2(0.5) + dist.xy / radius;
        if (dlightUv.x < 0.0 || dlightUv.x > 1.0 || dlightUv.y < 0.0 || dlightUv.y > 1.0) {
            discard;
        }
        float z = abs(dist.z);
        if (z > radius) {
            discard;
        }
        float modulate = (z < radius * 0.5) ? 1.0 : 2.0 * (radius - z) / radius;
        vec2 radial = dlightUv - vec2(0.5);
        float radialFade = clamp(1.0 - dot(radial, radial) * 4.0, 0.0, 1.0);
        if (radialFade <= 0.0) {
            discard;
        }
        outColor = vec4(ubo.params[2].xyz * modulate * radialFade, 1.0);
        return;
    }

    // Portal/mirror clip plane: discard fragments behind the portal surface.
    // params13 is zeroed for non-portal views, so this is skipped normally.
    if (dot(ubo.params[13].xyz, ubo.params[13].xyz) > 0.001) {
        if (dot(vWorldPos, ubo.params[13].xyz) - ubo.params[13].w < 0.0) {
            discard;
        }
    }

    /* Volumetric fog volume pass: draw the fog texture modulated by fog color.
     * The fog texture is white with alpha equal to fog density.
     * For UVs outside the texture we keep a physically reasonable fallback:
     * close to the eye (s < 0) means no fog, far/behind the clipping plane
     * (s > 1 or t out of bounds) means fully fogged. Vertex alpha is preserved
     * so a future non-opaque fog color behaves like OpenGL's modulate path. */
    if (ubo.params[16].w > 3.5) {
        float fogAlpha;
        if (vTexCoord.s < 0.0) {
            fogAlpha = 0.0;
        } else if (vTexCoord.s > 1.0 || vTexCoord.t < 0.0 || vTexCoord.t > 1.0) {
            fogAlpha = 1.0;
        } else {
            fogAlpha = texture(uBaseTex, vTexCoord).a;
        }
        outColor = vec4(vColor.rgb, vColor.a * fogAlpha);
        return;
    }

    vec4 base4 = texture(uBaseTex, vTexCoord);
    /* Lightmaps are already scaled during upload (R_ColorShiftLightingBytes),
     * matching OpenGL's GL_MODULATE path, so no extra *2.0 factor is needed. */
    vec3 lm = (ubo.params[7].z > 0.5) ? vec3(1.0) : texture(uLightmapTex, vLightmapCoord).rgb;
    bool sourceAlphaDecal = ubo.params[9].z > 0.5;
    bool skyMode = ubo.params[11].w > 0.5;
    vec3 vertexRgb = (ubo.params[7].x > 0.5) ? vColor.rgb : vec3(1.0);
    float vertexAlpha = (ubo.params[7].y > 0.5) ? vColor.a : 1.0;
    if (ubo.params[15].w > 0.0) {
        vertexAlpha *= vNormalZFadeAlpha * ubo.params[15].w;
    }
    if (skyMode) {
        int rgbMode = int(ubo.params[1].x + 0.5);
        if (rgbMode == 1) {
            vertexRgb *= ubo.params[1].yzw;
        } else if (rgbMode == 2) {
            float wave = ubo.params[2].y + ubo.params[2].z * evalWave(ubo.params[2].x, ubo.params[2].w + ubo.params[8].y * ubo.params[8].w);
            vertexRgb *= vec3(max(wave, 0.0));
        }
    }
    if (sourceAlphaDecal) {
        vertexRgb = max(vertexRgb, vec3(0.85));
        vertexAlpha = sqrt(clamp(vertexAlpha, 0.0, 1.0));
    }
    vec3 lit = base4.rgb * lm * vertexRgb;
    if (ubo.params[13].w > 0.0) {
        lit = mix(lit, ubo.params[13].xyz, clamp(ubo.params[13].w, 0.0, 0.95));
    } else if (skyMode && ubo.params[3].w > 0.0) {
        lit = mix(lit, ubo.params[3].xyz, clamp(ubo.params[3].w, 0.0, 0.75));
    } else if (ubo.params[11].z > 0.5) {
        vec3 fogColor = vec3(0.42, 0.58, 0.52);
        lit = mix(lit, fogColor, clamp(ubo.params[9].w, 0.0, 0.45));
    }

    /* Volumetric fog modulation for translucent surfaces inside fog volumes.
     * params17.w encodes the mode: 2 = RGB, 3 = RGBA, 4 = ALPHA. */
    float fogModFactor = 1.0;
    int fogModMode = 0;
    if (ubo.params[17].w > 1.5) {
        fogModMode = int(ubo.params[17].w + 0.5);
        fogModFactor = 1.0 - calcFogDensity(vWorldPos);
        if (fogModMode == 2 || fogModMode == 3) {
            lit *= fogModFactor;
        }
    }

    /* Distance fog matching OpenGL GL_FOG. The fog factor was computed per-vertex
     * in world.vert and perspective-correct interpolated, matching OpenGL's
     * per-vertex fog coordinate. This avoids per-pixel world-position interpolation
     * artifacts on large brush polygons.  Active whenever params16.w selects a
     * distance-fog mode (1=linear, 2=exp, 3=exp2), even if volumetric modulation
     * is also on. */
    if (ubo.params[17].w > 0.5 && ubo.params[16].w > 0.0 && ubo.params[16].w < 4.0) {
        lit = mix(ubo.params[16].xyz, lit, vFogFactor);
    }

    if (ubo.params[12].z > 0.5) {
        float range = max(ubo.params[12].w, 1.0);
        float portalAlpha = clamp(length(vWorldPos - ubo.params[14].xyz) / range, 0.0, 1.0);
        vertexAlpha *= portalAlpha;
    }
    float alpha = clamp(base4.a * vertexAlpha * ubo.params[0].y, 0.0, 1.0);
    /* Always write the texture alpha so that later stages using
       ONE_MINUS_DST_ALPHA / DST_ALPHA can read the correct value.
       For blended stages this is already required; for opaque stages
       it is harmless because blending is disabled. */
    float outAlpha = alpha;

    if (fogModMode == 3 || fogModMode == 4) {
        outAlpha *= fogModFactor;
    }

    if (ubo.params[0].z > 0.5) {
        float ref = ubo.params[0].w;
        int func = int(ubo.params[9].x + 0.5);
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

    if (ubo.params[14].w > 0.1 && ubo.params[14].w < 0.5) {
        outColor = vec4(lit, outAlpha);
    } else if (ubo.params[0].x > 0.5) {
        outColor = vec4(alpha, alpha, alpha, 1.0);
    } else if (skyMode) {
        outColor = vec4(lit, outAlpha);
    } else if (sourceAlphaDecal) {
        outColor = vec4(lit, outAlpha);
    } else {
        outColor = vec4(lit, outAlpha);
    }
}

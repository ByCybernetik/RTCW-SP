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
    vec4 params16;
    vec4 params17;
    vec4 params18;
    vec4 params19;
} pc;

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
    float s = dot(worldPos, pc.params18.xyz) + pc.params18.w;
    float eyeT = dot(pc.params14.xyz, pc.params19.xyz) + pc.params19.w;
    float t = dot(worldPos, pc.params19.xyz) + pc.params19.w;
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
        vec3 dist = pc.params1.xyz - vWorldPos;
        float radius = max(pc.params1.w, 1.0);
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
        outColor = vec4(pc.params2.xyz * modulate * radialFade, 1.0);
        return;
    }

    // Portal/mirror clip plane: discard fragments behind the portal surface.
    // params13 is zeroed for non-portal views, so this is skipped normally.
    if (dot(pc.params13.xyz, pc.params13.xyz) > 0.001) {
        if (dot(vWorldPos, pc.params13.xyz) - pc.params13.w < 0.0) {
            discard;
        }
    }

    /* Volumetric fog volume pass: draw the fog texture modulated by fog color.
     * The fog texture is white with alpha equal to fog density. */
    if (pc.params16.w > 2.5) {
        vec4 fogTex = texture(uBaseTex, vTexCoord);
        outColor = vec4(vColor.rgb, fogTex.a);
        return;
    }

    vec4 base4 = texture(uBaseTex, vTexCoord);
    vec3 lm = (pc.params7.z > 0.5) ? vec3(1.0) : texture(uLightmapTex, vLightmapCoord).rgb * 2.0;
    bool sourceAlphaDecal = pc.params9.z > 0.5;
    bool skyMode = pc.params11.w > 0.5;
    vec3 vertexRgb = (pc.params7.x > 0.5) ? vColor.rgb : vec3(1.0);
    float vertexAlpha = (pc.params7.y > 0.5) ? vColor.a : 1.0;
    if (pc.params15.w > 0.0) {
        vertexAlpha *= vNormalZFadeAlpha * pc.params15.w;
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
    if (pc.params13.w > 0.0) {
        lit = mix(lit, pc.params13.xyz, clamp(pc.params13.w, 0.0, 0.95));
    } else if (skyMode && pc.params3.w > 0.0) {
        lit = mix(lit, pc.params3.xyz, clamp(pc.params3.w, 0.0, 0.75));
    } else if (pc.params11.z > 0.5) {
        vec3 fogColor = vec3(0.42, 0.58, 0.52);
        lit = mix(lit, fogColor, clamp(pc.params9.w, 0.0, 0.45));
    }

    /* Volumetric fog modulation for translucent surfaces inside fog volumes.
     * params17.w encodes the mode: 2 = RGB, 3 = RGBA, 4 = ALPHA. */
    float fogModFactor = 1.0;
    int fogModMode = 0;
    if (pc.params17.w > 1.5) {
        fogModMode = int(pc.params17.w + 0.5);
        fogModFactor = 1.0 - calcFogDensity(vWorldPos);
        if (fogModMode == 2 || fogModMode == 3) {
            lit *= fogModFactor;
        }
    }

    /* Distance fog matching OpenGL GL_FOG. The fog factor was computed per-vertex
     * in world.vert and perspective-correct interpolated, matching OpenGL's
     * per-vertex fog coordinate. This avoids per-pixel world-position interpolation
     * artifacts on large brush polygons. */
    if (pc.params17.w > 0.5 && pc.params17.w < 1.5 && pc.params16.w > 0.0) {
        lit = mix(pc.params16.xyz, lit, vFogFactor);
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

    if (pc.params14.w > 0.1 && pc.params14.w < 0.5) {
        outColor = vec4(lit, outAlpha);
    } else if (pc.params0.x > 0.5) {
        outColor = vec4(alpha, alpha, alpha, 1.0);
    } else if (skyMode) {
        outColor = vec4(lit, outAlpha);
    } else if (sourceAlphaDecal) {
        outColor = vec4(lit, outAlpha);
    } else {
        outColor = vec4(lit, outAlpha);
    }
}

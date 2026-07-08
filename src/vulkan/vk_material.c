#include "vk_material.h"
#include "../game/surfaceflags.h"
#include <string.h>

static int g_alphaTestEnabled;
static int g_alphaTestFunc;
static float g_alphaTestRef;
static int g_materialBlendEnabled;
static int g_rgbGenVertex;
static int g_alphaGenVertex;
static int g_alphaGenPortal;
static float g_alphaGenPortalRange;
static int g_alphaGenNormalZFade;
static float g_normalZFadeBounds[2];
static float g_normalZFadeFireRiseDir[3];
static float g_normalZFadeMaxAlpha;
static int g_noLightmap;
static int g_tcGenEnvironment;
static int g_sourceAlphaDecal;
static int g_stageIsLightmap;
static float g_tcModScale[2];
static float g_tcModScroll[2];
static float g_tcModRotate;
static float g_tcModTransform[4];
static float g_tcModTranslate[2];
static int g_tcModTransformEnabled;
static int g_tcModTurbEnabled;
static float g_tcModTurb[4];
static int g_tcModStretchEnabled;
static float g_tcModStretch[5];
static float g_deformWaveDiv;
static float g_deformWaveFunc;
static float g_deformWaveBase;
static float g_deformWaveAmp;
static float g_deformWavePhase;
static float g_deformWaveFreq;
static int g_deformType;
static int g_waterFog;
static float g_waterFogStrength;
static int g_polygonOffset;
static float g_polyOffsetFactor;
static float g_polyOffsetUnits;

static cvar_t *r_vkDistanceFog;

static float MapWaveFunc(genFunc_t func) {
    switch (func) {
    case GF_TRIANGLE: return 1.0f;
    case GF_SAWTOOTH: return 2.0f;
    case GF_INVERSE_SAWTOOTH: return 3.0f;
    case GF_SQUARE: return 4.0f;
    case GF_SIN:
    default: return 0.0f;
    }
}

static void ResetMaterialStageState(void) {
    g_rgbGenVertex = 0;
    g_alphaGenVertex = 0;
    g_alphaGenPortal = 0;
    g_alphaGenPortalRange = 256.0f;
    g_alphaGenNormalZFade = 0;
    g_normalZFadeBounds[0] = -1.0f;
    g_normalZFadeBounds[1] = 1.0f;
    VectorClear(g_normalZFadeFireRiseDir);
    g_normalZFadeMaxAlpha = 0.0f;
    g_noLightmap = 0;
    g_tcGenEnvironment = 0;
    g_sourceAlphaDecal = 0;
    g_stageIsLightmap = 0;
    g_tcModScale[0] = 1.0f;
    g_tcModScale[1] = 1.0f;
    g_tcModScroll[0] = 0.0f;
    g_tcModScroll[1] = 0.0f;
    g_tcModRotate = 0.0f;
    g_tcModTransform[0] = 1.0f;
    g_tcModTransform[1] = 0.0f;
    g_tcModTransform[2] = 0.0f;
    g_tcModTransform[3] = 1.0f;
    g_tcModTranslate[0] = 0.0f;
    g_tcModTranslate[1] = 0.0f;
    g_tcModTransformEnabled = 0;
    g_tcModTurbEnabled = 0;
    g_tcModTurb[0] = 0.0f;
    g_tcModTurb[1] = 0.0f;
    g_tcModTurb[2] = 0.0f;
    g_tcModTurb[3] = 1.0f;
    g_tcModStretchEnabled = 0;
    g_tcModStretch[0] = 0.0f;
    g_tcModStretch[1] = 1.0f;
    g_tcModStretch[2] = 0.0f;
    g_tcModStretch[3] = 0.0f;
    g_tcModStretch[4] = 1.0f;
    g_deformWaveDiv = 1.0f;
    g_deformWaveFunc = 0.0f;
    g_deformWaveBase = 0.0f;
    g_deformWaveAmp = 0.0f;
    g_deformWavePhase = 0.0f;
    g_deformWaveFreq = 1.0f;
    g_deformType = 0;
    g_waterFog = 0;
    g_waterFogStrength = 0.0f;
    g_polygonOffset = 0;
    g_polyOffsetFactor = r_offsetFactor ? r_offsetFactor->value : -1.0f;
    g_polyOffsetUnits = r_offsetUnits ? r_offsetUnits->value : -2.0f;
}

static int ShaderHasLightmapStage(const shader_t *shader) {
    int i;

    if (!shader) {
        return 0;
    }

    for (i = 0; i < MAX_SHADER_STAGES && shader->stages[i]; i++) {
        if (shader->stages[i]->bundle[0].isLightmap) {
            return 1;
        }
    }
    return 0;
}

static int DepthWritePipelineForStage(int pipelineIndex) {
    switch (pipelineIndex) {
    case VK_PIPELINE_ALPHA: return VK_PIPELINE_ALPHA_DEPTHWRITE;
    case VK_PIPELINE_ADDITIVE: return VK_PIPELINE_ADDITIVE_DEPTHWRITE;
    case VK_PIPELINE_SRC_ALPHA_ONE: return VK_PIPELINE_SRC_ALPHA_ONE_DEPTHWRITE;
    case VK_PIPELINE_FILTER: return VK_PIPELINE_FILTER_DEPTHWRITE;
    case VK_PIPELINE_DSTCOLOR_ONE: return VK_PIPELINE_DSTCOLOR_ONE_DEPTHWRITE;
    case VK_PIPELINE_ONE_MINUS_DST_ALPHA_ONE: return VK_PIPELINE_ONE_MINUS_DST_ALPHA_ONE_DEPTHWRITE;
    case VK_PIPELINE_ONE_ONE_MINUS_SRC_ALPHA: return VK_PIPELINE_ONE_ONE_MINUS_SRC_ALPHA_DEPTHWRITE;
    case VK_PIPELINE_DSTCOLOR_ONE_MINUS_DST_ALPHA: return VK_PIPELINE_DSTCOLOR_ONE_MINUS_DST_ALPHA_DEPTHWRITE;
    case VK_PIPELINE_ONE_ONE_MINUS_SRC_COLOR: return VK_PIPELINE_ONE_ONE_MINUS_SRC_COLOR_DEPTHWRITE;
    case VK_PIPELINE_SRC_ALPHA_ONE_MINUS_SRC_COLOR: return VK_PIPELINE_SRC_ALPHA_ONE_MINUS_SRC_COLOR_DEPTHWRITE;
    case VK_PIPELINE_ZERO_ONE_MINUS_SRC_ALPHA: return VK_PIPELINE_ZERO_ONE_MINUS_SRC_ALPHA_DEPTHWRITE;
    case VK_PIPELINE_ZERO_ONE_MINUS_SRC_COLOR: return VK_PIPELINE_ZERO_ONE_MINUS_SRC_COLOR_DEPTHWRITE;
    case VK_PIPELINE_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR: return VK_PIPELINE_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR_DEPTHWRITE;
    default: return pipelineIndex;
    }
}

qboolean VK_StageIsBlended(const shaderStage_t *stage) {
    unsigned stateBits;

    if (!stage) {
        return qfalse;
    }

    stateBits = stage->stateBits;
    return (stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? qtrue : qfalse;
}

qboolean VK_RenderPassMatchesStage(int pass, qboolean polygonOffset, qboolean blended) {
    switch (pass) {
    case 0: return !polygonOffset && !blended;
    case 1: return !polygonOffset && blended;
    case 2: return polygonOffset && !blended;
    case 3: return polygonOffset && blended;
    default: return qfalse;
    }
}

qboolean VK_StageUsesSourceAlphaBlend(const shaderStage_t *stage) {
    unsigned stateBits;
    int srcBlend;
    int dstBlend;

    if (!stage || !VK_StageIsBlended(stage)) {
        return qfalse;
    }

    stateBits = stage->stateBits;
    srcBlend = stateBits & GLS_SRCBLEND_BITS;
    dstBlend = stateBits & GLS_DSTBLEND_BITS;
    return (srcBlend == GLS_SRCBLEND_SRC_ALPHA ||
            srcBlend == GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA ||
            dstBlend == GLS_DSTBLEND_SRC_ALPHA ||
            dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA) ? qtrue : qfalse;
}

int VK_PipelineForStage(const shaderStage_t *stage) {
    unsigned stateBits;
    int srcBlend;
    int dstBlend;
    int depthWrite;
    int pipelineIndex;

    if (!stage) {
        return VK_PIPELINE_OPAQUE;
    }

    stateBits = stage->stateBits;
    if (stateBits & GLS_DEPTHTEST_DISABLE) {
        return VK_PIPELINE_DEPTH_DISABLED_ALPHA;
    }

    srcBlend = stateBits & GLS_SRCBLEND_BITS;
    dstBlend = stateBits & GLS_DSTBLEND_BITS;
    depthWrite = (stateBits & GLS_DEPTHMASK_TRUE) ? 1 : 0;
    if (stateBits & GLS_DEPTHFUNC_EQUAL) {
        depthWrite = 0;
    }

    if (!srcBlend && !dstBlend) {
        /* Alpha test uses shader discard; needs opaque pipeline + depth write (not VK_PIPELINE_ALPHA). */
        pipelineIndex = VK_PIPELINE_OPAQUE;
    } else if (srcBlend == GLS_SRCBLEND_ONE && dstBlend == GLS_DSTBLEND_ONE) {
        pipelineIndex = VK_PIPELINE_ADDITIVE;
    } else if (srcBlend == GLS_SRCBLEND_SRC_ALPHA && dstBlend == GLS_DSTBLEND_ONE) {
        pipelineIndex = VK_PIPELINE_SRC_ALPHA_ONE;
    } else if (srcBlend == GLS_SRCBLEND_SRC_ALPHA && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA) {
        pipelineIndex = VK_PIPELINE_ALPHA;
    } else if (srcBlend == GLS_SRCBLEND_DST_COLOR &&
               (dstBlend == GLS_DSTBLEND_ZERO || !dstBlend)) {
        pipelineIndex = VK_PIPELINE_FILTER;
    } else if (srcBlend == GLS_SRCBLEND_DST_COLOR && dstBlend == GLS_DSTBLEND_ONE) {
        pipelineIndex = VK_PIPELINE_DSTCOLOR_ONE;
    } else if (srcBlend == GLS_SRCBLEND_ONE_MINUS_DST_ALPHA && dstBlend == GLS_DSTBLEND_ONE) {
        pipelineIndex = VK_PIPELINE_ONE_MINUS_DST_ALPHA_ONE;
    } else if (srcBlend == GLS_SRCBLEND_ONE && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA) {
        pipelineIndex = VK_PIPELINE_ONE_ONE_MINUS_SRC_ALPHA;
    } else if (srcBlend == GLS_SRCBLEND_DST_COLOR && dstBlend == GLS_DSTBLEND_ONE_MINUS_DST_ALPHA) {
        pipelineIndex = VK_PIPELINE_DSTCOLOR_ONE_MINUS_DST_ALPHA;
    } else if (srcBlend == GLS_SRCBLEND_ONE && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR) {
        pipelineIndex = VK_PIPELINE_ONE_ONE_MINUS_SRC_COLOR;
    } else if (srcBlend == GLS_SRCBLEND_SRC_ALPHA && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR) {
        pipelineIndex = VK_PIPELINE_SRC_ALPHA_ONE_MINUS_SRC_COLOR;
    } else if (srcBlend == GLS_SRCBLEND_ZERO && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA) {
        pipelineIndex = VK_PIPELINE_ZERO_ONE_MINUS_SRC_ALPHA;
    } else if (srcBlend == GLS_SRCBLEND_ZERO && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR) {
        pipelineIndex = VK_PIPELINE_ZERO_ONE_MINUS_SRC_COLOR;
    } else if (srcBlend == GLS_SRCBLEND_ZERO && dstBlend == GLS_DSTBLEND_SRC_COLOR) {
        pipelineIndex = VK_PIPELINE_ONE_MINUS_DST_ALPHA_ONE;
    } else if (srcBlend == GLS_SRCBLEND_ONE_MINUS_DST_COLOR && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR) {
        pipelineIndex = VK_PIPELINE_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR;
    } else {
        pipelineIndex = VK_PIPELINE_ALPHA;
    }

    if ((stateBits & GLS_DEPTHFUNC_EQUAL) && !depthWrite) {
        if (pipelineIndex == VK_PIPELINE_FILTER) {
            return VK_PIPELINE_FILTER_EQUAL;
        }
        if (pipelineIndex == VK_PIPELINE_DSTCOLOR_ONE ||
            pipelineIndex == VK_PIPELINE_ONE_MINUS_DST_ALPHA_ONE) {
            return VK_PIPELINE_DLIGHT;
        }
    }

    return depthWrite ? DepthWritePipelineForStage(pipelineIndex) : pipelineIndex;
}

static void SetAlphaTestFromStage(const shaderStage_t *stage) {
    unsigned stateBits;

    g_alphaTestEnabled = 0;
    g_alphaTestFunc = 0;
    g_alphaTestRef = 0.5f;

    if (!stage) {
        return;
    }

    stateBits = stage->stateBits;
    if (stateBits & GLS_ATEST_GT_0) {
        g_alphaTestEnabled = 1;
        g_alphaTestFunc = 1;
        g_alphaTestRef = 0.0f;
    } else if (stateBits & GLS_ATEST_LT_80) {
        g_alphaTestEnabled = 1;
        g_alphaTestFunc = 2;
        g_alphaTestRef = 0.5f;
    } else if (stateBits & GLS_ATEST_GE_80) {
        g_alphaTestEnabled = 1;
        g_alphaTestFunc = 3;
        g_alphaTestRef = 0.5f;
    }
}

void VK_SetStageStateFromShader(const shader_t *shader, const shaderStage_t *stage) {
    int i;

    ResetMaterialStageState();
    g_alphaTestEnabled = 0;
    g_alphaTestFunc = 0;
    g_alphaTestRef = 0.5f;
    g_materialBlendEnabled = 0;

    if (!shader) {
        return;
    }

    if (shader->surfaceFlags & SURF_NOLIGHTMAP) {
        g_noLightmap = 1;
    }
    if (ShaderHasLightmapStage(shader)) {
        g_noLightmap = 1;
    }
    if (shader->lightmapIndex < 0) {
        g_noLightmap = 1;
    }
    if (shader->polygonOffset) {
        g_polygonOffset = 1;
    }
    if (shader->contentFlags & CONTENTS_WATER) {
        g_waterFog = 1;
        g_waterFogStrength = 0.10f;
    } else if (shader->contentFlags & CONTENTS_LAVA) {
        g_waterFog = 1;
        g_waterFogStrength = 0.15f;
    }

    for (i = 0; i < shader->numDeforms; i++) {
        const deformStage_t *ds = &shader->deforms[i];
        if (ds->deformation == DEFORM_WAVE || ds->deformation == DEFORM_NORMALS) {
            g_deformWaveDiv = ds->deformationSpread > 0.0f ? ds->deformationSpread : 1.0f;
            g_deformWaveFunc = MapWaveFunc(ds->deformationWave.func);
            g_deformWaveBase = ds->deformationWave.base;
            g_deformWaveAmp = ds->deformationWave.amplitude;
            g_deformWavePhase = ds->deformationWave.phase;
            g_deformWaveFreq = ds->deformationWave.frequency;
            g_deformType = ds->deformation;
            break;
        }
    }

    if (!stage) {
        return;
    }

    SetAlphaTestFromStage(stage);
    g_materialBlendEnabled = VK_StageUsesSourceAlphaBlend(stage) ? 1 : 0;

    g_stageIsLightmap = stage->bundle[0].isLightmap ? 1 : 0;
    g_rgbGenVertex = (stage->rgbGen == CGEN_VERTEX || stage->rgbGen == CGEN_EXACT_VERTEX ||
                      stage->rgbGen == CGEN_LIGHTING_DIFFUSE) ? 1 : 0;
    g_alphaGenVertex = (stage->alphaGen == AGEN_VERTEX || stage->alphaGen == AGEN_ONE_MINUS_VERTEX ||
                        stage->alphaGen == AGEN_CONST) ? 1 : 0;
    g_alphaGenPortal = (stage->alphaGen == AGEN_PORTAL) ? 1 : 0;
    g_alphaGenPortalRange = shader->portalRange > 0.0f ? shader->portalRange : 256.0f;

    g_alphaGenNormalZFade = (stage->alphaGen == AGEN_NORMALZFADE) ? 1 : 0;
    if (g_alphaGenNormalZFade) {
        g_normalZFadeBounds[0] = stage->zFadeBounds[0];
        g_normalZFadeBounds[1] = stage->zFadeBounds[1];
        g_normalZFadeMaxAlpha = stage->constantColor[3] / 255.0f;

        if (backEnd.currentEntity) {
            g_normalZFadeMaxAlpha *= backEnd.currentEntity->e.shaderRGBA[3] / 255.0f;

            if (VectorCompare(backEnd.currentEntity->e.fireRiseDir, vec3_origin)) {
                VectorSet(g_normalZFadeFireRiseDir, 0.0f, 0.0f, 1.0f);
            } else if (backEnd.currentEntity->e.hModel) {
                VectorRotate(backEnd.currentEntity->e.fireRiseDir, backEnd.currentEntity->e.axis, g_normalZFadeFireRiseDir);
            } else {
                VectorCopy(backEnd.currentEntity->e.fireRiseDir, g_normalZFadeFireRiseDir);
            }
        } else {
            VectorSet(g_normalZFadeFireRiseDir, 0.0f, 0.0f, 1.0f);
        }
    }
    g_tcGenEnvironment = (stage->bundle[0].tcGen == TCGEN_ENVIRONMENT_MAPPED ||
                          stage->bundle[0].tcGen == TCGEN_FIRERISEENV_MAPPED) ? 1 : 0;

    if (shader->polygonOffset && VK_StageUsesSourceAlphaBlend(stage)) {
        g_noLightmap = 1;
        g_sourceAlphaDecal = 1;
    }

    if (stage->bundle[0].numTexMods && stage->bundle[0].texMods) {
        for (i = 0; i < stage->bundle[0].numTexMods; i++) {
            const texModInfo_t *op = &stage->bundle[0].texMods[i];
            switch (op->type) {
            case TMOD_SCALE:
                g_tcModScale[0] = op->scale[0];
                g_tcModScale[1] = op->scale[1];
                break;
            case TMOD_SCROLL:
                g_tcModScroll[0] = op->scroll[0];
                g_tcModScroll[1] = op->scroll[1];
                break;
            case TMOD_ROTATE:
                g_tcModRotate = op->rotateSpeed;
                break;
            case TMOD_TRANSFORM:
                g_tcModTransformEnabled = 1;
                g_tcModTransform[0] = op->matrix[0][0];
                g_tcModTransform[1] = op->matrix[0][1];
                g_tcModTransform[2] = op->matrix[1][0];
                g_tcModTransform[3] = op->matrix[1][1];
                g_tcModTranslate[0] = op->translate[0];
                g_tcModTranslate[1] = op->translate[1];
                break;
            case TMOD_TURBULENT:
                g_tcModTurbEnabled = 1;
                g_tcModTurb[0] = op->wave.base;
                g_tcModTurb[1] = op->wave.amplitude;
                g_tcModTurb[2] = op->wave.phase;
                g_tcModTurb[3] = op->wave.frequency;
                break;
            case TMOD_STRETCH:
                g_tcModStretchEnabled = 1;
                g_tcModStretch[0] = MapWaveFunc(op->wave.func);
                g_tcModStretch[1] = op->wave.base;
                g_tcModStretch[2] = op->wave.amplitude;
                g_tcModStretch[3] = op->wave.phase;
                g_tcModStretch[4] = op->wave.frequency;
                break;
            default:
                break;
            }
        }
    }
}

void VK_SetUIStageStateFromShader(const shader_t *shader, const shaderStage_t *stage) {
    VK_SetStageStateFromShader(shader, stage);
    g_noLightmap = 1;
    g_stageIsLightmap = 0;
}

static float VK_EvalWaveFormAtTime(const waveForm_t *wf, float shaderTime) {
    float *table;
    int index;

    switch (wf->func) {
    case GF_SIN:
        table = tr.sinTable;
        break;
    case GF_TRIANGLE:
        table = tr.triangleTable;
        break;
    case GF_SQUARE:
        table = tr.squareTable;
        break;
    case GF_SAWTOOTH:
        table = tr.sawToothTable;
        break;
    case GF_INVERSE_SAWTOOTH:
        table = tr.inverseSawToothTable;
        break;
    default:
        table = tr.sinTable;
        break;
    }

    index = myftol((wf->phase + shaderTime * wf->frequency) * FUNCTABLE_SIZE);
    index &= FUNCTABLE_MASK;
    return wf->base + table[index] * wf->amplitude;
}

static float VK_EvalWaveFormClampedAtTime(const waveForm_t *wf, float shaderTime) {
    float glow = VK_EvalWaveFormAtTime(wf, shaderTime);

    if (glow < 0.0f) {
        return 0.0f;
    }
    if (glow > 1.0f) {
        return 1.0f;
    }
    return glow;
}

void VK_FillPicStageColors(const shader_t *shader, const shaderStage_t *stage,
                           const byte color0[4], const byte color1[4],
                           const byte color2[4], const byte color3[4],
                           byte outColors[4][4]) {
    const byte *inputs[4];
    float shaderTime;
    int i;
    byte waveColor[4];

    inputs[0] = color0;
    inputs[1] = color1;
    inputs[2] = color2;
    inputs[3] = color3;

    for (i = 0; i < 4; i++) {
        outColors[i][0] = inputs[i][0];
        outColors[i][1] = inputs[i][1];
        outColors[i][2] = inputs[i][2];
        outColors[i][3] = inputs[i][3];
    }

    if (!shader || !stage) {
        return;
    }

    if (stage->rgbGen == CGEN_IDENTITY || stage->rgbGen == CGEN_IDENTITY_LIGHTING) {
        byte v = (byte)(tr.identityLight * 255.0f);

        for (i = 0; i < 4; i++) {
            outColors[i][0] = v;
            outColors[i][1] = v;
            outColors[i][2] = v;
        }
        if (stage->alphaGen == AGEN_IDENTITY) {
            for (i = 0; i < 4; i++) {
                outColors[i][3] = 255;
            }
            return;
        }
    }

    shaderTime = backEnd.refdef.floatTime - shader->timeOffset;
    if (shader->clampTime && shaderTime >= shader->clampTime) {
        shaderTime = shader->clampTime;
    }

    if (stage->rgbGen == CGEN_WAVEFORM) {
        int v = myftol(255.0f * VK_EvalWaveFormClampedAtTime(&stage->rgbWave, shaderTime) * tr.identityLight);
        waveColor[0] = (byte)v;
        waveColor[1] = (byte)v;
        waveColor[2] = (byte)v;
        waveColor[3] = 255;
        for (i = 0; i < 4; i++) {
            outColors[i][0] = waveColor[0];
            outColors[i][1] = waveColor[1];
            outColors[i][2] = waveColor[2];
            outColors[i][3] = (byte)((inputs[i][3] * waveColor[3]) / 255);
        }
    } else if (stage->rgbGen == CGEN_CONST) {
        for (i = 0; i < 4; i++) {
            outColors[i][0] = stage->constantColor[0];
            outColors[i][1] = stage->constantColor[1];
            outColors[i][2] = stage->constantColor[2];
            outColors[i][3] = (byte)((inputs[i][3] * stage->constantColor[3]) / 255);
        }
    }

    if (stage->alphaGen == AGEN_WAVEFORM) {
        int a = myftol(255.0f * VK_EvalWaveFormClampedAtTime(&stage->alphaWave, shaderTime));
        for (i = 0; i < 4; i++) {
            outColors[i][3] = (byte)((outColors[i][3] * a) / 255);
        }
    } else if (stage->alphaGen == AGEN_CONST) {
        for (i = 0; i < 4; i++) {
            outColors[i][3] = (byte)((inputs[i][3] * stage->constantColor[3]) / 255);
        }
    }
}

int VK_PipelineForUIStage(const shaderStage_t *stage) {
    int pipe;

    if (!stage) {
        return VK_PIPELINE_DEPTH_DISABLED_ALPHA;
    }

    pipe = VK_PipelineForStage(stage);
    if (pipe == VK_PIPELINE_OPAQUE || pipe == VK_PIPELINE_ALPHA ||
        pipe == VK_PIPELINE_SRC_ALPHA_ONE ||
        pipe == VK_PIPELINE_ONE_ONE_MINUS_SRC_COLOR ||
        pipe == VK_PIPELINE_SRC_ALPHA_ONE_MINUS_SRC_COLOR ||
        pipe == VK_PIPELINE_ZERO_ONE_MINUS_SRC_ALPHA ||
        pipe == VK_PIPELINE_ZERO_ONE_MINUS_SRC_COLOR) {
        return VK_PIPELINE_DEPTH_DISABLED_ALPHA;
    }
    if (pipe >= VK_PIPELINE_ALPHA_DEPTHWRITE &&
        pipe <= VK_PIPELINE_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR_DEPTHWRITE) {
        return pipe - (VK_PIPELINE_ALPHA_DEPTHWRITE - VK_PIPELINE_ALPHA);
    }
    if (pipe == VK_PIPELINE_SKY || pipe == VK_PIPELINE_SHADOW_STENCIL) {
        return VK_PIPELINE_DEPTH_DISABLED_ALPHA;
    }
    return pipe;
}

int VK_PipelineFor2DPic(const shaderStage_t *stage) {
    unsigned stateBits;
    int srcBlend;
    int dstBlend;

    if (!stage) {
        return VK_PIPELINE_2D;
    }

    stateBits = stage->stateBits;
    srcBlend = stateBits & GLS_SRCBLEND_BITS;
    dstBlend = stateBits & GLS_DSTBLEND_BITS;

    if (!srcBlend && !dstBlend) {
        return VK_PIPELINE_2D_OPAQUE;
    }
    if (srcBlend == GLS_SRCBLEND_ONE && dstBlend == GLS_DSTBLEND_ONE) {
        return VK_PIPELINE_2D_ADDITIVE;
    }
    if (srcBlend == GLS_SRCBLEND_DST_COLOR &&
        (dstBlend == GLS_DSTBLEND_ZERO || !dstBlend)) {
        return VK_PIPELINE_2D_MODULATE;
    }
    if (srcBlend == GLS_SRCBLEND_ONE_MINUS_DST_COLOR &&
        dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR) {
        return VK_PIPELINE_2D_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR;
    }
    return VK_PIPELINE_2D;
}

void VK_FillPushConstants(const float mvp[16], const shader_t *shader, vk_push_constants_t *pc) {
    float timeSec;

    if (!pc) {
        return;
    }

    memcpy(pc->mvp, mvp, sizeof(float) * 16);
    memset(pc->params, 0, sizeof(pc->params));

    timeSec = backEnd.refdef.floatTime;
    if (shader) {
        timeSec -= shader->timeOffset;
        if (shader->clampTime && timeSec >= shader->clampTime) {
            timeSec = shader->clampTime;
        }
    }

    pc->params[0][0] = 0.0f;
    pc->params[0][1] = 1.0f;
    pc->params[0][2] = (float)g_alphaTestEnabled;
    pc->params[0][3] = g_alphaTestRef;

    pc->params[1][0] = g_deformWaveDiv;
    pc->params[1][1] = g_deformWaveFunc;
    pc->params[1][2] = g_deformWaveBase;
    pc->params[1][3] = g_deformWaveAmp;

    pc->params[2][0] = g_deformWavePhase;
    pc->params[2][1] = g_deformWaveFreq;
    pc->params[2][2] = (float)g_deformType;
    /* Negative-frequency wave deformation (used by entityOnFire shaders) scales
     * the offset along the fire-rise direction.  params[2][3] becomes the
     * world-space Z scale factor for that path; for all other deform modes it
     * simply flags that a deformation is active. */
    if ( g_deformWaveFreq < 0.0f && backEnd.currentEntity ) {
        pc->params[2][3] = 0.4f + 0.6f * fabs( backEnd.currentEntity->e.fireRiseDir[2] );
    } else {
        pc->params[2][3] = (g_deformType != 0) ? 1.0f : 0.0f;
    }

    pc->params[3][0] = g_tcModTransform[0];
    pc->params[3][1] = g_tcModTransform[1];
    pc->params[3][2] = g_tcModTransform[2];
    pc->params[3][3] = g_tcModTransform[3];

    pc->params[4][0] = g_tcModTranslate[0];
    pc->params[4][1] = g_tcModTranslate[1];
    pc->params[4][2] = (float)g_tcModTransformEnabled;
    pc->params[4][3] = (float)g_tcModTurbEnabled;

    pc->params[5][0] = g_tcModTurb[0];
    pc->params[5][1] = g_tcModTurb[1];
    pc->params[5][2] = g_tcModTurb[2];
    pc->params[5][3] = g_tcModTurb[3];

    pc->params[6][0] = g_tcModScale[0];
    pc->params[6][1] = g_tcModScale[1];
    pc->params[6][2] = g_tcModScroll[0];
    pc->params[6][3] = g_tcModScroll[1];

    pc->params[7][0] = (float)g_rgbGenVertex;
    pc->params[7][1] = (float)g_alphaGenVertex;
    pc->params[7][2] = (float)g_noLightmap;
    pc->params[7][3] = (float)g_tcGenEnvironment;

    pc->params[8][0] = g_tcModRotate;
    pc->params[8][1] = timeSec;
    pc->params[8][2] = (float)g_stageIsLightmap;

    pc->params[9][0] = (float)g_alphaTestFunc;
    pc->params[9][1] = (float)g_materialBlendEnabled;
    pc->params[9][2] = (float)g_sourceAlphaDecal;
    pc->params[9][3] = g_waterFogStrength;

    pc->params[10][0] = g_tcModStretch[0];
    pc->params[10][1] = g_tcModStretch[1];
    pc->params[10][2] = g_tcModStretch[2];
    pc->params[10][3] = g_tcModStretch[3];

    pc->params[11][0] = g_tcModStretch[4];
    pc->params[11][1] = (float)g_tcModStretchEnabled;
    pc->params[11][2] = (float)g_waterFog;

    pc->params[12][0] = g_normalZFadeBounds[0];
    pc->params[12][1] = g_normalZFadeBounds[1];
    pc->params[12][2] = (float)g_alphaGenPortal;
    pc->params[12][3] = g_alphaGenPortalRange;

    pc->params[13][0] = backEnd.viewParms.portalPlane.normal[0];
    pc->params[13][1] = backEnd.viewParms.portalPlane.normal[1];
    pc->params[13][2] = backEnd.viewParms.portalPlane.normal[2];
    pc->params[13][3] = backEnd.viewParms.portalPlane.dist;

    pc->params[14][0] = backEnd.or.viewOrigin[0];
    pc->params[14][1] = backEnd.or.viewOrigin[1];
    pc->params[14][2] = backEnd.or.viewOrigin[2];
    pc->params[14][3] = 0.0f;    /* dlight pass flag; keep clear for normal draws */

    pc->params[15][0] = g_normalZFadeFireRiseDir[0];
    pc->params[15][1] = g_normalZFadeFireRiseDir[1];
    pc->params[15][2] = g_normalZFadeFireRiseDir[2];
    pc->params[15][3] = g_normalZFadeMaxAlpha;

    VK_FillFogPushConstants(pc);

    if (g_alphaGenNormalZFade) {
        static int logCount = 0;
        if (logCount < 20) {
            logCount++;
            ri.Printf(PRINT_ALL, "[NZF] bounds %.2f %.2f dir %.2f %.2f %.2f maxAlpha %.3f entAlpha %d shader %s\n",
                      g_normalZFadeBounds[0], g_normalZFadeBounds[1],
                      g_normalZFadeFireRiseDir[0], g_normalZFadeFireRiseDir[1], g_normalZFadeFireRiseDir[2],
                      g_normalZFadeMaxAlpha,
                      backEnd.currentEntity ? backEnd.currentEntity->e.shaderRGBA[3] : 0,
                      shader ? shader->name : "?");
        }
    }
}

void VK_FillFogPushConstants(vk_push_constants_t *pc) {
    glfog_t *curfog = NULL;
    float start, end, density;
    int mode;

    if (!pc) {
        return;
    }

    /* params[16..17] are reserved for fog; default to off. */
    memset(pc->params[VK_FOG_COLOR_PARAM], 0, sizeof(float) * 4);
    memset(pc->params[VK_FOG_RANGE_PARAM], 0, sizeof(float) * 4);

    if (!r_vkDistanceFog) {
        r_vkDistanceFog = ri.Cvar_Get("r_vkDistanceFog", "1", CVAR_ARCHIVE);
    }
    if (!r_vkDistanceFog->integer) {
        return;
    }

    if (!r_wolffog || !r_wolffog->integer) {
        return;
    }

    if (backEnd.refdef.rdflags & RDF_NOWORLDMODEL) {
        return;
    }

    /* Mirror SetIteratorFog logic from tr_shade.c. */
    if (backEnd.refdef.rdflags & RDF_DRAWINGSKY) {
        if (glfogsettings[FOG_SKY].registered) {
            curfog = &glfogsettings[FOG_SKY];
        }
    } else if (skyboxportal && (backEnd.refdef.rdflags & RDF_SKYBOXPORTAL)) {
        if (glfogsettings[FOG_PORTALVIEW].registered) {
            curfog = &glfogsettings[FOG_PORTALVIEW];
        }
    } else {
        if (glfogNum > FOG_NONE && glfogsettings[FOG_CURRENT].registered) {
            curfog = &glfogsettings[FOG_CURRENT];
        } else if (glfogNum > FOG_NONE && glfogsettings[FOG_TARGET].registered) {
            /* FOG_CURRENT may not be refreshed yet if R_SetFog was called after
             * R_SetFrameFog for this view. Fall back to FOG_TARGET so distance
             * fog is active immediately, matching OpenGL's per-draw-call fog. */
            curfog = &glfogsettings[FOG_TARGET];
        }
    }

    if (!curfog) {
        return;
    }

    if (curfog->mode == GL_EXP) {
        mode = 2;
    } else {
        mode = 1; /* GL_LINEAR or default. */
    }

    /* Match R_Fog defaults and overrides. */
    density = curfog->density > 0.0f ? curfog->density : 1.0f;

    if (backEnd.refdef.rdflags & RDF_SNOOPERVIEW) {
        start = curfog->end;
        end = curfog->end + 1000.0f;
    } else {
        start = curfog->start;
        if (r_zfar && r_zfar->value) {
            end = r_zfar->value;
        } else {
            end = curfog->end;
        }
    }

    pc->params[VK_FOG_COLOR_PARAM][0] = curfog->color[0];
    pc->params[VK_FOG_COLOR_PARAM][1] = curfog->color[1];
    pc->params[VK_FOG_COLOR_PARAM][2] = curfog->color[2];
    pc->params[VK_FOG_COLOR_PARAM][3] = (float)mode;

    pc->params[VK_FOG_RANGE_PARAM][0] = start;
    pc->params[VK_FOG_RANGE_PARAM][1] = end;
    pc->params[VK_FOG_RANGE_PARAM][2] = density;
    pc->params[VK_FOG_RANGE_PARAM][3] = 1.0f; /* registered/active flag */
}

void VK_SetSkyPushConstants(const shader_t *shader, const shaderStage_t *stage,
                            vk_push_constants_t *pc, qboolean cloudLayer) {
    float timeSec;

    if (!shader || !pc) {
        return;
    }

    timeSec = pc->params[8][1];
    pc->params[11][3] = 1.0f;
    pc->params[12][0] = shader->sky.cloudHeight > 0.0f ? shader->sky.cloudHeight : 512.0f;
    pc->params[12][1] = cloudLayer ? 1.0f : 0.0f;

    /*
     * params1.x selects the sky rgbGen mode in world.frag.
     * FillPushConstants leaves params1.x set to the deform wave divisor,
     * which would be misread as rgbMode 1 and zero out the cloud color.
     * Default to mode 0 (no modification) and override for const/waveform.
     */
    pc->params[1][0] = 0.0f;

    if (stage) {
        if (stage->rgbGen == CGEN_CONST) {
            pc->params[1][0] = 1.0f;
            pc->params[1][1] = stage->constantColor[0] / 255.0f;
            pc->params[1][2] = stage->constantColor[1] / 255.0f;
            pc->params[1][3] = stage->constantColor[2] / 255.0f;
        } else if (stage->rgbGen == CGEN_WAVEFORM) {
            pc->params[1][0] = 2.0f;
            pc->params[2][0] = MapWaveFunc(stage->rgbWave.func);
            pc->params[2][1] = stage->rgbWave.base;
            pc->params[2][2] = stage->rgbWave.amplitude;
            pc->params[2][3] = stage->rgbWave.phase;
            pc->params[8][3] = stage->rgbWave.frequency;
        }
        (void)timeSec;
    }

    if (shader->fogParms.depthForOpaque > 0.0f) {
        pc->params[3][0] = shader->fogParms.color[0];
        pc->params[3][1] = shader->fogParms.color[1];
        pc->params[3][2] = shader->fogParms.color[2];
        pc->params[3][3] = 0.35f;
    } else {
        /*
         * FillPushConstants stores the texture-coordinate transform matrix
         * in params3. world.frag interprets params3.xyz as a sky fog color
         * whenever params3.w > 0, so a default identity matrix (1,0,0,1)
         * tints the sky red. Disable the sky-fog branch when no fog is set.
         */
        pc->params[3][3] = 0.0f;
    }
}

image_t *VK_BundleImage(const textureBundle_t *bundle, const shader_t *shader) {
    float shaderTime;
    int index;

    if (!bundle || !bundle->image[0]) {
        return tr.whiteImage;
    }

    if (bundle->isLightmap) {
        if (shader && shader->lightmapIndex >= 0 && shader->lightmapIndex < tr.numLightmaps) {
            return tr.lightmaps[shader->lightmapIndex];
        }
        return tr.whiteImage;
    }

    if (bundle->isVideoMap) {
        ri.CIN_RunCinematic(bundle->videoMapHandle);
        ri.CIN_UploadCinematic(bundle->videoMapHandle);
        return bundle->image[0] ? bundle->image[0] : tr.whiteImage;
    }

    if (bundle->numImageAnimations <= 1) {
        if (bundle->isLightmap && (backEnd.refdef.rdflags & RDF_SNOOPERVIEW)) {
            return tr.whiteImage;
        }
        return bundle->image[0];
    }

    shaderTime = backEnd.refdef.floatTime;
    if (shader) {
        shaderTime -= shader->timeOffset;
        if (shader->clampTime && shaderTime >= shader->clampTime) {
            shaderTime = shader->clampTime;
        }
    }

    index = myftol(shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE);
    index >>= FUNCTABLE_SIZE2;
    if (index < 0) {
        index = 0;
    }
    index %= bundle->numImageAnimations;
    return bundle->image[index] ? bundle->image[index] : tr.whiteImage;
}

image_t *VK_StageLightmapImage(const shader_t *shader) {
    if (g_noLightmap || g_stageIsLightmap) {
        return tr.whiteImage;
    }
    if (shader && shader->lightmapIndex >= 0 && shader->lightmapIndex < tr.numLightmaps) {
        return tr.lightmaps[shader->lightmapIndex];
    }
    return tr.whiteImage;
}

VkDescriptorSet VK_StageDescriptorSet(const shader_t *shader, const shaderStage_t *stage) {
    image_t *base;
    image_t *light;

    if (!stage) {
        return VK_GetDescriptorSetForImages(tr.whiteImage, tr.whiteImage);
    }

    base = VK_BundleImage(&stage->bundle[0], shader);
    /* Use the explicit lightmap in bundle[1] when present (produced by OpenGL's
     * CollapseMultitexture for lightmap+base shaders).  Falling back to the
     * implicit surface lightmap keeps non-collapsed shaders working. */
    if (stage->bundle[1].image[0] && stage->bundle[1].isLightmap) {
        light = VK_BundleImage(&stage->bundle[1], shader);
    } else {
        light = VK_StageLightmapImage(shader);
    }
    return VK_GetDescriptorSetForImages(base, light);
}

int VK_MaterialPolygonOffset(void) {
    return g_polygonOffset;
}

float VK_MaterialPolyOffsetFactor(void) {
    return g_polyOffsetFactor;
}

float VK_MaterialPolyOffsetUnits(void) {
    return g_polyOffsetUnits;
}

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define BSP_IDENT (('P' << 24) + ('S' << 16) + ('B' << 8) + 'I')
#define BSP_VERSION 47

#define LUMP_DRAWVERTS 10
#define LUMP_DRAWINDEXES 11
#define LUMP_LIGHTMAPS 14
#define LUMP_SURFACES 13
#define HEADER_LUMPS 17

#define MST_PLANAR 1
#define MST_PATCH 2
#define MST_TRIANGLE_SOUP 3
#define MAX_QPATH 64
#define MAX_EXTRA_STAGES 6
#define MAX_FULL_STAGES 16
#define MAX_DEFORM_WAVES 4
#define MAX_DEFORM_MOVES 4
#define SURF_GLASS 0x200000
#define CONTENTS_TRANSLUCENT 0x20000000
#define ALPHA_FUNC_NONE 0
#define ALPHA_FUNC_GT0 1
#define ALPHA_FUNC_LT128 2
#define ALPHA_FUNC_GE128 3
#define WAVE_SIN 0
#define WAVE_TRIANGLE 1
#define WAVE_SAWTOOTH 2
#define WAVE_INVERSE_SAWTOOTH 3
#define WAVE_SQUARE 4

typedef struct {
    int32_t fileofs;
    int32_t filelen;
} lump_t;

typedef struct {
    int32_t ident;
    int32_t version;
    lump_t lumps[HEADER_LUMPS];
} dheader_t;

typedef struct {
    float xyz[3];
    float st[2];
    float lightmap[2];
    float normal[3];
    uint8_t color[4];
} drawVert_t;

typedef struct {
    int32_t shaderNum;
    int32_t fogNum;
    int32_t surfaceType;
    int32_t firstVert;
    int32_t numVerts;
    int32_t firstIndex;
    int32_t numIndexes;
    int32_t lightmapNum;
    int32_t lightmapX;
    int32_t lightmapY;
    int32_t lightmapWidth;
    int32_t lightmapHeight;
    float lightmapOrigin[3];
    float lightmapVecs[3][3];
    int32_t patchWidth;
    int32_t patchHeight;
} dsurface_t;

typedef struct {
    char shader[MAX_QPATH];
    int32_t surfaceFlags;
    int32_t contentFlags;
} dshader_t;

typedef struct {
    char *textureName;
    GLuint textureHandle;
    char **animFrames;
    GLuint *animFrameTextures;
    int numAnimFrames;
    float animFps;
    GLenum blendSrc;
    GLenum blendDst;
    int hasBlend;
    int hasAlphaFunc;
    float alphaRef;
    int alphaFuncMode;
    int depthWrite;
    int depthFuncEqual;
    float tcScale[2];
    float tcScroll[2];
    float tcRotate;
    int hasTcScale;
    int hasTcScroll;
    int hasTcRotate;
    int hasTcSwap;
    int hasTcTurb;
    int hasTcStretch;
    int hasTcTransform;
    float tcTurbBase;
    float tcTurbAmp;
    float tcTurbPhase;
    float tcTurbFreq;
    float tcStretchBase;
    float tcStretchAmp;
    float tcStretchPhase;
    float tcStretchFreq;
    float tcTransform[2][2];
    float tcTranslate[2];
    int hasTcGenVector;
    int tcGenEnvironment;
    int tcGenLightmap;
    float tcGenVecS[3];
    float tcGenVecT[3];
    int rgbGenVertex;
    int rgbGenConst;
    int rgbGenWave;
    float rgbConst[3];
    float rgbWaveBase;
    float rgbWaveAmp;
    float rgbWavePhase;
    float rgbWaveFreq;
    int alphaGenVertex;
    int alphaGenConst;
    int alphaGenWave;
    float alphaConst;
    float alphaWaveBase;
    float alphaWaveAmp;
    float alphaWavePhase;
    float alphaWaveFreq;
} shader_stage_t;

typedef struct {
    char *shaderName;
    char *textureName;
    char *editorTextureName;
    GLuint textureHandle;
    char *secondaryTextureName;
    GLuint secondaryTextureHandle;
    char **secondaryAnimFrames;
    GLuint *secondaryAnimFrameTextures;
    int secondaryNumAnimFrames;
    float secondaryAnimFps;
    GLenum secondaryBlendSrc;
    GLenum secondaryBlendDst;
    int secondaryHasAlphaFunc;
    float secondaryAlphaRef;
    int secondaryAlphaFuncMode;
    int secondaryDepthWrite;
    int secondaryDepthFuncEqual;
    int hasSecondary;
    int secondaryStageScore;
    char *extraStageTextureName[MAX_EXTRA_STAGES];
    GLuint extraStageTextureHandle[MAX_EXTRA_STAGES];
    char **extraStageAnimFrames[MAX_EXTRA_STAGES];
    GLuint *extraStageAnimFrameTextures[MAX_EXTRA_STAGES];
    int extraStageNumAnimFrames[MAX_EXTRA_STAGES];
    float extraStageAnimFps[MAX_EXTRA_STAGES];
    GLenum extraStageBlendSrc[MAX_EXTRA_STAGES];
    GLenum extraStageBlendDst[MAX_EXTRA_STAGES];
    int extraHasAlphaFunc[MAX_EXTRA_STAGES];
    float extraAlphaRef[MAX_EXTRA_STAGES];
    int extraAlphaFuncMode[MAX_EXTRA_STAGES];
    int extraHasBlend[MAX_EXTRA_STAGES];
    int extraDepthWrite[MAX_EXTRA_STAGES];
    int extraDepthFuncEqual[MAX_EXTRA_STAGES];
    float extraTcScale[MAX_EXTRA_STAGES][2];
    float extraTcScroll[MAX_EXTRA_STAGES][2];
    float extraTcRotate[MAX_EXTRA_STAGES];
    int extraHasTcScale[MAX_EXTRA_STAGES];
    int extraHasTcScroll[MAX_EXTRA_STAGES];
    int extraHasTcRotate[MAX_EXTRA_STAGES];
    int extraHasTcSwap[MAX_EXTRA_STAGES];
    int extraHasTcTurb[MAX_EXTRA_STAGES];
    int extraHasTcStretch[MAX_EXTRA_STAGES];
    int extraHasTcTransform[MAX_EXTRA_STAGES];
    float extraTcTurbBase[MAX_EXTRA_STAGES];
    float extraTcTurbAmp[MAX_EXTRA_STAGES];
    float extraTcTurbPhase[MAX_EXTRA_STAGES];
    float extraTcTurbFreq[MAX_EXTRA_STAGES];
    float extraTcStretchBase[MAX_EXTRA_STAGES];
    float extraTcStretchAmp[MAX_EXTRA_STAGES];
    float extraTcStretchPhase[MAX_EXTRA_STAGES];
    float extraTcStretchFreq[MAX_EXTRA_STAGES];
    float extraTcTransform[MAX_EXTRA_STAGES][2][2];
    float extraTcTranslate[MAX_EXTRA_STAGES][2];
    int extraHasTcGenVector[MAX_EXTRA_STAGES];
    int extraTcGenEnvironment[MAX_EXTRA_STAGES];
    float extraTcGenVecS[MAX_EXTRA_STAGES][3];
    float extraTcGenVecT[MAX_EXTRA_STAGES][3];
    int extraRgbGenVertex[MAX_EXTRA_STAGES];
    int extraRgbGenConst[MAX_EXTRA_STAGES];
    float extraRgbConst[MAX_EXTRA_STAGES][3];
    int extraAlphaGenVertex[MAX_EXTRA_STAGES];
    int extraAlphaGenConst[MAX_EXTRA_STAGES];
    int extraRgbGenWave[MAX_EXTRA_STAGES];
    int extraAlphaGenWave[MAX_EXTRA_STAGES];
    float extraAlphaConst[MAX_EXTRA_STAGES];
    float extraRgbWaveBase[MAX_EXTRA_STAGES];
    float extraRgbWaveAmp[MAX_EXTRA_STAGES];
    float extraRgbWavePhase[MAX_EXTRA_STAGES];
    float extraRgbWaveFreq[MAX_EXTRA_STAGES];
    float extraAlphaWaveBase[MAX_EXTRA_STAGES];
    float extraAlphaWaveAmp[MAX_EXTRA_STAGES];
    float extraAlphaWavePhase[MAX_EXTRA_STAGES];
    float extraAlphaWaveFreq[MAX_EXTRA_STAGES];
    int numExtraStages;
    char **animFrames;
    GLuint *animFrameTextures;
    int numAnimFrames;
    float animFps;
    float tcScale[2];
    float tcScroll[2];
    float tcRotateDegPerSec;
    float tcTurbBase;
    float tcTurbAmp;
    float tcTurbPhase;
    float tcTurbFreq;
    float tcStretchBase;
    float tcStretchAmp;
    float tcStretchPhase;
    float tcStretchFreq;
    float tcTransform[2][2];
    float tcTranslate[2];
    int hasTcScale;
    int hasTcScroll;
    int hasTcRotate;
    int hasTcTurb;
    int hasTcStretch;
    int hasTcTransform;
    int hasTcSwap;
    int hasTcGenVector;
    float tcGenVecS[3];
    float tcGenVecT[3];
    int tcGenEnvironment;
    int rgbGenIdentity;
    int rgbGenVertex;
    int rgbGenConst;
    int rgbGenWave;
    float rgbConst[3];
    float rgbWaveBase, rgbWaveAmp, rgbWavePhase, rgbWaveFreq;
    int alphaGenIdentity;
    int alphaGenVertex;
    int alphaGenConst;
    int alphaGenWave;
    float alphaConst;
    float alphaWaveBase, alphaWaveAmp, alphaWavePhase, alphaWaveFreq;
    GLenum blendSrc;
    GLenum blendDst;
    int hasBlend;
    int hasAlphaFunc;
    float alphaRef;
    int alphaFuncMode;
    int depthWrite;
    int depthFuncEqual;
    int polygonOffset;
    int cullNone;
    int sortDecal;
    int sortTransparent;
    int surfaceParmTrans;
    int surfaceParmNoLightmap;
    int numDeformWaves;
    float deformSpread[MAX_DEFORM_WAVES];
    int deformFunc[MAX_DEFORM_WAVES];
    float deformBase[MAX_DEFORM_WAVES];
    float deformAmp[MAX_DEFORM_WAVES];
    float deformPhase[MAX_DEFORM_WAVES];
    float deformFreq[MAX_DEFORM_WAVES];
    int numDeformMoves;
    float deformMoveVec[MAX_DEFORM_MOVES][3];
    int deformMoveFunc[MAX_DEFORM_MOVES];
    float deformMoveBase[MAX_DEFORM_MOVES];
    float deformMoveAmp[MAX_DEFORM_MOVES];
    float deformMovePhase[MAX_DEFORM_MOVES];
    float deformMoveFreq[MAX_DEFORM_MOVES];
    int deformAutoSprite;
    int deformAutoSprite2;
    int deformTextMode;
    int surfaceParmWater;
    int numFullStages;
    shader_stage_t fullStages[MAX_FULL_STAGES];
    int selectedStageScore;
} material_info_t;

typedef struct {
    drawVert_t *verts;
    int32_t *indices;
    dsurface_t *surfaces;
    uint32_t **surfaceIndices;
    int *surfaceIndexCounts;
    dshader_t *shaders;
    GLuint *shaderTextures;
    GLuint *lightmapTextures;
    int numVerts;
    int numIndices;
    int numSurfaces;
    int numShaders;
    uint8_t *lightmapData;
    int numLightmaps;
    material_info_t *materials;
    int *shaderMaterialIndex;
    int numMaterials;
    float (*surfaceCenters)[3];
    int *transparentSurfaceOrder;
    float *transparentSurfaceDist2;
    char gameRoot[1024];
} bsp_data_t;

static float g_camPos[3] = {0.0f, 0.0f, 128.0f};
static float g_yaw = 0.0f;
static float g_pitch = 0.0f;

static void BuildGameRootFromBspPath(const char *bspPath, char *out, size_t outSize) {
    const char *maps = strstr(bspPath, "/maps/");
    if (maps) {
        size_t n = (size_t)(maps - bspPath);
        if (n >= outSize) n = outSize - 1;
        memcpy(out, bspPath, n);
        out[n] = '\0';
        return;
    }
    if (outSize > 0) {
        out[0] = '.';
        if (outSize > 1) out[1] = '\0';
    }
}

static GLuint LoadTexture2D(const char *path) {
    int w, h, comp;
    unsigned char *pixels = stbi_load(path, &w, &h, &comp, STBI_rgb_alpha);
    GLuint tex = 0;
    if (!pixels) return 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    stbi_image_free(pixels);
    return tex;
}

static char *DupString(const char *s) {
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

static int StrIEqual(const char *a, const char *b) {
    if (!a || !b) return 0;
    return strcasecmp(a, b) == 0;
}

static void FreeShaderStage(shader_stage_t *st) {
    if (!st) return;
    free(st->textureName);
    if (st->textureHandle != 0) glDeleteTextures(1, &st->textureHandle);
    if (st->animFrames) {
        for (int i = 0; i < st->numAnimFrames; i++) free(st->animFrames[i]);
    }
    if (st->animFrameTextures) {
        for (int i = 0; i < st->numAnimFrames; i++) {
            if (st->animFrameTextures[i] != 0) glDeleteTextures(1, &st->animFrameTextures[i]);
        }
    }
    free(st->animFrames);
    free(st->animFrameTextures);
    memset(st, 0, sizeof(*st));
}

static void AppendSecondaryAsExtra(material_info_t *mat) {
    if (!mat || !mat->hasSecondary) return;
    if (!mat->secondaryTextureName && mat->secondaryNumAnimFrames <= 0) return;
    if (mat->numExtraStages >= MAX_EXTRA_STAGES) return;
    {
        int ei = mat->numExtraStages++;
        mat->extraStageTextureName[ei] = mat->secondaryTextureName ? DupString(mat->secondaryTextureName) : NULL;
        if (mat->secondaryTextureName && !mat->extraStageTextureName[ei]) {
            mat->numExtraStages--;
            return;
        }
        mat->extraStageBlendSrc[ei] = mat->secondaryBlendSrc;
        mat->extraStageBlendDst[ei] = mat->secondaryBlendDst;
        mat->extraStageNumAnimFrames[ei] = mat->secondaryNumAnimFrames;
        mat->extraStageAnimFps[ei] = mat->secondaryAnimFps;
        mat->extraStageAnimFrames[ei] = NULL;
        mat->extraStageAnimFrameTextures[ei] = NULL;
        if (mat->secondaryNumAnimFrames > 0 && mat->secondaryAnimFrames) {
            mat->extraStageAnimFrames[ei] = (char **)calloc((size_t)mat->secondaryNumAnimFrames, sizeof(char *));
            mat->extraStageAnimFrameTextures[ei] = (GLuint *)calloc((size_t)mat->secondaryNumAnimFrames, sizeof(GLuint));
            if (mat->extraStageAnimFrames[ei] && mat->extraStageAnimFrameTextures[ei]) {
                for (int i = 0; i < mat->secondaryNumAnimFrames; i++) {
                    mat->extraStageAnimFrames[ei][i] = DupString(mat->secondaryAnimFrames[i]);
                    if (!mat->extraStageAnimFrames[ei][i]) {
                        mat->extraStageNumAnimFrames[ei] = i;
                        break;
                    }
                }
            } else {
                free(mat->extraStageAnimFrames[ei]);
                free(mat->extraStageAnimFrameTextures[ei]);
                mat->extraStageAnimFrames[ei] = NULL;
                mat->extraStageAnimFrameTextures[ei] = NULL;
                mat->extraStageNumAnimFrames[ei] = 0;
                mat->extraStageAnimFps[ei] = 0.0f;
            }
        }
        mat->extraHasBlend[ei] = !((mat->secondaryBlendSrc == GL_ONE) && (mat->secondaryBlendDst == GL_ZERO));
        mat->extraHasAlphaFunc[ei] = mat->secondaryHasAlphaFunc;
        mat->extraAlphaRef[ei] = mat->secondaryAlphaRef;
        mat->extraAlphaFuncMode[ei] = mat->secondaryAlphaFuncMode;
    }
}

static int LooksLikeImagePath(const char *path) {
    size_t n = strlen(path);
    if (n < 4) return 0;
    return StrIEqual(path + n - 4, ".tga") || StrIEqual(path + n - 4, ".jpg") || StrIEqual(path + n - 4, ".png");
}

static GLuint LoadShaderImagePathEx(const char *gameRoot, const char *shaderImagePath, int alphaSensitive) {
    static const char *exts[] = { ".tga", ".jpg", ".png" };
    static const char *extsAlphaSafe[] = { ".tga", ".png" };
    char full[1400];
    GLuint t = 0;
    if (LooksLikeImagePath(shaderImagePath)) {
        const char *dot = strrchr(shaderImagePath, '.');
        snprintf(full, sizeof(full), "%s/%s", gameRoot, shaderImagePath);
        t = LoadTexture2D(full);
        if (t != 0) return t;
        /* Fallback: if explicit extension is missing on disk, try sibling formats. */
        if (dot && dot > shaderImagePath) {
            char base[1400];
            size_t baseLen = (size_t)(dot - shaderImagePath);
            if (baseLen >= sizeof(base)) baseLen = sizeof(base) - 1;
            memcpy(base, shaderImagePath, baseLen);
            base[baseLen] = '\0';
            for (int i = 0; i < (alphaSensitive ? 2 : 3); i++) {
                const char *ext = alphaSensitive ? extsAlphaSafe[i] : exts[i];
                snprintf(full, sizeof(full), "%s/%s%s", gameRoot, base, ext);
                t = LoadTexture2D(full);
                if (t != 0) return t;
            }
        }
        return 0;
    }
    for (int i = 0; i < 3; i++) {
        snprintf(full, sizeof(full), "%s/%s%s", gameRoot, shaderImagePath, exts[i]);
        {
            GLuint t = LoadTexture2D(full);
            if (t != 0) return t;
        }
    }
    return 0;
}

static GLuint LoadShaderImagePath(const char *gameRoot, const char *shaderImagePath) {
    return LoadShaderImagePathEx(gameRoot, shaderImagePath, 0);
}

static material_info_t *FindMaterialInfo(const bsp_data_t *bsp, const char *shaderName) {
    for (int i = 0; i < bsp->numMaterials; i++) {
        if (strcmp(bsp->materials[i].shaderName, shaderName) == 0) return &bsp->materials[i];
    }
    return NULL;
}

static int BuildSurfaceIndexCache(bsp_data_t *bsp) {
    bsp->surfaceIndices = (uint32_t **)calloc((size_t)bsp->numSurfaces, sizeof(uint32_t *));
    bsp->surfaceIndexCounts = (int *)calloc((size_t)bsp->numSurfaces, sizeof(int));
    if (!bsp->surfaceIndices || !bsp->surfaceIndexCounts) return 0;
    for (int si = 0; si < bsp->numSurfaces; si++) {
        const dsurface_t *s = &bsp->surfaces[si];
        uint32_t *idx = NULL;
        int count = 0;
        if (s->surfaceType == MST_PLANAR || s->surfaceType == MST_TRIANGLE_SOUP) {
            if (s->firstIndex < 0 || s->numIndexes <= 0 || s->firstIndex + s->numIndexes > bsp->numIndices ||
                s->numVerts <= 0) {
                continue;
            }
            idx = (uint32_t *)malloc((size_t)s->numIndexes * sizeof(uint32_t));
            if (!idx) return 0;
            for (int i = 0; i + 2 < s->numIndexes; i += 3) {
                int a = bsp->indices[s->firstIndex + i + 0];
                int b = bsp->indices[s->firstIndex + i + 1];
                int c = bsp->indices[s->firstIndex + i + 2];
                if (a < 0 || b < 0 || c < 0 || a >= s->numVerts || b >= s->numVerts || c >= s->numVerts) continue;
                idx[count++] = (uint32_t)a;
                idx[count++] = (uint32_t)b;
                idx[count++] = (uint32_t)c;
            }
        } else if (s->surfaceType == MST_PATCH) {
            int w = s->patchWidth, h = s->patchHeight;
            if (w < 2 || h < 2 || s->numVerts <= 0) continue;
            idx = (uint32_t *)malloc((size_t)(w - 1) * (size_t)(h - 1) * 6u * sizeof(uint32_t));
            if (!idx) return 0;
            for (int y = 0; y < h - 1; y++) {
                for (int x = 0; x < w - 1; x++) {
                    int v00 = y * w + x;
                    int v10 = y * w + x + 1;
                    int v01 = (y + 1) * w + x;
                    int v11 = (y + 1) * w + x + 1;
                    if (v00 >= s->numVerts || v10 >= s->numVerts || v01 >= s->numVerts || v11 >= s->numVerts) continue;
                    idx[count++] = (uint32_t)v00; idx[count++] = (uint32_t)v10; idx[count++] = (uint32_t)v11;
                    idx[count++] = (uint32_t)v00; idx[count++] = (uint32_t)v11; idx[count++] = (uint32_t)v01;
                }
            }
        }
        if (idx && count > 0) {
            uint32_t *shrunk = (uint32_t *)realloc(idx, (size_t)count * sizeof(uint32_t));
            bsp->surfaceIndices[si] = shrunk ? shrunk : idx;
            bsp->surfaceIndexCounts[si] = count;
        } else {
            free(idx);
        }
    }
    return 1;
}

static material_info_t *FindOrAddMaterialInfo(bsp_data_t *bsp, const char *shaderName) {
    material_info_t *mat = FindMaterialInfo(bsp, shaderName);
    if (mat) return mat;
    mat = (material_info_t *)realloc(bsp->materials, (size_t)(bsp->numMaterials + 1) * sizeof(material_info_t));
    if (!mat) return NULL;
    bsp->materials = mat;
    mat = &bsp->materials[bsp->numMaterials];
    memset(mat, 0, sizeof(*mat));
    mat->shaderName = DupString(shaderName);
    mat->tcScale[0] = mat->tcScale[1] = 1.0f;
    mat->tcGenVecS[0] = 1.0f; mat->tcGenVecS[1] = 0.0f; mat->tcGenVecS[2] = 0.0f;
    mat->tcGenVecT[0] = 0.0f; mat->tcGenVecT[1] = 1.0f; mat->tcGenVecT[2] = 0.0f;
    mat->tcTransform[0][0] = 1.0f; mat->tcTransform[0][1] = 0.0f;
    mat->tcTransform[1][0] = 0.0f; mat->tcTransform[1][1] = 1.0f;
    mat->blendSrc = GL_ONE;
    mat->blendDst = GL_ZERO;
    mat->secondaryBlendSrc = GL_ONE;
    mat->secondaryBlendDst = GL_ZERO;
    mat->secondaryHasAlphaFunc = 0;
    mat->secondaryAlphaRef = 0.5f;
    mat->secondaryAlphaFuncMode = ALPHA_FUNC_NONE;
    mat->secondaryDepthWrite = 0;
    mat->secondaryDepthFuncEqual = 0;
    mat->secondaryStageScore = -9999;
    mat->depthWrite = 0;
    mat->depthFuncEqual = 0;
    mat->alphaRef = 0.5f;
    mat->alphaFuncMode = ALPHA_FUNC_NONE;
    mat->alphaConst = 1.0f;
    mat->numDeformWaves = 0;
    mat->numDeformMoves = 0;
    mat->deformAutoSprite = 0;
    mat->deformAutoSprite2 = 0;
    mat->deformTextMode = 0;
    mat->selectedStageScore = -9999;
    if (!mat->shaderName) return NULL;
    bsp->numMaterials++;
    return mat;
}

static int StageScore(GLenum src, GLenum dst, int hasBlend) {
    if (!hasBlend) return 400;
    if (src == GL_DST_COLOR && dst == GL_ZERO) return 300; /* filter */
    if (src == GL_SRC_ALPHA && dst == GL_ONE_MINUS_SRC_ALPHA) return 200; /* glass */
    if (src == GL_ONE && dst == GL_ONE) return 100; /* additive detail/glow */
    return 150;
}

static float EvalWaveForm(int func, float x) {
    float t = x - floorf(x);
    switch (func) {
        case WAVE_TRIANGLE:
            return t < 0.5f ? (t * 4.0f - 1.0f) : (3.0f - t * 4.0f);
        case WAVE_SAWTOOTH:
            return t * 2.0f - 1.0f;
        case WAVE_INVERSE_SAWTOOTH:
            return 1.0f - t * 2.0f;
        case WAVE_SQUARE:
            return t < 0.5f ? 1.0f : -1.0f;
        case WAVE_SIN:
        default:
            return sinf(x * 6.2831853f);
    }
}

static GLenum ParseBlendFactor(const char *tok, GLenum fallback) {
    if (!tok) return fallback;
    if (StrIEqual(tok, "GL_ZERO")) return GL_ZERO;
    if (StrIEqual(tok, "GL_ONE")) return GL_ONE;
    if (StrIEqual(tok, "GL_DST_COLOR")) return GL_DST_COLOR;
    if (StrIEqual(tok, "GL_ONE_MINUS_DST_COLOR")) return GL_ONE_MINUS_DST_COLOR;
    if (StrIEqual(tok, "GL_SRC_ALPHA")) return GL_SRC_ALPHA;
    if (StrIEqual(tok, "GL_ONE_MINUS_SRC_ALPHA")) return GL_ONE_MINUS_SRC_ALPHA;
    if (StrIEqual(tok, "GL_SRC_COLOR")) return GL_SRC_COLOR;
    if (StrIEqual(tok, "GL_ONE_MINUS_SRC_COLOR")) return GL_ONE_MINUS_SRC_COLOR;
    if (StrIEqual(tok, "GL_DST_ALPHA")) return GL_DST_ALPHA;
    if (StrIEqual(tok, "GL_ONE_MINUS_DST_ALPHA")) return GL_ONE_MINUS_DST_ALPHA;
    return fallback;
}

static void ResetMaterialSelectedStage(material_info_t *mat) {
    free(mat->textureName);
    mat->textureName = NULL;
    if (mat->textureHandle && mat->textureHandle != 0) {
        glDeleteTextures(1, &mat->textureHandle);
        mat->textureHandle = 0;
    }
    if (mat->animFrames) {
        for (int i = 0; i < mat->numAnimFrames; i++) free(mat->animFrames[i]);
        free(mat->animFrames);
    }
    free(mat->animFrameTextures);
    mat->animFrames = NULL;
    mat->animFrameTextures = NULL;
    mat->numAnimFrames = 0;
    mat->animFps = 0.0f;
    mat->tcScale[0] = mat->tcScale[1] = 1.0f;
    mat->tcScroll[0] = mat->tcScroll[1] = 0.0f;
    mat->tcRotateDegPerSec = 0.0f;
    mat->tcTurbBase = mat->tcTurbAmp = mat->tcTurbPhase = 0.0f;
    mat->tcTurbFreq = 1.0f;
    mat->tcStretchBase = 1.0f; mat->tcStretchAmp = mat->tcStretchPhase = 0.0f; mat->tcStretchFreq = 1.0f;
    mat->tcTransform[0][0] = 1.0f; mat->tcTransform[0][1] = 0.0f;
    mat->tcTransform[1][0] = 0.0f; mat->tcTransform[1][1] = 1.0f;
    mat->tcTranslate[0] = mat->tcTranslate[1] = 0.0f;
    mat->hasTcScale = mat->hasTcScroll = mat->hasTcRotate = mat->hasTcTurb = 0;
    mat->hasTcStretch = mat->hasTcTransform = mat->hasTcSwap = 0;
    mat->hasTcGenVector = 0;
    mat->tcGenEnvironment = 0;
    mat->rgbGenIdentity = mat->rgbGenVertex = 0;
    mat->rgbGenConst = mat->rgbGenWave = 0;
    mat->rgbConst[0] = mat->rgbConst[1] = mat->rgbConst[2] = 1.0f;
    mat->rgbWaveBase = 1.0f; mat->rgbWaveAmp = mat->rgbWavePhase = 0.0f; mat->rgbWaveFreq = 1.0f;
    mat->alphaGenIdentity = mat->alphaGenVertex = 0;
    mat->alphaGenConst = mat->alphaGenWave = 0;
    mat->alphaConst = 1.0f;
    mat->alphaWaveBase = 1.0f; mat->alphaWaveAmp = mat->alphaWavePhase = 0.0f; mat->alphaWaveFreq = 1.0f;
    mat->blendSrc = GL_ONE;
    mat->blendDst = GL_ZERO;
    mat->hasBlend = 0;
    mat->depthWrite = 0;
    mat->depthFuncEqual = 0;
    free(mat->secondaryTextureName);
    mat->secondaryTextureName = NULL;
    if (mat->secondaryTextureHandle) glDeleteTextures(1, &mat->secondaryTextureHandle);
    mat->secondaryTextureHandle = 0;
    if (mat->secondaryAnimFrames) {
        for (int i = 0; i < mat->secondaryNumAnimFrames; i++) free(mat->secondaryAnimFrames[i]);
    }
    if (mat->secondaryAnimFrameTextures) {
        for (int i = 0; i < mat->secondaryNumAnimFrames; i++) {
            if (mat->secondaryAnimFrameTextures[i] != 0) glDeleteTextures(1, &mat->secondaryAnimFrameTextures[i]);
        }
    }
    free(mat->secondaryAnimFrames);
    free(mat->secondaryAnimFrameTextures);
    mat->secondaryAnimFrames = NULL;
    mat->secondaryAnimFrameTextures = NULL;
    mat->secondaryNumAnimFrames = 0;
    mat->secondaryAnimFps = 0.0f;
    mat->secondaryBlendSrc = GL_ONE;
    mat->secondaryBlendDst = GL_ZERO;
    mat->secondaryHasAlphaFunc = 0;
    mat->secondaryAlphaRef = 0.5f;
    mat->secondaryAlphaFuncMode = ALPHA_FUNC_NONE;
    mat->secondaryDepthWrite = 0;
    mat->secondaryDepthFuncEqual = 0;
    mat->hasSecondary = 0;
    mat->secondaryStageScore = -9999;
    for (int i = 0; i < MAX_EXTRA_STAGES; i++) {
        free(mat->extraStageTextureName[i]);
        mat->extraStageTextureName[i] = NULL;
        if (mat->extraStageTextureHandle[i]) glDeleteTextures(1, &mat->extraStageTextureHandle[i]);
        mat->extraStageTextureHandle[i] = 0;
        if (mat->extraStageAnimFrames[i]) {
            for (int k = 0; k < mat->extraStageNumAnimFrames[i]; k++) free(mat->extraStageAnimFrames[i][k]);
        }
        if (mat->extraStageAnimFrameTextures[i]) {
            for (int k = 0; k < mat->extraStageNumAnimFrames[i]; k++) {
                if (mat->extraStageAnimFrameTextures[i][k] != 0) glDeleteTextures(1, &mat->extraStageAnimFrameTextures[i][k]);
            }
        }
        free(mat->extraStageAnimFrames[i]);
        free(mat->extraStageAnimFrameTextures[i]);
        mat->extraStageAnimFrames[i] = NULL;
        mat->extraStageAnimFrameTextures[i] = NULL;
        mat->extraStageNumAnimFrames[i] = 0;
        mat->extraStageAnimFps[i] = 0.0f;
        mat->extraStageBlendSrc[i] = GL_ONE;
        mat->extraStageBlendDst[i] = GL_ZERO;
        mat->extraHasAlphaFunc[i] = 0;
        mat->extraAlphaRef[i] = 0.5f;
        mat->extraAlphaFuncMode[i] = ALPHA_FUNC_NONE;
        mat->extraHasBlend[i] = 0;
        mat->extraDepthWrite[i] = 0;
        mat->extraDepthFuncEqual[i] = 0;
        mat->extraTcScale[i][0] = mat->extraTcScale[i][1] = 1.0f;
        mat->extraTcScroll[i][0] = mat->extraTcScroll[i][1] = 0.0f;
        mat->extraTcRotate[i] = 0.0f;
        mat->extraHasTcScale[i] = mat->extraHasTcScroll[i] = mat->extraHasTcRotate[i] = 0;
        mat->extraHasTcSwap[i] = 0;
        mat->extraHasTcTurb[i] = mat->extraHasTcStretch[i] = mat->extraHasTcTransform[i] = 0;
        mat->extraTcTurbBase[i] = mat->extraTcTurbAmp[i] = mat->extraTcTurbPhase[i] = 0.0f; mat->extraTcTurbFreq[i] = 1.0f;
        mat->extraTcStretchBase[i] = 1.0f; mat->extraTcStretchAmp[i] = mat->extraTcStretchPhase[i] = 0.0f; mat->extraTcStretchFreq[i] = 1.0f;
        mat->extraTcTransform[i][0][0] = 1.0f; mat->extraTcTransform[i][0][1] = 0.0f;
        mat->extraTcTransform[i][1][0] = 0.0f; mat->extraTcTransform[i][1][1] = 1.0f;
        mat->extraTcTranslate[i][0] = mat->extraTcTranslate[i][1] = 0.0f;
        mat->extraHasTcGenVector[i] = 0;
        mat->extraTcGenEnvironment[i] = 0;
        mat->extraTcGenVecS[i][0] = 1.0f; mat->extraTcGenVecS[i][1] = 0.0f; mat->extraTcGenVecS[i][2] = 0.0f;
        mat->extraTcGenVecT[i][0] = 0.0f; mat->extraTcGenVecT[i][1] = 1.0f; mat->extraTcGenVecT[i][2] = 0.0f;
        mat->extraRgbGenVertex[i] = 0;
        mat->extraRgbGenConst[i] = 0;
        mat->extraRgbGenWave[i] = 0;
        mat->extraRgbConst[i][0] = mat->extraRgbConst[i][1] = mat->extraRgbConst[i][2] = 1.0f;
        mat->extraAlphaGenVertex[i] = 0;
        mat->extraAlphaGenConst[i] = 0;
        mat->extraAlphaGenWave[i] = 0;
        mat->extraAlphaConst[i] = 1.0f;
        mat->extraRgbWaveBase[i] = 1.0f; mat->extraRgbWaveAmp[i] = mat->extraRgbWavePhase[i] = 0.0f; mat->extraRgbWaveFreq[i] = 1.0f;
        mat->extraAlphaWaveBase[i] = 1.0f; mat->extraAlphaWaveAmp[i] = mat->extraAlphaWavePhase[i] = 0.0f; mat->extraAlphaWaveFreq[i] = 1.0f;
    }
    mat->numExtraStages = 0;
    mat->hasAlphaFunc = 0;
    mat->alphaRef = 0.5f;
    mat->alphaFuncMode = ALPHA_FUNC_NONE;
}

static GLuint GetMaterialTextureNow(bsp_data_t *bsp, material_info_t *mat, GLuint whiteTex, float timeSec) {
    if (mat && mat->textureName) {
        if (StrIEqual(mat->textureName, "$whiteimage")) return whiteTex;
        if (StrIEqual(mat->textureName, "$lightmap") || StrIEqual(mat->textureName, "$dlight")) return 0;
    }
    if (mat->numAnimFrames > 0 && mat->animFps > 0.0f) {
        int idx = (int)(timeSec * mat->animFps) % mat->numAnimFrames;
        if (idx < 0) idx = 0;
        if (!mat->animFrameTextures[idx]) {
            mat->animFrameTextures[idx] = LoadShaderImagePath(bsp->gameRoot, mat->animFrames[idx]);
            if (!mat->animFrameTextures[idx]) mat->animFrameTextures[idx] = whiteTex;
        }
        return mat->animFrameTextures[idx];
    }
    if (mat->textureName) {
        if (!mat->textureHandle) {
            mat->textureHandle = LoadShaderImagePathEx(bsp->gameRoot, mat->textureName, mat->hasAlphaFunc ? 1 : 0);
            if (!mat->textureHandle) mat->textureHandle = whiteTex;
        }
        return mat->textureHandle;
    }
    if (mat->editorTextureName) {
        if (!mat->textureHandle) {
            mat->textureHandle = LoadShaderImagePath(bsp->gameRoot, mat->editorTextureName);
            if (!mat->textureHandle) mat->textureHandle = whiteTex;
        }
        return mat->textureHandle;
    }
    return whiteTex;
}

static GLuint GetMaterialSecondaryTextureNow(bsp_data_t *bsp, material_info_t *mat, GLuint whiteTex, float timeSec) {
    if (!mat || !mat->hasSecondary) return whiteTex;
    if (mat->secondaryNumAnimFrames > 0 && mat->secondaryAnimFps > 0.0f) {
        int idx = (int)(timeSec * mat->secondaryAnimFps) % mat->secondaryNumAnimFrames;
        if (idx < 0) idx = 0;
        if (!mat->secondaryAnimFrameTextures[idx]) {
            mat->secondaryAnimFrameTextures[idx] = LoadShaderImagePathEx(
                bsp->gameRoot, mat->secondaryAnimFrames[idx], mat->secondaryHasAlphaFunc ? 1 : 0);
            if (!mat->secondaryAnimFrameTextures[idx]) mat->secondaryAnimFrameTextures[idx] = whiteTex;
        }
        return mat->secondaryAnimFrameTextures[idx];
    }
    if (!mat->secondaryTextureName) return whiteTex;
    if (StrIEqual(mat->secondaryTextureName, "$whiteimage")) return whiteTex;
    if (StrIEqual(mat->secondaryTextureName, "$lightmap") || StrIEqual(mat->secondaryTextureName, "$dlight")) return 0;
    if (!mat->secondaryTextureHandle) {
        mat->secondaryTextureHandle = LoadShaderImagePathEx(
            bsp->gameRoot, mat->secondaryTextureName, mat->secondaryHasAlphaFunc ? 1 : 0);
        if (!mat->secondaryTextureHandle) mat->secondaryTextureHandle = whiteTex;
    }
    return mat->secondaryTextureHandle;
}

static GLuint GetMaterialExtraStageTextureNow(bsp_data_t *bsp, material_info_t *mat, int idx, GLuint whiteTex, float timeSec) {
    if (!mat || idx < 0 || idx >= mat->numExtraStages) return whiteTex;
    if (mat->extraStageNumAnimFrames[idx] > 0 && mat->extraStageAnimFps[idx] > 0.0f) {
        int fi = (int)(timeSec * mat->extraStageAnimFps[idx]) % mat->extraStageNumAnimFrames[idx];
        if (fi < 0) fi = 0;
        if (!mat->extraStageAnimFrameTextures[idx][fi]) {
            mat->extraStageAnimFrameTextures[idx][fi] = LoadShaderImagePathEx(
                bsp->gameRoot, mat->extraStageAnimFrames[idx][fi], mat->extraHasAlphaFunc[idx] ? 1 : 0);
            if (!mat->extraStageAnimFrameTextures[idx][fi]) mat->extraStageAnimFrameTextures[idx][fi] = whiteTex;
        }
        return mat->extraStageAnimFrameTextures[idx][fi];
    }
    if (!mat->extraStageTextureName[idx]) return whiteTex;
    if (StrIEqual(mat->extraStageTextureName[idx], "$whiteimage")) return whiteTex;
    if (StrIEqual(mat->extraStageTextureName[idx], "$lightmap") || StrIEqual(mat->extraStageTextureName[idx], "$dlight")) return 0;
    if (!mat->extraStageTextureHandle[idx]) {
        mat->extraStageTextureHandle[idx] = LoadShaderImagePathEx(
            bsp->gameRoot, mat->extraStageTextureName[idx], mat->extraHasAlphaFunc[idx] ? 1 : 0);
        if (!mat->extraStageTextureHandle[idx]) mat->extraStageTextureHandle[idx] = whiteTex;
    }
    return mat->extraStageTextureHandle[idx];
}

static GLuint GetFullStageTextureNow(bsp_data_t *bsp, shader_stage_t *st, GLuint whiteTex, float timeSec, int alphaSensitive) {
    if (!st) return whiteTex;
    if (st->numAnimFrames > 0 && st->animFps > 0.0f) {
        int fi = (int)(timeSec * st->animFps) % st->numAnimFrames;
        if (fi < 0) fi = 0;
        if (!st->animFrameTextures[fi]) {
            st->animFrameTextures[fi] = LoadShaderImagePathEx(bsp->gameRoot, st->animFrames[fi], alphaSensitive ? 1 : 0);
            if (!st->animFrameTextures[fi]) st->animFrameTextures[fi] = whiteTex;
        }
        return st->animFrameTextures[fi];
    }
    if (!st->textureName) return whiteTex;
    if (StrIEqual(st->textureName, "$whiteimage")) return whiteTex;
    if (StrIEqual(st->textureName, "$lightmap") || StrIEqual(st->textureName, "$dlight")) return 0;
    if (!st->textureHandle) {
        st->textureHandle = LoadShaderImagePathEx(bsp->gameRoot, st->textureName, alphaSensitive ? 1 : 0);
        if (!st->textureHandle) st->textureHandle = whiteTex;
    }
    return st->textureHandle;
}

static GLuint GetSurfaceTexture(bsp_data_t *bsp, int shaderNum, GLuint whiteTex, float timeSec) {
    static const char *exts[] = { ".tga", ".jpg", ".png" };
    char full[1400];
    if (!bsp->shaderTextures || shaderNum < 0 || shaderNum >= bsp->numShaders) {
        return whiteTex;
    }
    if (bsp->shaderTextures[shaderNum] != 0) {
        return bsp->shaderTextures[shaderNum];
    }
    if (!bsp->shaders[shaderNum].shader[0]) {
        bsp->shaderTextures[shaderNum] = whiteTex;
        return whiteTex;
    }

    {
        material_info_t *mat = NULL;
        if (bsp->shaderMaterialIndex && shaderNum >= 0 && shaderNum < bsp->numShaders) {
            int mi = bsp->shaderMaterialIndex[shaderNum];
            if (mi >= 0 && mi < bsp->numMaterials) mat = &bsp->materials[mi];
        }
        if (mat) {
            GLuint mtex = GetMaterialTextureNow(bsp, mat, whiteTex, timeSec);
            if (mtex != 0 && mtex != whiteTex) {
                if (mat->numAnimFrames <= 0) bsp->shaderTextures[shaderNum] = mtex;
                return mtex;
            }
        }
    }
    for (int i = 0; i < 3; i++) {
        snprintf(full, sizeof(full), "%s/%s%s", bsp->gameRoot, bsp->shaders[shaderNum].shader, exts[i]);
        bsp->shaderTextures[shaderNum] = LoadTexture2D(full);
        if (bsp->shaderTextures[shaderNum] != 0) {
            return bsp->shaderTextures[shaderNum];
        }
    }
    bsp->shaderTextures[shaderNum] = whiteTex;
    return whiteTex;
}

static void PreloadMaterialTextures(bsp_data_t *bsp, GLuint whiteTex) {
    for (int i = 0; i < bsp->numMaterials; i++) {
        material_info_t *mat = &bsp->materials[i];
        for (int k = 0; k < mat->numAnimFrames; k++) {
            if (!mat->animFrameTextures[k]) {
                mat->animFrameTextures[k] = LoadShaderImagePath(bsp->gameRoot, mat->animFrames[k]);
                if (!mat->animFrameTextures[k]) mat->animFrameTextures[k] = whiteTex;
            }
        }
        for (int k = 0; k < mat->secondaryNumAnimFrames; k++) {
            if (!mat->secondaryAnimFrameTextures[k]) {
                mat->secondaryAnimFrameTextures[k] = LoadShaderImagePathEx(
                    bsp->gameRoot, mat->secondaryAnimFrames[k], mat->secondaryHasAlphaFunc ? 1 : 0);
                if (!mat->secondaryAnimFrameTextures[k]) mat->secondaryAnimFrameTextures[k] = whiteTex;
            }
        }
        (void)GetMaterialSecondaryTextureNow(bsp, mat, whiteTex, 0.0f);
        for (int e = 0; e < mat->numExtraStages; e++) {
            for (int k = 0; k < mat->extraStageNumAnimFrames[e]; k++) {
                if (!mat->extraStageAnimFrameTextures[e][k]) {
                    mat->extraStageAnimFrameTextures[e][k] = LoadShaderImagePathEx(
                        bsp->gameRoot, mat->extraStageAnimFrames[e][k], mat->extraHasAlphaFunc[e] ? 1 : 0);
                    if (!mat->extraStageAnimFrameTextures[e][k]) mat->extraStageAnimFrameTextures[e][k] = whiteTex;
                }
            }
            (void)GetMaterialExtraStageTextureNow(bsp, mat, e, whiteTex, 0.0f);
        }
        for (int si = 0; si < mat->numFullStages; si++) {
            shader_stage_t *st = &mat->fullStages[si];
            for (int k = 0; k < st->numAnimFrames; k++) {
                if (!st->animFrameTextures[k]) {
                    st->animFrameTextures[k] = LoadShaderImagePathEx(
                        bsp->gameRoot, st->animFrames[k], st->hasAlphaFunc ? 1 : 0);
                    if (!st->animFrameTextures[k]) st->animFrameTextures[k] = whiteTex;
                }
            }
            (void)GetFullStageTextureNow(bsp, st, whiteTex, 0.0f, st->hasAlphaFunc);
        }
    }
    for (int s = 0; s < bsp->numShaders; s++) {
        (void)GetSurfaceTexture(bsp, s, whiteTex, 0.0f);
    }
}

static void BuildShaderMaterialCache(bsp_data_t *bsp) {
    bsp->shaderMaterialIndex = (int *)malloc((size_t)bsp->numShaders * sizeof(int));
    if (!bsp->shaderMaterialIndex) return;
    for (int i = 0; i < bsp->numShaders; i++) {
        material_info_t *m = FindMaterialInfo(bsp, bsp->shaders[i].shader);
        bsp->shaderMaterialIndex[i] = -1;
        if (m) bsp->shaderMaterialIndex[i] = (int)(m - bsp->materials);
    }
}

static int IsAlphaBlendMaterial(const material_info_t *mat) {
    if (!mat || !mat->hasBlend) return 0;
    if (mat->hasAlphaFunc) return 0;
    if (mat->blendSrc == GL_DST_COLOR && mat->blendDst == GL_ZERO) return 0; /* filter/modulate */
    if (mat->blendSrc == GL_ONE && mat->blendDst == GL_ZERO) return 0;      /* opaque */
    return 1;
}

static material_info_t *GetSurfaceMaterial(bsp_data_t *bsp, const dsurface_t *s) {
    if (!bsp->shaderMaterialIndex || s->shaderNum < 0 || s->shaderNum >= bsp->numShaders) return NULL;
    {
        int mi = bsp->shaderMaterialIndex[s->shaderNum];
        if (mi >= 0 && mi < bsp->numMaterials) return &bsp->materials[mi];
    }
    return NULL;
}

static int IsSurfaceTransparent(bsp_data_t *bsp, const dsurface_t *s) {
    const material_info_t *mat = GetSurfaceMaterial(bsp, s);
    const dshader_t *sh;
    int flaggedTransparent;
    if (s->shaderNum < 0 || s->shaderNum >= bsp->numShaders) return 0;
    sh = &bsp->shaders[s->shaderNum];
    flaggedTransparent = ((sh->surfaceFlags & SURF_GLASS) || (sh->contentFlags & CONTENTS_TRANSLUCENT)) ? 1 : 0;
    if (mat && mat->surfaceParmWater) return 1;
    if (mat && (mat->sortTransparent || mat->surfaceParmTrans)) {
        if (mat->hasBlend && !mat->hasAlphaFunc) return 1;
    }
    if (IsAlphaBlendMaterial(mat)) return 1;
    if (!flaggedTransparent || !mat) return 0;
    if (mat->hasBlend || mat->hasAlphaFunc) return 1;
    if (mat->hasSecondary || mat->numExtraStages > 0) return 1;
    return 0;
}

static int IsSurfaceDecal(bsp_data_t *bsp, const dsurface_t *s) {
    const material_info_t *mat = GetSurfaceMaterial(bsp, s);
    return mat && (mat->polygonOffset || mat->sortDecal);
}

static const float *g_sortDistRef = NULL;
static int CompareSurfaceBackToFront(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    float da = g_sortDistRef ? g_sortDistRef[ia] : 0.0f;
    float db = g_sortDistRef ? g_sortDistRef[ib] : 0.0f;
    if (da < db) return 1;
    if (da > db) return -1;
    return 0;
}

static int BuildSurfaceCenters(bsp_data_t *bsp) {
    bsp->surfaceCenters = (float (*)[3])calloc((size_t)bsp->numSurfaces, sizeof(float[3]));
    if (!bsp->surfaceCenters) return 0;
    for (int i = 0; i < bsp->numSurfaces; i++) {
        const dsurface_t *s = &bsp->surfaces[i];
        double cx = 0.0, cy = 0.0, cz = 0.0;
        int count = 0;
        if (s->firstVert < 0 || s->numVerts <= 0 || s->firstVert + s->numVerts > bsp->numVerts) continue;
        for (int v = 0; v < s->numVerts; v++) {
            const drawVert_t *dv = &bsp->verts[s->firstVert + v];
            cx += dv->xyz[0];
            cy += dv->xyz[1];
            cz += dv->xyz[2];
            count++;
        }
        if (count > 0) {
            bsp->surfaceCenters[i][0] = (float)(cx / count);
            bsp->surfaceCenters[i][1] = (float)(cy / count);
            bsp->surfaceCenters[i][2] = (float)(cz / count);
        }
    }
    return 1;
}

static int InitTransparentSortBuffers(bsp_data_t *bsp) {
    bsp->transparentSurfaceOrder = (int *)malloc((size_t)bsp->numSurfaces * sizeof(int));
    bsp->transparentSurfaceDist2 = (float *)malloc((size_t)bsp->numSurfaces * sizeof(float));
    return bsp->transparentSurfaceOrder && bsp->transparentSurfaceDist2;
}

static GLuint GetSurfaceLightmapTexture(bsp_data_t *bsp, int lightmapNum, GLuint whiteTex) {
    if (!bsp->lightmapTextures || lightmapNum < 0 || lightmapNum >= bsp->numLightmaps) {
        return whiteTex;
    }
    if (bsp->lightmapTextures[lightmapNum] == 0) {
        return whiteTex;
    }
    return bsp->lightmapTextures[lightmapNum];
}

static void UploadLightmaps(bsp_data_t *bsp) {
    const int lightmapBytes = 128 * 128 * 3;
    const float lightmapBoost = 4.0f;
    if (!bsp->lightmapData || bsp->numLightmaps <= 0) return;
    bsp->lightmapTextures = (GLuint *)calloc((size_t)bsp->numLightmaps, sizeof(GLuint));
    if (!bsp->lightmapTextures) return;
    for (int i = 0; i < bsp->numLightmaps; i++) {
        const uint8_t *src = bsp->lightmapData + (size_t)i * (size_t)lightmapBytes;
        uint8_t boosted[128 * 128 * 3];
        for (int p = 0; p < lightmapBytes; p++) {
            int v = (int)(src[p] * lightmapBoost);
            if (v > 255) v = 255;
            boosted[p] = (uint8_t)v;
        }
        glGenTextures(1, &bsp->lightmapTextures[i]);
        glBindTexture(GL_TEXTURE_2D, bsp->lightmapTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 128, 128, 0, GL_RGB, GL_UNSIGNED_BYTE, boosted);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

static int ReadFile(const char *path, uint8_t **outData, size_t *outLen) {
    FILE *f = fopen(path, "rb");
    long sz;
    uint8_t *buf;

    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    sz = ftell(f);
    if (sz <= 0) {
        fclose(f);
        return 0;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }

    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return 0;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return 0;
    }
    fclose(f);
    *outData = buf;
    *outLen = (size_t)sz;
    return 1;
}

static void ParseShaderScript(bsp_data_t *bsp, const char *path) {
    uint8_t *buf = NULL;
    size_t len = 0;
    char *data, *line, *save = NULL;
    char currentShader[256] = {0};
    char pendingShader[256] = {0};
    int inShader = 0, inStage = 0;
    int stageHasMap = 0, stageHasAnim = 0;
    int stageMapIsLightmap = 0;
    char stageMap[256] = {0};
    char **stageAnimFrames = NULL;
    int stageAnimCount = 0;
    float stageAnimFps = 0.0f;
    float stageTcScale[2] = {1.0f, 1.0f};
    float stageTcScroll[2] = {0.0f, 0.0f};
    float stageTcRotate = 0.0f;
    float stageTurbBase = 0.0f, stageTurbAmp = 0.0f, stageTurbPhase = 0.0f, stageTurbFreq = 1.0f;
    float stageStretchBase = 1.0f, stageStretchAmp = 0.0f, stageStretchPhase = 0.0f, stageStretchFreq = 1.0f;
    float stageTransform[2][2] = {{1.0f, 0.0f}, {0.0f, 1.0f}};
    float stageTranslate[2] = {0.0f, 0.0f};
    float stageTcGenVecS[3] = {1.0f, 0.0f, 0.0f};
    float stageTcGenVecT[3] = {0.0f, 1.0f, 0.0f};
    int stageHasTcScale = 0, stageHasTcScroll = 0, stageHasTcRotate = 0, stageHasTcTurb = 0;
    int stageHasTcStretch = 0, stageHasTcTransform = 0, stageHasTcSwap = 0;
    int stageHasTcGenVector = 0;
    int stageTcLightmap = 0;
    int stageRgbIdentity = 0, stageRgbVertex = 0, stageRgbConst = 0, stageRgbWave = 0;
    float stageRgbConstV[3] = {1.0f, 1.0f, 1.0f};
    float stageRgbWaveBase = 1.0f, stageRgbWaveAmp = 0.0f, stageRgbWavePhase = 0.0f, stageRgbWaveFreq = 1.0f;
    int stageAlphaIdentity = 0, stageAlphaVertex = 0, stageAlphaConst = 0, stageAlphaWave = 0;
    float stageAlphaConstV = 1.0f;
    float stageAlphaWaveBase = 1.0f, stageAlphaWaveAmp = 0.0f, stageAlphaWavePhase = 0.0f, stageAlphaWaveFreq = 1.0f;
    int stageTcEnv = 0;
    GLenum stageBlendSrc = GL_ONE, stageBlendDst = GL_ZERO;
    int stageHasBlend = 0;
    int stageHasAlphaFunc = 0;
    float stageAlphaRef = 0.5f;
    int stageAlphaFuncMode = ALPHA_FUNC_NONE;
    int stageDepthWrite = 0;
    int stageDepthFuncEqual = 0;
    if (!ReadFile(path, &buf, &len) || !buf) return;
    data = (char *)malloc(len + 1);
    if (!data) { free(buf); return; }
    memcpy(data, buf, len);
    data[len] = '\0';
    line = strtok_r(data, "\n\r", &save);
    while (line) {
        char *comment = strstr(line, "//");
        char *tok;
        if (comment) *comment = '\0';
        while (*line == ' ' || *line == '\t') line++;
        if (!*line) { line = strtok_r(NULL, "\n\r", &save); continue; }
        if (!inShader) {
            if (strchr(line, '{') && pendingShader[0]) {
                strncpy(currentShader, pendingShader, sizeof(currentShader) - 1);
                currentShader[sizeof(currentShader) - 1] = '\0';
                inShader = 1;
                {
                    material_info_t *mat = FindOrAddMaterialInfo(bsp, currentShader);
                    if (mat && strstr(line, "polygonOffset")) mat->polygonOffset = 1;
                }
            } else if (!strchr(line, '{') && !strchr(line, '}')) {
                strncpy(pendingShader, line, sizeof(pendingShader) - 1);
                pendingShader[sizeof(pendingShader) - 1] = '\0';
            }
            line = strtok_r(NULL, "\n\r", &save);
            continue;
        }
        if (strcmp(line, "{") == 0) {
            inStage = 1;
            stageHasMap = 0; stageHasAnim = 0;
            stageMapIsLightmap = 0;
            stageMap[0] = '\0';
            if (stageAnimFrames) {
                for (int i = 0; i < stageAnimCount; i++) free(stageAnimFrames[i]);
                free(stageAnimFrames);
            }
            stageAnimFrames = NULL; stageAnimCount = 0; stageAnimFps = 0.0f;
            stageHasTcScale = stageHasTcScroll = stageHasTcRotate = stageHasTcTurb = 0;
            stageHasTcStretch = stageHasTcTransform = stageHasTcSwap = 0;
            stageTcScale[0] = stageTcScale[1] = 1.0f;
            stageTcScroll[0] = stageTcScroll[1] = 0.0f;
            stageTcRotate = 0.0f;
            stageTurbBase = stageTurbAmp = stageTurbPhase = 0.0f; stageTurbFreq = 1.0f;
            stageStretchBase = 1.0f; stageStretchAmp = stageStretchPhase = 0.0f; stageStretchFreq = 1.0f;
            stageTransform[0][0] = 1.0f; stageTransform[0][1] = 0.0f; stageTransform[1][0] = 0.0f; stageTransform[1][1] = 1.0f;
            stageTranslate[0] = stageTranslate[1] = 0.0f;
            stageTcGenVecS[0] = 1.0f; stageTcGenVecS[1] = 0.0f; stageTcGenVecS[2] = 0.0f;
            stageTcGenVecT[0] = 0.0f; stageTcGenVecT[1] = 1.0f; stageTcGenVecT[2] = 0.0f;
            stageTcLightmap = 0;
            stageRgbIdentity = stageRgbVertex = stageRgbConst = stageRgbWave = 0;
            stageRgbConstV[0] = stageRgbConstV[1] = stageRgbConstV[2] = 1.0f;
            stageRgbWaveBase = 1.0f; stageRgbWaveAmp = stageRgbWavePhase = 0.0f; stageRgbWaveFreq = 1.0f;
            stageAlphaIdentity = stageAlphaVertex = stageAlphaConst = stageAlphaWave = 0;
            stageAlphaConstV = 1.0f;
            stageAlphaWaveBase = 1.0f; stageAlphaWaveAmp = stageAlphaWavePhase = 0.0f; stageAlphaWaveFreq = 1.0f;
            stageTcEnv = 0;
            stageBlendSrc = GL_ONE; stageBlendDst = GL_ZERO; stageHasBlend = 0;
            stageHasAlphaFunc = 0; stageAlphaRef = 0.5f;
            stageAlphaFuncMode = ALPHA_FUNC_NONE;
            stageDepthWrite = 0; stageDepthFuncEqual = 0;
            line = strtok_r(NULL, "\n\r", &save);
            continue;
        }
        if (strcmp(line, "}") == 0 && inStage) {
            if (currentShader[0]) {
                material_info_t *mat = FindOrAddMaterialInfo(bsp, currentShader);
                if (mat && (stageHasMap || stageHasAnim)) {
                    if (mat->numFullStages < MAX_FULL_STAGES) {
                        shader_stage_t *fst = &mat->fullStages[mat->numFullStages++];
                        memset(fst, 0, sizeof(*fst));
                        fst->textureName = stageHasMap ? DupString(stageMap) : NULL;
                        if (stageHasAnim && stageAnimCount > 0) {
                            fst->animFrames = (char **)malloc((size_t)stageAnimCount * sizeof(char *));
                            fst->animFrameTextures = (GLuint *)calloc((size_t)stageAnimCount, sizeof(GLuint));
                            if (fst->animFrames && fst->animFrameTextures) {
                                fst->numAnimFrames = stageAnimCount;
                                fst->animFps = stageAnimFps > 0.0f ? stageAnimFps : 1.0f;
                                for (int i = 0; i < stageAnimCount; i++) fst->animFrames[i] = DupString(stageAnimFrames[i]);
                            } else {
                                free(fst->animFrames);
                                free(fst->animFrameTextures);
                                fst->animFrames = NULL;
                                fst->animFrameTextures = NULL;
                            }
                        }
                        fst->blendSrc = stageBlendSrc;
                        fst->blendDst = stageBlendDst;
                        fst->hasBlend = stageHasBlend;
                        fst->hasAlphaFunc = stageHasAlphaFunc;
                        fst->alphaRef = stageAlphaRef;
                        fst->alphaFuncMode = stageAlphaFuncMode;
                        fst->depthWrite = stageDepthWrite;
                        fst->depthFuncEqual = stageDepthFuncEqual;
                        fst->hasTcScale = stageHasTcScale;
                        fst->tcScale[0] = stageTcScale[0]; fst->tcScale[1] = stageTcScale[1];
                        fst->hasTcScroll = stageHasTcScroll;
                        fst->tcScroll[0] = stageTcScroll[0]; fst->tcScroll[1] = stageTcScroll[1];
                        fst->hasTcRotate = stageHasTcRotate;
                        fst->tcRotate = stageTcRotate;
                        fst->hasTcSwap = stageHasTcSwap;
                        fst->hasTcTurb = stageHasTcTurb;
                        fst->tcTurbBase = stageTurbBase; fst->tcTurbAmp = stageTurbAmp;
                        fst->tcTurbPhase = stageTurbPhase; fst->tcTurbFreq = stageTurbFreq;
                        fst->hasTcStretch = stageHasTcStretch;
                        fst->tcStretchBase = stageStretchBase; fst->tcStretchAmp = stageStretchAmp;
                        fst->tcStretchPhase = stageStretchPhase; fst->tcStretchFreq = stageStretchFreq;
                        fst->hasTcTransform = stageHasTcTransform;
                        fst->tcTransform[0][0] = stageTransform[0][0]; fst->tcTransform[0][1] = stageTransform[0][1];
                        fst->tcTransform[1][0] = stageTransform[1][0]; fst->tcTransform[1][1] = stageTransform[1][1];
                        fst->tcTranslate[0] = stageTranslate[0]; fst->tcTranslate[1] = stageTranslate[1];
                        fst->hasTcGenVector = stageHasTcGenVector;
                        fst->tcGenEnvironment = stageTcEnv;
                        fst->tcGenLightmap = stageTcLightmap || (stageHasMap && StrIEqual(stageMap, "$lightmap"));
                        fst->tcGenVecS[0] = stageTcGenVecS[0]; fst->tcGenVecS[1] = stageTcGenVecS[1]; fst->tcGenVecS[2] = stageTcGenVecS[2];
                        fst->tcGenVecT[0] = stageTcGenVecT[0]; fst->tcGenVecT[1] = stageTcGenVecT[1]; fst->tcGenVecT[2] = stageTcGenVecT[2];
                        fst->rgbGenVertex = stageRgbVertex;
                        fst->rgbGenConst = stageRgbConst;
                        fst->rgbGenWave = stageRgbWave;
                        fst->rgbConst[0] = stageRgbConstV[0]; fst->rgbConst[1] = stageRgbConstV[1]; fst->rgbConst[2] = stageRgbConstV[2];
                        fst->rgbWaveBase = stageRgbWaveBase; fst->rgbWaveAmp = stageRgbWaveAmp;
                        fst->rgbWavePhase = stageRgbWavePhase; fst->rgbWaveFreq = stageRgbWaveFreq;
                        fst->alphaGenVertex = stageAlphaVertex;
                        fst->alphaGenConst = stageAlphaConst;
                        fst->alphaGenWave = stageAlphaWave;
                        fst->alphaConst = stageAlphaConstV;
                        fst->alphaWaveBase = stageAlphaWaveBase; fst->alphaWaveAmp = stageAlphaWaveAmp;
                        fst->alphaWavePhase = stageAlphaWavePhase; fst->alphaWaveFreq = stageAlphaWaveFreq;
                    }
                    int score = StageScore(stageBlendSrc, stageBlendDst, stageHasBlend);
                    if (stageMapIsLightmap) score -= 500;
                    if (stageTcEnv && score >= 300) score -= 240;
                    if (stageHasBlend && stageBlendSrc == GL_ONE && stageBlendDst == GL_ONE) score -= 120;
                    if (stageHasAlphaFunc && score < 350) score = 350;
                    if (mat->polygonOffset) {
                        /* Prefer true decal stages over helper/detail stages. */
                        if (stageHasAlphaFunc || stageHasBlend) score += 250;
                        else score -= 200;
                    }
                    if (score > mat->selectedStageScore) {
                        ResetMaterialSelectedStage(mat);
                        mat->selectedStageScore = score;
                    if (stageHasAnim && stageAnimCount > 0) {
                        mat->animFrames = (char **)malloc((size_t)stageAnimCount * sizeof(char *));
                        mat->animFrameTextures = (GLuint *)calloc((size_t)stageAnimCount, sizeof(GLuint));
                        if (mat->animFrames && mat->animFrameTextures) {
                            mat->numAnimFrames = stageAnimCount;
                            mat->animFps = stageAnimFps > 0.0f ? stageAnimFps : 1.0f;
                            for (int i = 0; i < stageAnimCount; i++) mat->animFrames[i] = DupString(stageAnimFrames[i]);
                        }
                    } else if (stageHasMap) {
                        mat->textureName = DupString(stageMap);
                    }
                    if (stageHasTcScale) { mat->tcScale[0] = stageTcScale[0]; mat->tcScale[1] = stageTcScale[1]; mat->hasTcScale = 1; }
                    if (stageHasTcScroll) { mat->tcScroll[0] = stageTcScroll[0]; mat->tcScroll[1] = stageTcScroll[1]; mat->hasTcScroll = 1; }
                    if (stageHasTcRotate) { mat->tcRotateDegPerSec = stageTcRotate; mat->hasTcRotate = 1; }
                    if (stageHasTcTurb) {
                        mat->tcTurbBase = stageTurbBase; mat->tcTurbAmp = stageTurbAmp;
                        mat->tcTurbPhase = stageTurbPhase; mat->tcTurbFreq = stageTurbFreq; mat->hasTcTurb = 1;
                    }
                    if (stageHasTcStretch) {
                        mat->tcStretchBase = stageStretchBase; mat->tcStretchAmp = stageStretchAmp;
                        mat->tcStretchPhase = stageStretchPhase; mat->tcStretchFreq = stageStretchFreq; mat->hasTcStretch = 1;
                    }
                    if (stageHasTcTransform) {
                        mat->tcTransform[0][0] = stageTransform[0][0]; mat->tcTransform[0][1] = stageTransform[0][1];
                        mat->tcTransform[1][0] = stageTransform[1][0]; mat->tcTransform[1][1] = stageTransform[1][1];
                        mat->tcTranslate[0] = stageTranslate[0]; mat->tcTranslate[1] = stageTranslate[1];
                        mat->hasTcTransform = 1;
                    }
                    if (stageHasTcSwap) mat->hasTcSwap = 1;
                    if (stageHasTcGenVector) {
                        mat->tcGenVecS[0] = stageTcGenVecS[0];
                        mat->tcGenVecS[1] = stageTcGenVecS[1];
                        mat->tcGenVecS[2] = stageTcGenVecS[2];
                        mat->tcGenVecT[0] = stageTcGenVecT[0];
                        mat->tcGenVecT[1] = stageTcGenVecT[1];
                        mat->tcGenVecT[2] = stageTcGenVecT[2];
                        mat->hasTcGenVector = 1;
                    }
                    mat->rgbGenIdentity = stageRgbIdentity;
                    mat->rgbGenVertex = stageRgbVertex;
                    if (stageRgbConst) {
                        mat->rgbGenConst = 1;
                        mat->rgbConst[0] = stageRgbConstV[0];
                        mat->rgbConst[1] = stageRgbConstV[1];
                        mat->rgbConst[2] = stageRgbConstV[2];
                    }
                    if (stageRgbWave) {
                        mat->rgbGenWave = 1;
                        mat->rgbWaveBase = stageRgbWaveBase; mat->rgbWaveAmp = stageRgbWaveAmp;
                        mat->rgbWavePhase = stageRgbWavePhase; mat->rgbWaveFreq = stageRgbWaveFreq;
                    }
                    mat->alphaGenIdentity = stageAlphaIdentity;
                    mat->alphaGenVertex = stageAlphaVertex;
                    if (stageAlphaConst) { mat->alphaGenConst = 1; mat->alphaConst = stageAlphaConstV; }
                    if (stageAlphaWave) {
                        mat->alphaGenWave = 1;
                        mat->alphaWaveBase = stageAlphaWaveBase; mat->alphaWaveAmp = stageAlphaWaveAmp;
                        mat->alphaWavePhase = stageAlphaWavePhase; mat->alphaWaveFreq = stageAlphaWaveFreq;
                    }
                    mat->tcGenEnvironment = stageTcEnv;
                    if (stageHasBlend) {
                        mat->blendSrc = stageBlendSrc;
                        mat->blendDst = stageBlendDst;
                        mat->hasBlend = 1;
                    }
                    if (stageHasAlphaFunc) {
                        mat->hasAlphaFunc = 1;
                        mat->alphaRef = stageAlphaRef;
                        mat->alphaFuncMode = stageAlphaFuncMode;
                    }
                    mat->depthWrite = stageDepthWrite;
                    mat->depthFuncEqual = stageDepthFuncEqual;
                    } else if ((stageHasMap || stageHasAnim) && !stageMapIsLightmap && (stageHasBlend || stageHasAlphaFunc) && score > mat->secondaryStageScore) {
                        /* Keep previous secondary as an extra pass instead of dropping it.
                         * machine_cl01 uses same texture across different blend states. */
                        AppendSecondaryAsExtra(mat);
                        free(mat->secondaryTextureName);
                        mat->secondaryTextureName = stageHasMap ? DupString(stageMap) : NULL;
                        if (mat->secondaryAnimFrames) {
                            for (int i = 0; i < mat->secondaryNumAnimFrames; i++) free(mat->secondaryAnimFrames[i]);
                        }
                        if (mat->secondaryAnimFrameTextures) {
                            for (int i = 0; i < mat->secondaryNumAnimFrames; i++) {
                                if (mat->secondaryAnimFrameTextures[i] != 0) glDeleteTextures(1, &mat->secondaryAnimFrameTextures[i]);
                            }
                        }
                        free(mat->secondaryAnimFrames);
                        free(mat->secondaryAnimFrameTextures);
                        mat->secondaryAnimFrames = NULL;
                        mat->secondaryAnimFrameTextures = NULL;
                        mat->secondaryNumAnimFrames = 0;
                        mat->secondaryAnimFps = 0.0f;
                        if (stageHasAnim && stageAnimCount > 0) {
                            mat->secondaryAnimFrames = (char **)malloc((size_t)stageAnimCount * sizeof(char *));
                            mat->secondaryAnimFrameTextures = (GLuint *)calloc((size_t)stageAnimCount, sizeof(GLuint));
                            if (mat->secondaryAnimFrames && mat->secondaryAnimFrameTextures) {
                                mat->secondaryNumAnimFrames = stageAnimCount;
                                mat->secondaryAnimFps = stageAnimFps > 0.0f ? stageAnimFps : 1.0f;
                                for (int i = 0; i < stageAnimCount; i++) mat->secondaryAnimFrames[i] = DupString(stageAnimFrames[i]);
                            } else {
                                free(mat->secondaryAnimFrames);
                                free(mat->secondaryAnimFrameTextures);
                                mat->secondaryAnimFrames = NULL;
                                mat->secondaryAnimFrameTextures = NULL;
                                mat->secondaryNumAnimFrames = 0;
                                mat->secondaryAnimFps = 0.0f;
                            }
                        }
                        mat->secondaryBlendSrc = stageBlendSrc;
                        mat->secondaryBlendDst = stageBlendDst;
                        mat->secondaryHasAlphaFunc = stageHasAlphaFunc;
                        mat->secondaryAlphaRef = stageAlphaRef;
                        mat->secondaryAlphaFuncMode = stageAlphaFuncMode;
                        mat->secondaryDepthWrite = stageDepthWrite;
                        mat->secondaryDepthFuncEqual = stageDepthFuncEqual;
                        mat->hasSecondary = (mat->secondaryTextureName != NULL || mat->secondaryNumAnimFrames > 0);
                        mat->secondaryStageScore = score;
                    } else if ((stageHasMap || stageHasAnim) && !stageMapIsLightmap && (stageHasBlend || stageHasAlphaFunc) && mat->numExtraStages < MAX_EXTRA_STAGES) {
                        int exists = 0;
                        for (int ei = 0; ei < mat->numExtraStages; ei++) {
                            int textureMatches = 0;
                            if (stageHasMap && mat->extraStageTextureName[ei]) {
                                textureMatches = (strcmp(mat->extraStageTextureName[ei], stageMap) == 0);
                            } else if (!stageHasMap && !mat->extraStageTextureName[ei]) {
                                textureMatches = 1;
                            }
                            if (textureMatches &&
                                mat->extraStageNumAnimFrames[ei] == stageAnimCount &&
                                mat->extraStageBlendSrc[ei] == stageBlendSrc &&
                                mat->extraStageBlendDst[ei] == stageBlendDst &&
                                mat->extraHasAlphaFunc[ei] == stageHasAlphaFunc &&
                                (!stageHasAlphaFunc || fabsf(mat->extraAlphaRef[ei] - stageAlphaRef) < 1e-6f)) {
                                exists = 1;
                                break;
                            }
                        }
                        if (!exists) {
                            int ei = mat->numExtraStages;
                            mat->extraStageTextureName[ei] = stageHasMap ? DupString(stageMap) : NULL;
                            if (!stageHasMap || mat->extraStageTextureName[ei]) {
                                mat->extraStageBlendSrc[ei] = stageBlendSrc;
                                mat->extraStageBlendDst[ei] = stageBlendDst;
                                mat->extraHasAlphaFunc[ei] = stageHasAlphaFunc;
                                mat->extraAlphaRef[ei] = stageAlphaRef;
                                mat->extraAlphaFuncMode[ei] = stageAlphaFuncMode;
                                mat->extraHasBlend[ei] = stageHasBlend;
                                mat->extraHasTcScale[ei] = stageHasTcScale;
                                mat->extraTcScale[ei][0] = stageTcScale[0];
                                mat->extraTcScale[ei][1] = stageTcScale[1];
                                mat->extraHasTcScroll[ei] = stageHasTcScroll;
                                mat->extraTcScroll[ei][0] = stageTcScroll[0];
                                mat->extraTcScroll[ei][1] = stageTcScroll[1];
                                mat->extraHasTcRotate[ei] = stageHasTcRotate;
                                mat->extraTcRotate[ei] = stageTcRotate;
                                mat->extraHasTcSwap[ei] = stageHasTcSwap;
                                mat->extraHasTcTurb[ei] = stageHasTcTurb;
                                mat->extraTcTurbBase[ei] = stageTurbBase;
                                mat->extraTcTurbAmp[ei] = stageTurbAmp;
                                mat->extraTcTurbPhase[ei] = stageTurbPhase;
                                mat->extraTcTurbFreq[ei] = stageTurbFreq;
                                mat->extraHasTcStretch[ei] = stageHasTcStretch;
                                mat->extraTcStretchBase[ei] = stageStretchBase;
                                mat->extraTcStretchAmp[ei] = stageStretchAmp;
                                mat->extraTcStretchPhase[ei] = stageStretchPhase;
                                mat->extraTcStretchFreq[ei] = stageStretchFreq;
                                mat->extraHasTcTransform[ei] = stageHasTcTransform;
                                mat->extraTcTransform[ei][0][0] = stageTransform[0][0];
                                mat->extraTcTransform[ei][0][1] = stageTransform[0][1];
                                mat->extraTcTransform[ei][1][0] = stageTransform[1][0];
                                mat->extraTcTransform[ei][1][1] = stageTransform[1][1];
                                mat->extraTcTranslate[ei][0] = stageTranslate[0];
                                mat->extraTcTranslate[ei][1] = stageTranslate[1];
                                mat->extraHasTcGenVector[ei] = stageHasTcGenVector;
                                mat->extraTcGenEnvironment[ei] = stageTcEnv;
                                mat->extraTcGenVecS[ei][0] = stageTcGenVecS[0];
                                mat->extraTcGenVecS[ei][1] = stageTcGenVecS[1];
                                mat->extraTcGenVecS[ei][2] = stageTcGenVecS[2];
                                mat->extraTcGenVecT[ei][0] = stageTcGenVecT[0];
                                mat->extraTcGenVecT[ei][1] = stageTcGenVecT[1];
                                mat->extraTcGenVecT[ei][2] = stageTcGenVecT[2];
                                mat->extraRgbGenVertex[ei] = stageRgbVertex;
                                mat->extraRgbGenConst[ei] = stageRgbConst;
                                mat->extraRgbGenWave[ei] = stageRgbWave;
                                mat->extraRgbConst[ei][0] = stageRgbConstV[0];
                                mat->extraRgbConst[ei][1] = stageRgbConstV[1];
                                mat->extraRgbConst[ei][2] = stageRgbConstV[2];
                                mat->extraRgbWaveBase[ei] = stageRgbWaveBase;
                                mat->extraRgbWaveAmp[ei] = stageRgbWaveAmp;
                                mat->extraRgbWavePhase[ei] = stageRgbWavePhase;
                                mat->extraRgbWaveFreq[ei] = stageRgbWaveFreq;
                                mat->extraAlphaGenVertex[ei] = stageAlphaVertex;
                                mat->extraAlphaGenConst[ei] = stageAlphaConst;
                                mat->extraAlphaGenWave[ei] = stageAlphaWave;
                                mat->extraAlphaConst[ei] = stageAlphaConstV;
                                mat->extraAlphaWaveBase[ei] = stageAlphaWaveBase;
                                mat->extraAlphaWaveAmp[ei] = stageAlphaWaveAmp;
                                mat->extraAlphaWavePhase[ei] = stageAlphaWavePhase;
                                mat->extraAlphaWaveFreq[ei] = stageAlphaWaveFreq;
                                mat->extraDepthWrite[ei] = stageDepthWrite;
                                mat->extraDepthFuncEqual[ei] = stageDepthFuncEqual;
                                mat->extraStageNumAnimFrames[ei] = 0;
                                mat->extraStageAnimFps[ei] = 0.0f;
                                mat->extraStageAnimFrames[ei] = NULL;
                                mat->extraStageAnimFrameTextures[ei] = NULL;
                                if (stageHasAnim && stageAnimCount > 0) {
                                    mat->extraStageAnimFrames[ei] = (char **)malloc((size_t)stageAnimCount * sizeof(char *));
                                    mat->extraStageAnimFrameTextures[ei] = (GLuint *)calloc((size_t)stageAnimCount, sizeof(GLuint));
                                    if (mat->extraStageAnimFrames[ei] && mat->extraStageAnimFrameTextures[ei]) {
                                        mat->extraStageNumAnimFrames[ei] = stageAnimCount;
                                        mat->extraStageAnimFps[ei] = stageAnimFps > 0.0f ? stageAnimFps : 1.0f;
                                        for (int i = 0; i < stageAnimCount; i++) {
                                            mat->extraStageAnimFrames[ei][i] = DupString(stageAnimFrames[i]);
                                        }
                                    } else {
                                        free(mat->extraStageAnimFrames[ei]);
                                        free(mat->extraStageAnimFrameTextures[ei]);
                                        mat->extraStageAnimFrames[ei] = NULL;
                                        mat->extraStageAnimFrameTextures[ei] = NULL;
                                    }
                                }
                                mat->numExtraStages++;
                            }
                        }
                    }
                }
            }
            inStage = 0;
            line = strtok_r(NULL, "\n\r", &save);
            continue;
        }
        if (strchr(line, '}') && !inStage) {
            inShader = 0;
            currentShader[0] = '\0';
            line = strtok_r(NULL, "\n\r", &save);
            continue;
        }
        tok = strtok(line, " \t");
        if (!tok) { line = strtok_r(NULL, "\n\r", &save); continue; }
        if (currentShader[0]) {
            material_info_t *mat = FindOrAddMaterialInfo(bsp, currentShader);
            if (!mat) { line = strtok_r(NULL, "\n\r", &save); continue; }
            if (StrIEqual(tok, "polygonOffset")) {
                mat->polygonOffset = 1;
                line = strtok_r(NULL, "\n\r", &save);
                continue;
            }
            if (StrIEqual(tok, "qer_editorimage")) {
                char *arg = strtok(NULL, " \t");
                if (arg && arg[0] != '$' && !mat->editorTextureName) {
                    mat->editorTextureName = DupString(arg);
                }
            } else if (StrIEqual(tok, "cull") && !inStage) {
                char *mode = strtok(NULL, " \t");
                if (mode && (StrIEqual(mode, "none") || StrIEqual(mode, "disable") || StrIEqual(mode, "twosided"))) {
                    mat->cullNone = 1;
                } else {
                    mat->cullNone = 0;
                }
            } else if (StrIEqual(tok, "sort") && !inStage) {
                char *mode = strtok(NULL, " \t");
                if (mode) {
                    if (StrIEqual(mode, "decal")) mat->sortDecal = 1;
                    if (StrIEqual(mode, "banner") || StrIEqual(mode, "underwater") || StrIEqual(mode, "additive") ||
                        StrIEqual(mode, "nearest") || StrIEqual(mode, "seeThrough")) {
                        mat->sortTransparent = 1;
                    } else {
                        double sv = atof(mode);
                        if (sv >= 6.0) mat->sortTransparent = 1;
                    }
                }
            } else if (StrIEqual(tok, "surfaceParm") && !inStage) {
                char *arg = strtok(NULL, " \t");
                if (arg) {
                    if (StrIEqual(arg, "trans")) mat->surfaceParmTrans = 1;
                    if (StrIEqual(arg, "nolightmap")) mat->surfaceParmNoLightmap = 1;
                    if (StrIEqual(arg, "water")) mat->surfaceParmWater = 1;
                }
            } else if (StrIEqual(tok, "deformVertexes") && !inStage) {
                char *mode = strtok(NULL, " \t");
                if (mode && StrIEqual(mode, "wave")) {
                    char *spread = strtok(NULL, " \t");
                    char *func = strtok(NULL, " \t");
                    char *base = strtok(NULL, " \t");
                    char *amp = strtok(NULL, " \t");
                    char *phase = strtok(NULL, " \t");
                    char *freq = strtok(NULL, " \t");
                    if (spread && func && base && amp && phase && freq && mat->numDeformWaves < MAX_DEFORM_WAVES) {
                        int wi = mat->numDeformWaves++;
                        mat->deformSpread[wi] = (float)atof(spread);
                        mat->deformBase[wi] = (float)atof(base);
                        mat->deformAmp[wi] = (float)atof(amp);
                        mat->deformPhase[wi] = (float)atof(phase);
                        mat->deformFreq[wi] = (float)atof(freq);
                        mat->deformFunc[wi] = WAVE_SIN;
                        if (StrIEqual(func, "triangle")) mat->deformFunc[wi] = WAVE_TRIANGLE;
                        else if (StrIEqual(func, "sawtooth")) mat->deformFunc[wi] = WAVE_SAWTOOTH;
                        else if (StrIEqual(func, "inversesawtooth")) mat->deformFunc[wi] = WAVE_INVERSE_SAWTOOTH;
                        else if (StrIEqual(func, "square")) mat->deformFunc[wi] = WAVE_SQUARE;
                    }
                } else if (mode && StrIEqual(mode, "move")) {
                    char *x = strtok(NULL, " \t");
                    char *y = strtok(NULL, " \t");
                    char *z = strtok(NULL, " \t");
                    char *func = strtok(NULL, " \t");
                    char *base = strtok(NULL, " \t");
                    char *amp = strtok(NULL, " \t");
                    char *phase = strtok(NULL, " \t");
                    char *freq = strtok(NULL, " \t");
                    if (x && y && z && func && base && amp && phase && freq && mat->numDeformMoves < MAX_DEFORM_MOVES) {
                        int mi = mat->numDeformMoves++;
                        mat->deformMoveVec[mi][0] = (float)atof(x);
                        mat->deformMoveVec[mi][1] = (float)atof(y);
                        mat->deformMoveVec[mi][2] = (float)atof(z);
                        mat->deformMoveBase[mi] = (float)atof(base);
                        mat->deformMoveAmp[mi] = (float)atof(amp);
                        mat->deformMovePhase[mi] = (float)atof(phase);
                        mat->deformMoveFreq[mi] = (float)atof(freq);
                        mat->deformMoveFunc[mi] = WAVE_SIN;
                        if (StrIEqual(func, "triangle")) mat->deformMoveFunc[mi] = WAVE_TRIANGLE;
                        else if (StrIEqual(func, "sawtooth")) mat->deformMoveFunc[mi] = WAVE_SAWTOOTH;
                        else if (StrIEqual(func, "inversesawtooth")) mat->deformMoveFunc[mi] = WAVE_INVERSE_SAWTOOTH;
                        else if (StrIEqual(func, "square")) mat->deformMoveFunc[mi] = WAVE_SQUARE;
                    }
                } else if (mode && StrIEqual(mode, "autosprite")) {
                    mat->deformAutoSprite = 1;
                } else if (mode && StrIEqual(mode, "autosprite2")) {
                    mat->deformAutoSprite2 = 1;
                } else if (mode && (StrIEqual(mode, "text0") || StrIEqual(mode, "text1"))) {
                    mat->deformTextMode = 1;
                }
            } else if ((StrIEqual(tok, "map") || StrIEqual(tok, "clampmap") || StrIEqual(tok, "implicitMap")) && inStage) {
                char *arg = strtok(NULL, " \t");
                if (arg) {
                    stageHasMap = 1;
                    strncpy(stageMap, arg, sizeof(stageMap) - 1);
                    stageMap[sizeof(stageMap) - 1] = '\0';
                    stageMapIsLightmap = StrIEqual(arg, "$lightmap");
                }
            } else if (StrIEqual(tok, "animMap") && inStage) {
                char *fps = strtok(NULL, " \t");
                char *arg;
                if (fps) stageAnimFps = (float)atof(fps);
                while ((arg = strtok(NULL, " \t")) != NULL) {
                    if (arg[0] == '$') continue;
                    char **newFrames = (char **)realloc(stageAnimFrames, (size_t)(stageAnimCount + 1) * sizeof(char *));
                    if (!newFrames) break;
                    stageAnimFrames = newFrames;
                    stageAnimFrames[stageAnimCount] = DupString(arg);
                    if (stageAnimFrames[stageAnimCount]) stageAnimCount++;
                }
                if (stageAnimCount > 0) stageHasAnim = 1;
            } else if (StrIEqual(tok, "tcMod") && inStage) {
                char *mode = strtok(NULL, " \t");
                char *a = strtok(NULL, " \t");
                char *b = strtok(NULL, " \t");
                if (mode && a && b && StrIEqual(mode, "scale")) {
                    stageTcScale[0] = (float)atof(a);
                    stageTcScale[1] = (float)atof(b);
                    stageHasTcScale = 1;
                } else if (mode && a && b && StrIEqual(mode, "scroll")) {
                    stageTcScroll[0] = (float)atof(a);
                    stageTcScroll[1] = (float)atof(b);
                    stageHasTcScroll = 1;
                } else if (mode && a && StrIEqual(mode, "rotate")) {
                    stageTcRotate = (float)atof(a);
                    stageHasTcRotate = 1;
                } else if (mode && a && b && StrIEqual(mode, "turb")) {
                    char *c = strtok(NULL, " \t");
                    char *d = strtok(NULL, " \t");
                    stageTurbBase = (float)atof(a);
                    stageTurbAmp = (float)atof(b);
                    stageTurbPhase = c ? (float)atof(c) : 0.0f;
                    stageTurbFreq = d ? (float)atof(d) : 1.0f;
                    stageHasTcTurb = 1;
                } else if (mode && StrIEqual(mode, "swap")) {
                    stageHasTcSwap = 1;
                } else if (mode && a && b && StrIEqual(mode, "stretch")) {
                    char *c = strtok(NULL, " \t");
                    char *d = strtok(NULL, " \t");
                    stageStretchBase = (float)atof(a);
                    stageStretchAmp = (float)atof(b);
                    stageStretchPhase = c ? (float)atof(c) : 0.0f;
                    stageStretchFreq = d ? (float)atof(d) : 1.0f;
                    stageHasTcStretch = 1;
                } else if (mode && a && b && StrIEqual(mode, "transform")) {
                    char *c = strtok(NULL, " \t");
                    char *d = strtok(NULL, " \t");
                    char *e = strtok(NULL, " \t");
                    char *f = strtok(NULL, " \t");
                    if (c && d && e && f) {
                        stageTransform[0][0] = (float)atof(a);
                        stageTransform[0][1] = (float)atof(b);
                        stageTransform[1][0] = (float)atof(c);
                        stageTransform[1][1] = (float)atof(d);
                        stageTranslate[0] = (float)atof(e);
                        stageTranslate[1] = (float)atof(f);
                        stageHasTcTransform = 1;
                    }
                }
            } else if (StrIEqual(tok, "blendFunc") && inStage) {
                char *a = strtok(NULL, " \t");
                char *b = strtok(NULL, " \t");
                if (a && !b) {
                    if (StrIEqual(a, "add")) { stageBlendSrc = GL_ONE; stageBlendDst = GL_ONE; stageHasBlend = 1; }
                    else if (StrIEqual(a, "filter")) { stageBlendSrc = GL_DST_COLOR; stageBlendDst = GL_ZERO; stageHasBlend = 1; }
                    else if (StrIEqual(a, "blend")) { stageBlendSrc = GL_SRC_ALPHA; stageBlendDst = GL_ONE_MINUS_SRC_ALPHA; stageHasBlend = 1; }
                } else if (a && b) {
                    stageBlendSrc = ParseBlendFactor(a, stageBlendSrc);
                    stageBlendDst = ParseBlendFactor(b, stageBlendDst);
                    stageHasBlend = 1;
                }
            } else if (StrIEqual(tok, "alphaFunc") && inStage) {
                char *mode = strtok(NULL, " \t");
                if (mode) {
                    stageHasAlphaFunc = 1;
                    if (StrIEqual(mode, "GT0")) { stageAlphaRef = 0.0f; stageAlphaFuncMode = ALPHA_FUNC_GT0; }
                    else if (StrIEqual(mode, "LT128")) { stageAlphaRef = 0.5f; stageAlphaFuncMode = ALPHA_FUNC_LT128; }
                    else if (StrIEqual(mode, "GE128")) { stageAlphaRef = 0.5f; stageAlphaFuncMode = ALPHA_FUNC_GE128; }
                }
            } else if (StrIEqual(tok, "depthWrite") && inStage) {
                stageDepthWrite = 1;
            } else if (StrIEqual(tok, "depthFunc") && inStage) {
                char *mode = strtok(NULL, " \t");
                if (mode && StrIEqual(mode, "equal")) stageDepthFuncEqual = 1;
            } else if (StrIEqual(tok, "rgbGen") && inStage) {
                char *mode = strtok(NULL, " \t");
                if (mode && StrIEqual(mode, "identity")) stageRgbIdentity = 1;
                else if (mode && StrIEqual(mode, "vertex")) stageRgbVertex = 1;
                else if (mode && StrIEqual(mode, "const")) {
                    char *r = strtok(NULL, " \t"), *g = strtok(NULL, " \t"), *b = strtok(NULL, " \t");
                    if (r && g && b) {
                        stageRgbConstV[0] = (float)atof(r);
                        stageRgbConstV[1] = (float)atof(g);
                        stageRgbConstV[2] = (float)atof(b);
                        stageRgbConst = 1;
                    }
                } else if (mode && StrIEqual(mode, "wave")) {
                    char *func = strtok(NULL, " \t");
                    char *base = strtok(NULL, " \t");
                    char *amp = strtok(NULL, " \t");
                    char *phase = strtok(NULL, " \t");
                    char *freq = strtok(NULL, " \t");
                    (void)func;
                    if (base && amp && phase && freq) {
                        stageRgbWaveBase = (float)atof(base);
                        stageRgbWaveAmp = (float)atof(amp);
                        stageRgbWavePhase = (float)atof(phase);
                        stageRgbWaveFreq = (float)atof(freq);
                        stageRgbWave = 1;
                    }
                }
            } else if (StrIEqual(tok, "alphaGen") && inStage) {
                char *mode = strtok(NULL, " \t");
                if (mode && StrIEqual(mode, "identity")) stageAlphaIdentity = 1;
                else if (mode && StrIEqual(mode, "vertex")) stageAlphaVertex = 1;
                else if (mode && StrIEqual(mode, "const")) {
                    char *a = strtok(NULL, " \t");
                    if (a) { stageAlphaConstV = (float)atof(a); stageAlphaConst = 1; }
                } else if (mode && StrIEqual(mode, "wave")) {
                    char *func = strtok(NULL, " \t");
                    char *base = strtok(NULL, " \t");
                    char *amp = strtok(NULL, " \t");
                    char *phase = strtok(NULL, " \t");
                    char *freq = strtok(NULL, " \t");
                    (void)func;
                    if (base && amp && phase && freq) {
                        stageAlphaWaveBase = (float)atof(base);
                        stageAlphaWaveAmp = (float)atof(amp);
                        stageAlphaWavePhase = (float)atof(phase);
                        stageAlphaWaveFreq = (float)atof(freq);
                        stageAlphaWave = 1;
                    }
                }
            } else if (StrIEqual(tok, "tcGen") && inStage) {
                char *mode = strtok(NULL, " \t");
                if (mode && StrIEqual(mode, "environment")) stageTcEnv = 1;
                else if (mode && (StrIEqual(mode, "lightmap") || StrIEqual(mode, "base"))) stageTcLightmap = StrIEqual(mode, "lightmap") ? 1 : 0;
                else if (mode && StrIEqual(mode, "vector")) {
                    char *a = strtok(NULL, " \t()");
                    char *b = strtok(NULL, " \t()");
                    char *c = strtok(NULL, " \t()");
                    char *d = strtok(NULL, " \t()");
                    char *e = strtok(NULL, " \t()");
                    char *f = strtok(NULL, " \t()");
                    if (a && b && c && d && e && f) {
                        stageTcGenVecS[0] = (float)atof(a);
                        stageTcGenVecS[1] = (float)atof(b);
                        stageTcGenVecS[2] = (float)atof(c);
                        stageTcGenVecT[0] = (float)atof(d);
                        stageTcGenVecT[1] = (float)atof(e);
                        stageTcGenVecT[2] = (float)atof(f);
                        stageHasTcGenVector = 1;
                    }
                }
            }
        }
        line = strtok_r(NULL, "\n\r", &save);
    }
    if (stageAnimFrames) {
        for (int i = 0; i < stageAnimCount; i++) free(stageAnimFrames[i]);
        free(stageAnimFrames);
    }
    free(data);
    free(buf);
}

static void LoadMaterialScripts(bsp_data_t *bsp) {
    char scriptsDir[1400];
    DIR *d;
    struct dirent *ent;
    snprintf(scriptsDir, sizeof(scriptsDir), "%s/scripts", bsp->gameRoot);
    d = opendir(scriptsDir);
    if (!d) return;
    while ((ent = readdir(d)) != NULL) {
        size_t n = strlen(ent->d_name);
        char path[1600];
        if (n < 8) continue;
        if (strcmp(ent->d_name + n - 7, ".shader") != 0) continue;
        if (strlen(scriptsDir) + 1 + n + 1 > sizeof(path)) continue;
        strcpy(path, scriptsDir);
        strcat(path, "/");
        strcat(path, ent->d_name);
        ParseShaderScript(bsp, path);
    }
    closedir(d);
}

static int LoadBSP(const char *path, bsp_data_t *bsp) {
    uint8_t *buf = NULL;
    size_t len = 0;
    const dheader_t *hdr;
    const lump_t *lv, *li, *ls, *llm, *lsh;

    memset(bsp, 0, sizeof(*bsp));
    if (!ReadFile(path, &buf, &len)) {
        fprintf(stderr, "Failed to read BSP: %s\n", path);
        return 0;
    }
    if (len < sizeof(dheader_t)) {
        fprintf(stderr, "File too small for BSP header\n");
        free(buf);
        return 0;
    }

    hdr = (const dheader_t *)buf;
    if (hdr->ident != BSP_IDENT || hdr->version != BSP_VERSION) {
        fprintf(stderr, "Invalid BSP header ident/version\n");
        free(buf);
        return 0;
    }

    lv = &hdr->lumps[LUMP_DRAWVERTS];
    li = &hdr->lumps[LUMP_DRAWINDEXES];
    ls = &hdr->lumps[LUMP_SURFACES];
    llm = &hdr->lumps[LUMP_LIGHTMAPS];
    lsh = &hdr->lumps[1];

    if (lv->fileofs < 0 || li->fileofs < 0 || ls->fileofs < 0 || lsh->fileofs < 0 ||
        lv->filelen < 0 || li->filelen < 0 || ls->filelen < 0 || lsh->filelen < 0 ||
        (size_t)(lv->fileofs + lv->filelen) > len ||
        (size_t)(li->fileofs + li->filelen) > len ||
        (size_t)(ls->fileofs + ls->filelen) > len ||
        (size_t)(lsh->fileofs + lsh->filelen) > len) {
        fprintf(stderr, "Invalid BSP lump bounds\n");
        free(buf);
        return 0;
    }

    BuildGameRootFromBspPath(path, bsp->gameRoot, sizeof(bsp->gameRoot));
    bsp->numVerts = lv->filelen / (int)sizeof(drawVert_t);
    bsp->numIndices = li->filelen / (int)sizeof(int32_t);
    bsp->numSurfaces = ls->filelen / (int)sizeof(dsurface_t);
    bsp->numShaders = lsh->filelen / (int)sizeof(dshader_t);
    if (bsp->numVerts <= 0 || bsp->numIndices <= 0 || bsp->numSurfaces <= 0) {
        fprintf(stderr, "BSP has empty required lumps\n");
        free(buf);
        return 0;
    }

    bsp->verts = (drawVert_t *)malloc((size_t)bsp->numVerts * sizeof(drawVert_t));
    bsp->indices = (int32_t *)malloc((size_t)bsp->numIndices * sizeof(int32_t));
    bsp->surfaces = (dsurface_t *)malloc((size_t)bsp->numSurfaces * sizeof(dsurface_t));
    bsp->shaders = (dshader_t *)malloc((size_t)bsp->numShaders * sizeof(dshader_t));
    bsp->shaderTextures = (GLuint *)calloc((size_t)bsp->numShaders, sizeof(GLuint));
    if (!bsp->verts || !bsp->indices || !bsp->surfaces || !bsp->shaders || !bsp->shaderTextures) {
        fprintf(stderr, "Out of memory loading BSP\n");
        free(buf);
        return 0;
    }

    memcpy(bsp->verts, buf + lv->fileofs, (size_t)bsp->numVerts * sizeof(drawVert_t));
    memcpy(bsp->indices, buf + li->fileofs, (size_t)bsp->numIndices * sizeof(int32_t));
    memcpy(bsp->surfaces, buf + ls->fileofs, (size_t)bsp->numSurfaces * sizeof(dsurface_t));
    memcpy(bsp->shaders, buf + lsh->fileofs, (size_t)bsp->numShaders * sizeof(dshader_t));
    if (llm->fileofs >= 0 && llm->filelen > 0 && (size_t)(llm->fileofs + llm->filelen) <= len) {
        const int lightmapBytes = 128 * 128 * 3;
        bsp->numLightmaps = llm->filelen / lightmapBytes;
        if (bsp->numLightmaps > 0) {
            bsp->lightmapData = (uint8_t *)malloc((size_t)bsp->numLightmaps * (size_t)lightmapBytes);
            if (bsp->lightmapData) {
                memcpy(bsp->lightmapData, buf + llm->fileofs, (size_t)bsp->numLightmaps * (size_t)lightmapBytes);
            } else {
                bsp->numLightmaps = 0;
            }
        }
    }
    LoadMaterialScripts(bsp);
    BuildShaderMaterialCache(bsp);
    if (!BuildSurfaceCenters(bsp)) {
        free(buf);
        return 0;
    }
    if (!InitTransparentSortBuffers(bsp)) {
        free(buf);
        return 0;
    }
    if (!BuildSurfaceIndexCache(bsp)) {
        free(buf);
        return 0;
    }
    free(buf);
    return 1;
}

static void FreeBSP(bsp_data_t *bsp) {
    free(bsp->verts);
    free(bsp->indices);
    if (bsp->surfaceIndices) {
        for (int i = 0; i < bsp->numSurfaces; i++) free(bsp->surfaceIndices[i]);
    }
    free(bsp->surfaceIndices);
    free(bsp->surfaceIndexCounts);
    free(bsp->surfaces);
    free(bsp->shaders);
    free(bsp->shaderTextures);
    free(bsp->lightmapTextures);
    if (bsp->materials) {
        for (int i = 0; i < bsp->numMaterials; i++) {
            material_info_t *m = &bsp->materials[i];
            free(m->shaderName);
            free(m->textureName);
            free(m->editorTextureName);
            if (m->textureHandle != 0) glDeleteTextures(1, &m->textureHandle);
            free(m->secondaryTextureName);
            if (m->secondaryTextureHandle != 0) glDeleteTextures(1, &m->secondaryTextureHandle);
            if (m->secondaryAnimFrames) {
                for (int k = 0; k < m->secondaryNumAnimFrames; k++) free(m->secondaryAnimFrames[k]);
            }
            if (m->secondaryAnimFrameTextures) {
                for (int k = 0; k < m->secondaryNumAnimFrames; k++) {
                    if (m->secondaryAnimFrameTextures[k] != 0) glDeleteTextures(1, &m->secondaryAnimFrameTextures[k]);
                }
            }
            free(m->secondaryAnimFrames);
            free(m->secondaryAnimFrameTextures);
            for (int ei = 0; ei < m->numExtraStages; ei++) {
                free(m->extraStageTextureName[ei]);
                if (m->extraStageTextureHandle[ei] != 0) glDeleteTextures(1, &m->extraStageTextureHandle[ei]);
                if (m->extraStageAnimFrames[ei]) {
                    for (int k = 0; k < m->extraStageNumAnimFrames[ei]; k++) free(m->extraStageAnimFrames[ei][k]);
                }
                if (m->extraStageAnimFrameTextures[ei]) {
                    for (int k = 0; k < m->extraStageNumAnimFrames[ei]; k++) {
                        if (m->extraStageAnimFrameTextures[ei][k] != 0) glDeleteTextures(1, &m->extraStageAnimFrameTextures[ei][k]);
                    }
                }
                free(m->extraStageAnimFrames[ei]);
                free(m->extraStageAnimFrameTextures[ei]);
            }
            if (m->animFrames) {
                for (int k = 0; k < m->numAnimFrames; k++) free(m->animFrames[k]);
            }
            if (m->animFrameTextures) {
                for (int k = 0; k < m->numAnimFrames; k++) {
                    if (m->animFrameTextures[k] != 0) glDeleteTextures(1, &m->animFrameTextures[k]);
                }
            }
            free(m->animFrames);
            free(m->animFrameTextures);
            for (int si = 0; si < m->numFullStages; si++) {
                FreeShaderStage(&m->fullStages[si]);
            }
        }
    }
    free(bsp->materials);
    free(bsp->shaderMaterialIndex);
    free(bsp->surfaceCenters);
    free(bsp->transparentSurfaceOrder);
    free(bsp->transparentSurfaceDist2);
    free(bsp->lightmapData);
    memset(bsp, 0, sizeof(*bsp));
}

static void SetProjection(float fovYDeg, float aspect, float zNear, float zFar) {
    float f = 1.0f / tanf(fovYDeg * (float)M_PI / 360.0f);
    float m[16] = {0};
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zFar + zNear) / (zNear - zFar);
    m[11] = -1.0f;
    m[14] = (2.0f * zFar * zNear) / (zNear - zFar);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(m);
}

static void ApplyCamera(void) {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(-g_pitch, 1.0f, 0.0f, 0.0f);
    glRotatef(-g_yaw, 0.0f, 0.0f, 1.0f);
    glTranslatef(-g_camPos[0], -g_camPos[1], -g_camPos[2]);
}

static void DrawFullMaterialStages(
    bsp_data_t *bsp, const dsurface_t *s, material_info_t *mat, const drawVert_t *base,
    int indexCount, uint32_t *idx, float *tc, unsigned char *rgba, float timeSec, GLuint whiteTex) {
    for (int si = 0; si < mat->numFullStages; si++) {
        shader_stage_t *st = &mat->fullStages[si];
        float emul[3] = {1.0f, 1.0f, 1.0f};
        float ealpha = 1.0f;
        GLuint tex;
        if (st->textureName && StrIEqual(st->textureName, "$lightmap")) {
            tex = GetSurfaceLightmapTexture(bsp, s->lightmapNum, whiteTex);
        } else if (st->textureName && StrIEqual(st->textureName, "$dlight")) {
            continue;
        } else {
            tex = GetFullStageTextureNow(bsp, st, whiteTex, timeSec, st->hasAlphaFunc);
        }
        if (tex == 0) continue;
        if (st->rgbGenConst) {
            emul[0] = st->rgbConst[0]; emul[1] = st->rgbConst[1]; emul[2] = st->rgbConst[2];
        }
        if (st->alphaGenConst) ealpha = st->alphaConst;
        if (st->rgbGenWave) {
            float w = st->rgbWaveBase + sinf((st->rgbWavePhase + timeSec * st->rgbWaveFreq) * 6.28318f) * st->rgbWaveAmp;
            emul[0] = emul[1] = emul[2] = w;
        }
        if (st->alphaGenWave) {
            ealpha = st->alphaWaveBase + sinf((st->alphaWavePhase + timeSec * st->alphaWaveFreq) * 6.28318f) * st->alphaWaveAmp;
        }
        for (int vi = 0; vi < s->numVerts; vi++) {
            const drawVert_t *v = &base[vi];
            float u, vv;
            if (st->tcGenLightmap) {
                u = v->lightmap[0];
                vv = v->lightmap[1];
            } else if (st->tcGenEnvironment) {
                float viewerX = g_camPos[0] - v->xyz[0];
                float viewerY = g_camPos[1] - v->xyz[1];
                float viewerZ = g_camPos[2] - v->xyz[2];
                float invLen = 1.0f / sqrtf(viewerX * viewerX + viewerY * viewerY + viewerZ * viewerZ + 1e-6f);
                float d, reflectedY, reflectedZ;
                viewerX *= invLen; viewerY *= invLen; viewerZ *= invLen;
                d = v->normal[0] * viewerX + v->normal[1] * viewerY + v->normal[2] * viewerZ;
                reflectedY = v->normal[1] * 2.0f * d - viewerY;
                reflectedZ = v->normal[2] * 2.0f * d - viewerZ;
                u = 0.5f + reflectedY * 0.5f;
                vv = 0.5f - reflectedZ * 0.5f;
            } else if (st->hasTcGenVector) {
                u = v->xyz[0] * st->tcGenVecS[0] + v->xyz[1] * st->tcGenVecS[1] + v->xyz[2] * st->tcGenVecS[2];
                vv = v->xyz[0] * st->tcGenVecT[0] + v->xyz[1] * st->tcGenVecT[1] + v->xyz[2] * st->tcGenVecT[2];
            } else {
                u = v->st[0];
                vv = v->st[1];
            }
            if (st->hasTcScale) { u *= st->tcScale[0]; vv *= st->tcScale[1]; }
            if (st->hasTcRotate) {
                float ang = st->tcRotate * timeSec * (float)M_PI / 180.0f;
                float c = cosf(ang), s2 = sinf(ang);
                float ru = (u - 0.5f) * c - (vv - 0.5f) * s2 + 0.5f;
                float rv = (u - 0.5f) * s2 + (vv - 0.5f) * c + 0.5f;
                u = ru; vv = rv;
            }
            if (st->hasTcTurb) {
                float tb = st->tcTurbBase + sinf(st->tcTurbPhase + timeSec * st->tcTurbFreq) * st->tcTurbAmp;
                u += sinf((vv + timeSec) * 6.28318f) * tb;
                vv += cosf((u + timeSec) * 6.28318f) * tb;
            }
            if (st->hasTcStretch) {
                float str = st->tcStretchBase + sinf((st->tcStretchPhase + timeSec * st->tcStretchFreq) * 6.28318f) * st->tcStretchAmp;
                if (fabsf(str) > 1e-4f) {
                    float p = 1.0f / str;
                    u = (u - 0.5f) * p + 0.5f;
                    vv = (vv - 0.5f) * p + 0.5f;
                }
            }
            if (st->hasTcTransform) {
                float tu = u, tv = vv;
                u = tu * st->tcTransform[0][0] + tv * st->tcTransform[1][0] + st->tcTranslate[0];
                vv = tu * st->tcTransform[0][1] + tv * st->tcTransform[1][1] + st->tcTranslate[1];
            }
            if (st->hasTcSwap) {
                float t = u; u = vv; vv = t;
            }
            tc[vi * 2 + 0] = u + (st->hasTcScroll ? st->tcScroll[0] * timeSec : 0.0f);
            tc[vi * 2 + 1] = vv + (st->hasTcScroll ? st->tcScroll[1] * timeSec : 0.0f);
            if (rgba) {
                rgba[vi * 4 + 0] = st->rgbGenVertex ? v->color[0] : (unsigned char)(emul[0] * 255.0f);
                rgba[vi * 4 + 1] = st->rgbGenVertex ? v->color[1] : (unsigned char)(emul[1] * 255.0f);
                rgba[vi * 4 + 2] = st->rgbGenVertex ? v->color[2] : (unsigned char)(emul[2] * 255.0f);
                rgba[vi * 4 + 3] = st->alphaGenVertex ? v->color[3] : (unsigned char)(ealpha * 255.0f);
            }
        }
        if (st->hasBlend) {
            glEnable(GL_BLEND);
            glBlendFunc(st->blendSrc, st->blendDst);
        } else {
            glDisable(GL_BLEND);
        }
        if (st->hasAlphaFunc) {
            glEnable(GL_ALPHA_TEST);
            if (st->alphaFuncMode == ALPHA_FUNC_LT128) glAlphaFunc(GL_LESS, st->alphaRef);
            else if (st->alphaFuncMode == ALPHA_FUNC_GE128) glAlphaFunc(GL_GEQUAL, st->alphaRef);
            else glAlphaFunc(GL_GREATER, st->alphaRef);
        } else {
            glDisable(GL_ALPHA_TEST);
        }
        if (st->depthFuncEqual) glDepthFunc(GL_EQUAL);
        else glDepthFunc(GL_LEQUAL);
        if (st->depthWrite) glDepthMask(GL_TRUE);
        else if (st->hasBlend) glDepthMask(GL_FALSE);
        else glDepthMask(GL_TRUE);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexCoordPointer(2, GL_FLOAT, 0, tc);
        if (rgba) {
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, rgba);
        } else {
            glDisableClientState(GL_COLOR_ARRAY);
            glColor4f(emul[0], emul[1], emul[2], ealpha);
        }
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, idx);
    }
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
}

static void DrawSurfaceTriangles(bsp_data_t *bsp, int surfIndex, const dsurface_t *s, int patchOnly, int noPatch, GLuint whiteTex, int lightmapPass, float timeSec) {
    material_info_t *mat = NULL;
    int useFullStages = 0;
    int isDecalSurf = IsSurfaceDecal(bsp, s);
    float tcScaleU = 1.0f, tcScaleV = 1.0f;
    float tcScrollU = 0.0f, tcScrollV = 0.0f;
    float tcCos = 1.0f, tcSin = 0.0f;
    float colorMul[3] = {1.0f, 1.0f, 1.0f}, alphaMul = 1.0f;
    float turb = 0.0f;
    if (s->surfaceType != MST_PLANAR && s->surfaceType != MST_TRIANGLE_SOUP && s->surfaceType != MST_PATCH) {
        return;
    }
    if (patchOnly && s->surfaceType != MST_PATCH) return;
    if (noPatch && s->surfaceType == MST_PATCH) return;
    if (!bsp->surfaceIndices || !bsp->surfaceIndexCounts || surfIndex < 0 || surfIndex >= bsp->numSurfaces) return;
    if (bsp->surfaceIndexCounts[surfIndex] <= 0 || !bsp->surfaceIndices[surfIndex]) return;
    if (s->shaderNum >= 0 && s->shaderNum < bsp->numShaders && bsp->shaderMaterialIndex) {
        int mi = bsp->shaderMaterialIndex[s->shaderNum];
        if (mi >= 0 && mi < bsp->numMaterials) mat = &bsp->materials[mi];
    }
    (void)useFullStages;
    useFullStages = 0;
    if (!lightmapPass) {
        if (mat && mat->hasTcScale) { tcScaleU = mat->tcScale[0]; tcScaleV = mat->tcScale[1]; }
        if (mat && mat->hasTcScroll) {
            tcScrollU = mat->tcScroll[0] * timeSec;
            tcScrollV = mat->tcScroll[1] * timeSec;
        }
        if (mat && mat->hasTcRotate) {
            float ang = mat->tcRotateDegPerSec * timeSec * (float)M_PI / 180.0f;
            tcCos = cosf(ang);
            tcSin = sinf(ang);
        }
        if (mat && mat->hasTcTurb) {
            turb = mat->tcTurbBase + sinf(mat->tcTurbPhase + timeSec * mat->tcTurbFreq) * mat->tcTurbAmp;
        }
        if (mat && mat->rgbGenIdentity) colorMul[0] = colorMul[1] = colorMul[2] = 1.0f;
        if (mat && mat->rgbGenConst) {
            colorMul[0] = mat->rgbConst[0]; colorMul[1] = mat->rgbConst[1]; colorMul[2] = mat->rgbConst[2];
        }
        if (mat && mat->rgbGenWave) {
            float w = mat->rgbWaveBase + sinf((mat->rgbWavePhase + timeSec * mat->rgbWaveFreq) * 6.28318f) * mat->rgbWaveAmp;
            colorMul[0] = colorMul[1] = colorMul[2] = w;
        }
        if (mat && mat->alphaGenIdentity) alphaMul = 1.0f;
        if (mat && mat->alphaGenConst) alphaMul = mat->alphaConst;
        if (mat && mat->alphaGenWave) {
            alphaMul = mat->alphaWaveBase + sinf((mat->alphaWavePhase + timeSec * mat->alphaWaveFreq) * 6.28318f) * mat->alphaWaveAmp;
        }
        if (isDecalSurf) {
            /* Decals in RTCW are usually authored as explicit blend/alpha stages.
             * Vertex color modulation often over-fades them in this viewer. */
            colorMul[0] = colorMul[1] = colorMul[2] = 1.0f;
            alphaMul = 1.0f;
        }
    }
    if (lightmapPass && IsAlphaBlendMaterial(mat)) {
        /* Avoid dark halos and wrong alpha composition on transparent surfaces. */
        return;
    }
    if (lightmapPass && mat && mat->surfaceParmNoLightmap) {
        return;
    }
    if (lightmapPass && mat && mat->tcGenEnvironment) {
        return;
    }

    if (s->firstVert < 0 || s->numVerts <= 0 || s->firstVert + s->numVerts > bsp->numVerts) return;
    {
        const drawVert_t *base = &bsp->verts[s->firstVert];
        uint32_t *idx = bsp->surfaceIndices[surfIndex];
        int indexCount = bsp->surfaceIndexCounts[surfIndex];
        float *tc = (float *)malloc((size_t)s->numVerts * 2u * sizeof(float));
        float *pos = NULL;
        float *baseTc = NULL;
        unsigned char *rgba = NULL;
        if (!tc) return;
        if (mat && (mat->numDeformWaves > 0 || mat->numDeformMoves > 0 || mat->deformAutoSprite || mat->deformAutoSprite2)) {
            pos = (float *)malloc((size_t)s->numVerts * 3u * sizeof(float));
            if (!pos) { free(tc); return; }
            for (int i = 0; i < s->numVerts; i++) {
                const drawVert_t *v = &base[i];
                float off = 0.0f, mx = 0.0f, my = 0.0f, mz = 0.0f;
                for (int wv = 0; wv < mat->numDeformWaves; wv++) {
                    float spread = mat->deformSpread[wv];
                    float arg = mat->deformPhase[wv] + timeSec * mat->deformFreq[wv];
                    if (fabsf(spread) > 1e-5f) arg += (v->xyz[0] + v->xyz[1] + v->xyz[2]) / spread;
                    off += mat->deformBase[wv] + EvalWaveForm(mat->deformFunc[wv], arg) * mat->deformAmp[wv];
                }
                for (int mv = 0; mv < mat->numDeformMoves; mv++) {
                    float w = mat->deformMoveBase[mv] +
                              EvalWaveForm(mat->deformMoveFunc[mv], mat->deformMovePhase[mv] + timeSec * mat->deformMoveFreq[mv]) *
                              mat->deformMoveAmp[mv];
                    mx += mat->deformMoveVec[mv][0] * w;
                    my += mat->deformMoveVec[mv][1] * w;
                    mz += mat->deformMoveVec[mv][2] * w;
                }
                pos[i * 3 + 0] = v->xyz[0] + v->normal[0] * off + mx;
                pos[i * 3 + 1] = v->xyz[1] + v->normal[1] * off + my;
                pos[i * 3 + 2] = v->xyz[2] + v->normal[2] * off + mz;
            }
            if ((mat->deformAutoSprite || mat->deformAutoSprite2) && s->numVerts >= 3) {
                float cx = 0.0f, cy = 0.0f, cz = 0.0f;
                float tx = base[1].xyz[0] - base[0].xyz[0];
                float ty = base[1].xyz[1] - base[0].xyz[1];
                float tz = base[1].xyz[2] - base[0].xyz[2];
                float tlen = sqrtf(tx * tx + ty * ty + tz * tz);
                float pitchRad = g_pitch * (float)M_PI / 180.0f;
                float yawRad = g_yaw * (float)M_PI / 180.0f;
                float camRight[3] = { cosf(yawRad), sinf(yawRad), 0.0f };
                float camUp[3] = { sinf(yawRad) * sinf(pitchRad), -cosf(yawRad) * sinf(pitchRad), cosf(pitchRad) };
                if (tlen < 1e-5f) tlen = 1.0f;
                tx /= tlen; ty /= tlen; tz /= tlen;
                for (int i = 0; i < s->numVerts; i++) {
                    cx += base[i].xyz[0];
                    cy += base[i].xyz[1];
                    cz += base[i].xyz[2];
                }
                cx /= (float)s->numVerts; cy /= (float)s->numVerts; cz /= (float)s->numVerts;
                for (int i = 0; i < s->numVerts; i++) {
                    float ox = base[i].xyz[0] - cx;
                    float oy = base[i].xyz[1] - cy;
                    float oz = base[i].xyz[2] - cz;
                    float su = ox * tx + oy * ty + oz * tz;
                    float sv = ox * base[0].normal[0] + oy * base[0].normal[1] + oz * base[0].normal[2];
                    pos[i * 3 + 0] = cx + camRight[0] * su + camUp[0] * sv;
                    pos[i * 3 + 1] = cy + camRight[1] * su + camUp[1] * sv;
                    pos[i * 3 + 2] = cz + camRight[2] * su + camUp[2] * sv;
                }
            }
        }
        if (!lightmapPass && !isDecalSurf && mat && (mat->rgbGenVertex || mat->alphaGenVertex)) {
            rgba = (unsigned char *)malloc((size_t)s->numVerts * 4u);
            if (!rgba) { free(tc); free(pos); return; }
        }
        glBindTexture(GL_TEXTURE_2D, lightmapPass ?
            GetSurfaceLightmapTexture(bsp, s->lightmapNum, whiteTex) :
            GetSurfaceTexture(bsp, s->shaderNum, whiteTex, timeSec));
        if (lightmapPass) {
            /* Match classic lightmap composition: base * lightmap. */
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
        }
        if (!lightmapPass && mat && mat->hasBlend) {
            glEnable(GL_BLEND);
            glBlendFunc(mat->blendSrc, mat->blendDst);
        }
        if (!lightmapPass && mat && mat->hasAlphaFunc) {
            glEnable(GL_ALPHA_TEST);
            if (mat->alphaFuncMode == ALPHA_FUNC_LT128) glAlphaFunc(GL_LESS, mat->alphaRef);
            else if (mat->alphaFuncMode == ALPHA_FUNC_GE128) glAlphaFunc(GL_GEQUAL, mat->alphaRef);
            else glAlphaFunc(GL_GREATER, mat->alphaRef);
        }
        if (!lightmapPass && mat && mat->depthFuncEqual && !IsAlphaBlendMaterial(mat)) glDepthFunc(GL_EQUAL);
        if (!lightmapPass && mat && mat->depthWrite) glDepthMask(GL_TRUE);
        if (!lightmapPass && IsAlphaBlendMaterial(mat)) glDepthMask(GL_FALSE);
        for (int i = 0; i < s->numVerts; i++) {
            const drawVert_t *v = &base[i];
            float u, vv, ru, rv;
            if (lightmapPass) {
                tc[i * 2 + 0] = v->lightmap[0];
                tc[i * 2 + 1] = v->lightmap[1];
            } else {
                if (mat && mat->tcGenEnvironment) {
                    float viewerX = g_camPos[0] - v->xyz[0];
                    float viewerY = g_camPos[1] - v->xyz[1];
                    float viewerZ = g_camPos[2] - v->xyz[2];
                    float invLen = 1.0f / sqrtf(viewerX * viewerX + viewerY * viewerY + viewerZ * viewerZ + 1e-6f);
                    float d, reflectedY, reflectedZ;
                    viewerX *= invLen; viewerY *= invLen; viewerZ *= invLen;
                    d = v->normal[0] * viewerX + v->normal[1] * viewerY + v->normal[2] * viewerZ;
                    reflectedY = v->normal[1] * 2.0f * d - viewerY;
                    reflectedZ = v->normal[2] * 2.0f * d - viewerZ;
                    u = 0.5f + reflectedY * 0.5f;
                    vv = 0.5f - reflectedZ * 0.5f;
                } else if (mat && mat->hasTcGenVector) {
                    u = v->xyz[0] * mat->tcGenVecS[0] + v->xyz[1] * mat->tcGenVecS[1] + v->xyz[2] * mat->tcGenVecS[2];
                    vv = v->xyz[0] * mat->tcGenVecT[0] + v->xyz[1] * mat->tcGenVecT[1] + v->xyz[2] * mat->tcGenVecT[2];
                } else {
                    u = v->st[0] * tcScaleU; vv = v->st[1] * tcScaleV;
                    ru = (u - 0.5f) * tcCos - (vv - 0.5f) * tcSin + 0.5f;
                    rv = (u - 0.5f) * tcSin + (vv - 0.5f) * tcCos + 0.5f;
                    u = ru; vv = rv;
                }
                if (mat && mat->hasTcTurb) {
                    u += sinf((vv + timeSec) * 6.28318f) * turb;
                    vv += cosf((u + timeSec) * 6.28318f) * turb;
                }
                if (mat && mat->hasTcStretch) {
                    float st = mat->tcStretchBase + sinf((mat->tcStretchPhase + timeSec * mat->tcStretchFreq) * 6.28318f) * mat->tcStretchAmp;
                    if (fabsf(st) > 1e-4f) {
                        float p = 1.0f / st;
                        u = (u - 0.5f) * p + 0.5f;
                        vv = (vv - 0.5f) * p + 0.5f;
                    }
                }
                if (mat && mat->hasTcTransform) {
                    float tu = u, tv = vv;
                    u = tu * mat->tcTransform[0][0] + tv * mat->tcTransform[1][0] + mat->tcTranslate[0];
                    vv = tu * mat->tcTransform[0][1] + tv * mat->tcTransform[1][1] + mat->tcTranslate[1];
                }
                if (mat && mat->hasTcSwap) {
                    float tmp = u; u = vv; vv = tmp;
                }
                tc[i * 2 + 0] = u + tcScrollU;
                tc[i * 2 + 1] = vv + tcScrollV;
                if (rgba) {
                    rgba[i * 4 + 0] = mat->rgbGenVertex ? v->color[0] : (unsigned char)(colorMul[0] * 255.0f);
                    rgba[i * 4 + 1] = mat->rgbGenVertex ? v->color[1] : (unsigned char)(colorMul[1] * 255.0f);
                    rgba[i * 4 + 2] = mat->rgbGenVertex ? v->color[2] : (unsigned char)(colorMul[2] * 255.0f);
                    rgba[i * 4 + 3] = mat->alphaGenVertex ? v->color[3] : (unsigned char)(alphaMul * 255.0f);
                }
            }
        }
        if (!lightmapPass && mat && mat->hasAlphaFunc && !mat->hasBlend && (mat->hasSecondary || mat->numExtraStages > 0)) {
            baseTc = (float *)malloc((size_t)s->numVerts * 2u * sizeof(float));
            if (baseTc) memcpy(baseTc, tc, (size_t)s->numVerts * 2u * sizeof(float));
        }
        if (!lightmapPass && !rgba) glColor4f(colorMul[0], colorMul[1], colorMul[2], alphaMul);
        if (!lightmapPass && mat && mat->cullNone) glDisable(GL_CULL_FACE);
        else glEnable(GL_CULL_FACE);
        glEnableClientState(GL_VERTEX_ARRAY);
        if (pos) glVertexPointer(3, GL_FLOAT, 0, pos);
        else glVertexPointer(3, GL_FLOAT, sizeof(drawVert_t), &base[0].xyz[0]);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, tc);
        if (!lightmapPass && rgba) {
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, rgba);
        } else {
            glDisableClientState(GL_COLOR_ARRAY);
        }
        if (useFullStages) {
            DrawFullMaterialStages(bsp, s, mat, base, indexCount, idx, tc, rgba, timeSec, whiteTex);
            goto draw_done;
        }
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, idx);
        if (!lightmapPass && mat && mat->hasSecondary) {
            GLuint sec = GetMaterialSecondaryTextureNow(bsp, mat, whiteTex, timeSec);
            if (sec != 0) {
                int secHasBlend = !(mat->secondaryBlendSrc == GL_ONE && mat->secondaryBlendDst == GL_ZERO);
                if (secHasBlend) {
                    glEnable(GL_BLEND);
                    glBlendFunc(mat->secondaryBlendSrc, mat->secondaryBlendDst);
                }
                if (mat->secondaryDepthFuncEqual) glDepthFunc(GL_EQUAL);
                if (mat->secondaryDepthWrite) glDepthMask(GL_TRUE);
                else if (secHasBlend) glDepthMask(GL_FALSE);
                if (mat->secondaryHasAlphaFunc) {
                    glEnable(GL_ALPHA_TEST);
                    if (mat->secondaryAlphaFuncMode == ALPHA_FUNC_LT128) glAlphaFunc(GL_LESS, mat->secondaryAlphaRef);
                    else if (mat->secondaryAlphaFuncMode == ALPHA_FUNC_GE128) glAlphaFunc(GL_GEQUAL, mat->secondaryAlphaRef);
                    else glAlphaFunc(GL_GREATER, mat->secondaryAlphaRef);
                }
                glBindTexture(GL_TEXTURE_2D, sec);
                glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, idx);
                if (mat->secondaryHasAlphaFunc) glDisable(GL_ALPHA_TEST);
                if (mat->secondaryDepthFuncEqual) glDepthFunc(GL_LEQUAL);
                if (mat->secondaryDepthWrite || secHasBlend) glDepthMask(GL_TRUE);
                if (secHasBlend) glDisable(GL_BLEND);
                glBindTexture(GL_TEXTURE_2D, GetSurfaceTexture(bsp, s->shaderNum, whiteTex, timeSec));
            }
        }
        if (!lightmapPass && mat && mat->numExtraStages > 0) {
            for (int ei = 0; ei < mat->numExtraStages; ei++) {
                GLuint ext = GetMaterialExtraStageTextureNow(bsp, mat, ei, whiteTex, timeSec);
                if (ext == 0) continue;
                float emul[3] = {1.0f, 1.0f, 1.0f};
                float ealpha = 1.0f;
                if (mat->extraRgbGenConst[ei]) {
                    emul[0] = mat->extraRgbConst[ei][0];
                    emul[1] = mat->extraRgbConst[ei][1];
                    emul[2] = mat->extraRgbConst[ei][2];
                }
                if (mat->extraAlphaGenConst[ei]) ealpha = mat->extraAlphaConst[ei];
                if (mat->extraRgbGenWave[ei]) {
                    float w = mat->extraRgbWaveBase[ei] + sinf((mat->extraRgbWavePhase[ei] + timeSec * mat->extraRgbWaveFreq[ei]) * 6.28318f) * mat->extraRgbWaveAmp[ei];
                    emul[0] = emul[1] = emul[2] = w;
                }
                if (mat->extraAlphaGenWave[ei]) {
                    ealpha = mat->extraAlphaWaveBase[ei] + sinf((mat->extraAlphaWavePhase[ei] + timeSec * mat->extraAlphaWaveFreq[ei]) * 6.28318f) * mat->extraAlphaWaveAmp[ei];
                }
                for (int vi = 0; vi < s->numVerts; vi++) {
                    const drawVert_t *v = &base[vi];
                    float u, vv;
                    if (mat->extraTcGenEnvironment[ei]) {
                        float viewerX = g_camPos[0] - v->xyz[0];
                        float viewerY = g_camPos[1] - v->xyz[1];
                        float viewerZ = g_camPos[2] - v->xyz[2];
                        float invLen = 1.0f / sqrtf(viewerX * viewerX + viewerY * viewerY + viewerZ * viewerZ + 1e-6f);
                        float d, reflectedY, reflectedZ;
                        viewerX *= invLen; viewerY *= invLen; viewerZ *= invLen;
                        d = v->normal[0] * viewerX + v->normal[1] * viewerY + v->normal[2] * viewerZ;
                        reflectedY = v->normal[1] * 2.0f * d - viewerY;
                        reflectedZ = v->normal[2] * 2.0f * d - viewerZ;
                        u = 0.5f + reflectedY * 0.5f;
                        vv = 0.5f - reflectedZ * 0.5f;
                    } else if (mat->extraHasTcGenVector[ei]) {
                        u = v->xyz[0] * mat->extraTcGenVecS[ei][0] + v->xyz[1] * mat->extraTcGenVecS[ei][1] + v->xyz[2] * mat->extraTcGenVecS[ei][2];
                        vv = v->xyz[0] * mat->extraTcGenVecT[ei][0] + v->xyz[1] * mat->extraTcGenVecT[ei][1] + v->xyz[2] * mat->extraTcGenVecT[ei][2];
                    } else {
                        u = v->st[0];
                        vv = v->st[1];
                    }
                    if (mat->extraHasTcScale[ei]) {
                        u *= mat->extraTcScale[ei][0];
                        vv *= mat->extraTcScale[ei][1];
                    }
                    if (mat->extraHasTcRotate[ei]) {
                        float ang = mat->extraTcRotate[ei] * timeSec * (float)M_PI / 180.0f;
                        float c = cosf(ang), s2 = sinf(ang);
                        float ru = (u - 0.5f) * c - (vv - 0.5f) * s2 + 0.5f;
                        float rv = (u - 0.5f) * s2 + (vv - 0.5f) * c + 0.5f;
                        u = ru; vv = rv;
                    }
                    if (mat->extraHasTcTurb[ei]) {
                        float tb = mat->extraTcTurbBase[ei] + sinf(mat->extraTcTurbPhase[ei] + timeSec * mat->extraTcTurbFreq[ei]) * mat->extraTcTurbAmp[ei];
                        u += sinf((vv + timeSec) * 6.28318f) * tb;
                        vv += cosf((u + timeSec) * 6.28318f) * tb;
                    }
                    if (mat->extraHasTcStretch[ei]) {
                        float st = mat->extraTcStretchBase[ei] + sinf((mat->extraTcStretchPhase[ei] + timeSec * mat->extraTcStretchFreq[ei]) * 6.28318f) * mat->extraTcStretchAmp[ei];
                        if (fabsf(st) > 1e-4f) {
                            float p = 1.0f / st;
                            u = (u - 0.5f) * p + 0.5f;
                            vv = (vv - 0.5f) * p + 0.5f;
                        }
                    }
                    if (mat->extraHasTcTransform[ei]) {
                        float tu = u, tv = vv;
                        u = tu * mat->extraTcTransform[ei][0][0] + tv * mat->extraTcTransform[ei][1][0] + mat->extraTcTranslate[ei][0];
                        vv = tu * mat->extraTcTransform[ei][0][1] + tv * mat->extraTcTransform[ei][1][1] + mat->extraTcTranslate[ei][1];
                    }
                    if (mat->extraHasTcSwap[ei]) {
                        float t = u; u = vv; vv = t;
                    }
                    tc[vi * 2 + 0] = u + (mat->extraHasTcScroll[ei] ? mat->extraTcScroll[ei][0] * timeSec : 0.0f);
                    tc[vi * 2 + 1] = vv + (mat->extraHasTcScroll[ei] ? mat->extraTcScroll[ei][1] * timeSec : 0.0f);
                    if (rgba) {
                        rgba[vi * 4 + 0] = mat->extraRgbGenVertex[ei] ? v->color[0] : (unsigned char)(emul[0] * 255.0f);
                        rgba[vi * 4 + 1] = mat->extraRgbGenVertex[ei] ? v->color[1] : (unsigned char)(emul[1] * 255.0f);
                        rgba[vi * 4 + 2] = mat->extraRgbGenVertex[ei] ? v->color[2] : (unsigned char)(emul[2] * 255.0f);
                        rgba[vi * 4 + 3] = mat->extraAlphaGenVertex[ei] ? v->color[3] : (unsigned char)(ealpha * 255.0f);
                    }
                }
                if (mat->extraHasBlend[ei]) {
                    glEnable(GL_BLEND);
                    glBlendFunc(mat->extraStageBlendSrc[ei], mat->extraStageBlendDst[ei]);
                }
                if (mat->extraHasAlphaFunc[ei]) {
                    glEnable(GL_ALPHA_TEST);
                    if (mat->extraAlphaFuncMode[ei] == ALPHA_FUNC_LT128) glAlphaFunc(GL_LESS, mat->extraAlphaRef[ei]);
                    else if (mat->extraAlphaFuncMode[ei] == ALPHA_FUNC_GE128) glAlphaFunc(GL_GEQUAL, mat->extraAlphaRef[ei]);
                    else glAlphaFunc(GL_GREATER, mat->extraAlphaRef[ei]);
                }
                if (mat->extraDepthFuncEqual[ei]) glDepthFunc(GL_EQUAL);
                if (mat->extraDepthWrite[ei]) glDepthMask(GL_TRUE);
                else if (mat->extraHasBlend[ei]) glDepthMask(GL_FALSE);
                glBindTexture(GL_TEXTURE_2D, ext);
                glTexCoordPointer(2, GL_FLOAT, 0, tc);
                if (rgba) {
                    glEnableClientState(GL_COLOR_ARRAY);
                    glColorPointer(4, GL_UNSIGNED_BYTE, 0, rgba);
                } else {
                    glDisableClientState(GL_COLOR_ARRAY);
                    glColor4f(emul[0], emul[1], emul[2], ealpha);
                }
                glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, idx);
                if (mat->extraHasAlphaFunc[ei]) glDisable(GL_ALPHA_TEST);
                if (mat->extraDepthFuncEqual[ei]) glDepthFunc(GL_LEQUAL);
                if (mat->extraDepthWrite[ei] || mat->extraHasBlend[ei]) glDepthMask(GL_TRUE);
                if (mat->extraHasBlend[ei]) glDisable(GL_BLEND);
            }
            glBindTexture(GL_TEXTURE_2D, GetSurfaceTexture(bsp, s->shaderNum, whiteTex, timeSec));
        }
        if (!lightmapPass && mat && baseTc && mat->hasAlphaFunc && !mat->hasBlend) {
            glDisable(GL_BLEND);
            glBindTexture(GL_TEXTURE_2D, GetSurfaceTexture(bsp, s->shaderNum, whiteTex, timeSec));
            glTexCoordPointer(2, GL_FLOAT, 0, baseTc);
            if (rgba) {
                glEnableClientState(GL_COLOR_ARRAY);
                glColorPointer(4, GL_UNSIGNED_BYTE, 0, rgba);
            } else {
                glDisableClientState(GL_COLOR_ARRAY);
                glColor4f(colorMul[0], colorMul[1], colorMul[2], alphaMul);
            }
            glEnable(GL_ALPHA_TEST);
            if (mat->alphaFuncMode == ALPHA_FUNC_LT128) glAlphaFunc(GL_LESS, mat->alphaRef);
            else if (mat->alphaFuncMode == ALPHA_FUNC_GE128) glAlphaFunc(GL_GEQUAL, mat->alphaRef);
            else glAlphaFunc(GL_GREATER, mat->alphaRef);
            glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, idx);
            glDisable(GL_ALPHA_TEST);
        }
draw_done:
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        if (!lightmapPass && rgba) glDisableClientState(GL_COLOR_ARRAY);
        if (lightmapPass) glDisable(GL_BLEND);
        if (!lightmapPass && mat && mat->cullNone) glEnable(GL_CULL_FACE);
        free(tc);
        free(pos);
        free(baseTc);
        free(rgba);
        if (!lightmapPass && mat && mat->hasBlend) glDisable(GL_BLEND);
        if (!lightmapPass && mat && mat->hasAlphaFunc) glDisable(GL_ALPHA_TEST);
        if (!lightmapPass && mat && mat->depthFuncEqual && !IsAlphaBlendMaterial(mat)) glDepthFunc(GL_LEQUAL);
        if (!lightmapPass && IsAlphaBlendMaterial(mat)) glDepthMask(GL_TRUE);
    }
}

int main(int argc, char **argv) {
    SDL_Window *window = NULL;
    SDL_GLContext glctx = NULL;
    bsp_data_t bsp;
    int running = 1;
    int wireframe = 0;
    int patchOnly = 0;
    int noPatch = 0;
    uint64_t prevTicks;
    GLuint whiteTex = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path/to/map.bsp>\n", argv[0]);
        return 1;
    }

    if (!LoadBSP(argv[1], &bsp)) {
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        FreeBSP(&bsp);
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    window = SDL_CreateWindow("RTCW BSP Viewer (OpenGL)",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        FreeBSP(&bsp);
        return 1;
    }
    glctx = SDL_GL_CreateContext(window);
    if (!glctx) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        FreeBSP(&bsp);
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_LIGHTING);
    UploadLightmaps(&bsp);

    {
        uint8_t white[3] = {255, 255, 255};
        glGenTextures(1, &whiteTex);
        glBindTexture(GL_TEXTURE_2D, whiteTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    PreloadMaterialTextures(&bsp, whiteTex);

    printf("Loaded BSP: %s\n", argv[1]);
    printf("Surfaces: %d, Verts: %d, Indices: %d\n", bsp.numSurfaces, bsp.numVerts, bsp.numIndices);
    printf("Controls: WASD+QE move, mouse look, F1 wireframe, F2 patch-only, F3 hide-patch, ESC quit\n");

    prevTicks = SDL_GetPerformanceCounter();
    while (running) {
        SDL_Event ev;
        uint64_t now = SDL_GetPerformanceCounter();
        float timeSec = (float)(SDL_GetTicks()) * 0.001f;
        float dt = (float)(now - prevTicks) / (float)SDL_GetPerformanceFrequency();
        float speed = 600.0f * dt;
        const uint8_t *keys = SDL_GetKeyboardState(NULL);
        float yawRad = g_yaw * (float)M_PI / 180.0f;
        float fwd[3] = { -sinf(yawRad), cosf(yawRad), 0.0f };
        float right[3] = { cosf(yawRad), sinf(yawRad), 0.0f };
        int w, h;

        prevTicks = now;

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) running = 0;
                if (ev.key.keysym.sym == SDLK_F1) wireframe = !wireframe;
                if (ev.key.keysym.sym == SDLK_F2) patchOnly = !patchOnly;
                if (ev.key.keysym.sym == SDLK_F3) noPatch = !noPatch;
            }
            if (ev.type == SDL_MOUSEMOTION) {
                g_yaw -= ev.motion.xrel * 0.12f;
                g_pitch -= ev.motion.yrel * 0.12f;
                if (g_pitch > 89.0f) g_pitch = 89.0f;
                if (g_pitch < -89.0f) g_pitch = -89.0f;
            }
        }

        if (keys[SDL_SCANCODE_W]) {
            g_camPos[0] += fwd[0] * speed; g_camPos[1] += fwd[1] * speed;
        }
        if (keys[SDL_SCANCODE_S]) {
            g_camPos[0] -= fwd[0] * speed; g_camPos[1] -= fwd[1] * speed;
        }
        if (keys[SDL_SCANCODE_A]) {
            g_camPos[0] -= right[0] * speed; g_camPos[1] -= right[1] * speed;
        }
        if (keys[SDL_SCANCODE_D]) {
            g_camPos[0] += right[0] * speed; g_camPos[1] += right[1] * speed;
        }
        if (keys[SDL_SCANCODE_Q]) g_camPos[2] += speed;
        if (keys[SDL_SCANCODE_E]) g_camPos[2] -= speed;

        SDL_GetWindowSize(window, &w, &h);
        if (h < 1) h = 1;
        glViewport(0, 0, w, h);
        SetProjection(75.0f, (float)w / (float)h, 4.0f, 65536.0f);
        ApplyCamera();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

        /* Opaque pass first. */
        for (int i = 0; i < bsp.numSurfaces; i++) {
            const dsurface_t *s = &bsp.surfaces[i];
            if (IsSurfaceDecal(&bsp, s)) continue;
            if (IsSurfaceTransparent(&bsp, s)) continue;
            DrawSurfaceTriangles(&bsp, i, s, patchOnly, noPatch, whiteTex, 0, timeSec);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_EQUAL);
        /* Lightmap only for opaque materials. */
        for (int i = 0; i < bsp.numSurfaces; i++) {
            const dsurface_t *s = &bsp.surfaces[i];
            if (IsSurfaceDecal(&bsp, s)) continue;
            if (IsSurfaceTransparent(&bsp, s)) continue;
            DrawSurfaceTriangles(&bsp, i, s, patchOnly, noPatch, whiteTex, 1, timeSec);
        }
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        /* Decal pass with polygon offset to avoid z-fighting. */
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -2.0f);
        glDepthMask(GL_FALSE);
        {
            int tn = 0;
            for (int i = 0; i < bsp.numSurfaces; i++) {
                const dsurface_t *s = &bsp.surfaces[i];
                float dx, dy, dz;
                if (!IsSurfaceDecal(&bsp, s)) continue;
                dx = bsp.surfaceCenters[i][0] - g_camPos[0];
                dy = bsp.surfaceCenters[i][1] - g_camPos[1];
                dz = bsp.surfaceCenters[i][2] - g_camPos[2];
                bsp.transparentSurfaceOrder[tn] = i;
                bsp.transparentSurfaceDist2[i] = dx * dx + dy * dy + dz * dz;
                tn++;
            }
            g_sortDistRef = bsp.transparentSurfaceDist2;
            qsort(bsp.transparentSurfaceOrder, (size_t)tn, sizeof(int), CompareSurfaceBackToFront);
            for (int i = 0; i < tn; i++) {
                int si = bsp.transparentSurfaceOrder[i];
                DrawSurfaceTriangles(&bsp, si, &bsp.surfaces[si], patchOnly, noPatch, whiteTex, 0, timeSec);
            }
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_POLYGON_OFFSET_FILL);

        /* Transparent pass sorted back-to-front for glass-like surfaces. */
        {
            int tn = 0;
            for (int i = 0; i < bsp.numSurfaces; i++) {
                const dsurface_t *s = &bsp.surfaces[i];
                float dx, dy, dz;
                if (IsSurfaceDecal(&bsp, s)) continue;
                if (!IsSurfaceTransparent(&bsp, s)) continue;
                dx = bsp.surfaceCenters[i][0] - g_camPos[0];
                dy = bsp.surfaceCenters[i][1] - g_camPos[1];
                dz = bsp.surfaceCenters[i][2] - g_camPos[2];
                bsp.transparentSurfaceOrder[tn] = i;
                bsp.transparentSurfaceDist2[i] = dx * dx + dy * dy + dz * dz;
                tn++;
            }
            g_sortDistRef = bsp.transparentSurfaceDist2;
            qsort(bsp.transparentSurfaceOrder, (size_t)tn, sizeof(int), CompareSurfaceBackToFront);
            for (int i = 0; i < tn; i++) {
                int si = bsp.transparentSurfaceOrder[i];
                DrawSurfaceTriangles(&bsp, si, &bsp.surfaces[si], patchOnly, noPatch, whiteTex, 0, timeSec);
            }
        }

        SDL_GL_SwapWindow(window);
    }

    if (bsp.shaderTextures) {
        for (int i = 0; i < bsp.numShaders; i++) {
            if (bsp.shaderTextures[i] != 0 && bsp.shaderTextures[i] != whiteTex) {
                glDeleteTextures(1, &bsp.shaderTextures[i]);
            }
        }
    }
    if (bsp.lightmapTextures) {
        for (int i = 0; i < bsp.numLightmaps; i++) {
            if (bsp.lightmapTextures[i] != 0) {
                glDeleteTextures(1, &bsp.lightmapTextures[i]);
            }
        }
    }
    if (whiteTex) {
        glDeleteTextures(1, &whiteTex);
    }
    SDL_GL_DeleteContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    FreeBSP(&bsp);
    return 0;
}

#include "bsp_viewer_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <math.h>

#ifdef MATERIAL_VIEWER_VK
static char *DupString(const char *s) {
    size_t n;
    char *d;
    if (!s) return NULL;
    n = strlen(s);
    d = (char *)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

static int ParseAnimMapFromShaderFile(const char *shaderFile, const char *materialName, char ***outFrames, int *outCount, float *outFps) {
    FILE *f = fopen(shaderFile, "rb");
    char line[2048];
    char pending[512] = {0};
    char current[512] = {0};
    int inShader = 0, inStage = 0;
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        char *comment = strstr(line, "//");
        char *tok;
        if (comment) *comment = '\0';
        tok = line;
        while (*tok == ' ' || *tok == '\t' || *tok == '\r' || *tok == '\n') tok++;
        if (!*tok) continue;
        {
            size_t L = strlen(tok);
            while (L > 0 && (tok[L - 1] == '\n' || tok[L - 1] == '\r' || tok[L - 1] == ' ' || tok[L - 1] == '\t')) {
                tok[--L] = '\0';
            }
        }
        if (!inShader) {
            if (strchr(tok, '{') && pending[0]) {
                strncpy(current, pending, sizeof(current) - 1);
                current[sizeof(current) - 1] = '\0';
                inShader = 1;
                inStage = 0;
                continue;
            }
            if (!strchr(tok, '{') && !strchr(tok, '}')) {
                strncpy(pending, tok, sizeof(pending) - 1);
                pending[sizeof(pending) - 1] = '\0';
            }
            continue;
        }
        if (strcmp(tok, "{") == 0) { inStage = 1; continue; }
        if (strcmp(tok, "}") == 0 && inStage) { inStage = 0; continue; }
        if (strcmp(tok, "}") == 0 && !inStage) { inShader = 0; current[0] = '\0'; continue; }
        if (!inStage) continue;
        if (strcasecmp(current, materialName) != 0) continue;
        {
            char *save = NULL;
            char *w = strtok_r(tok, " \t", &save);
            if (w && strcasecmp(w, "animMap") == 0) {
                char *fpsTok = strtok_r(NULL, " \t", &save);
                int count = 0;
                char **frames = NULL;
                if (fpsTok) *outFps = (float)atof(fpsTok);
                while (1) {
                    char *fr = strtok_r(NULL, " \t", &save);
                    char **nf;
                    if (!fr) break;
                    nf = (char **)realloc(frames, (size_t)(count + 1) * sizeof(char *));
                    if (!nf) break;
                    frames = nf;
                    frames[count] = DupString(fr);
                    if (!frames[count]) break;
                    count++;
                }
                if (count > 0) {
                    *outFrames = frames;
                    *outCount = count;
                    fclose(f);
                    return 1;
                }
                if (frames) {
                    for (int i = 0; i < count; i++) free(frames[i]);
                    free(frames);
                }
            }
        }
    }
    fclose(f);
    return 0;
}
#endif /* MATERIAL_VIEWER_VK */

static void NormalizeAssetPath(const char *in, char *out, size_t outSize) {
    size_t i = 0;
    if (!out || outSize == 0) return;
    out[0] = '\0';
    if (!in) return;
    while (*in == ' ' || *in == '\t' || *in == '\r' || *in == '\n') in++;
    for (; in[i] && i + 1 < outSize; i++) {
        char c = in[i];
        if (c == '\\') c = '/';
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[i] = '\0';
    while (i > 0 && (out[i - 1] == ' ' || out[i - 1] == '\t' || out[i - 1] == '\r' || out[i - 1] == '\n')) {
        out[--i] = '\0';
    }
}

static int WaveModeFromName(const char *name) {
    if (!name) return 0;
    if (strcasecmp(name, "sin") == 0) return 1;
    if (strcasecmp(name, "sawtooth") == 0) return 2;
    if (strcasecmp(name, "inversesawtooth") == 0) return 3;
    if (strcasecmp(name, "square") == 0) return 4;
    if (strcasecmp(name, "triangle") == 0) return 5;
    return 0;
}

static void ResetMaterialStageAnim(material_stage_anim_t *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->scaleU = 1.0f;
    s->scaleV = 1.0f;
    s->turbFreq = 1.0f;
    s->transform[0] = 1.0f;
    s->transform[1] = 0.0f;
    s->transform[2] = 0.0f;
    s->transform[3] = 1.0f;
    s->transform[4] = 0.0f;
    s->transform[5] = 0.0f;
    s->rgbGenMode = GEN_IDENTITY;
    s->rgbConst[0] = 1.0f;
    s->rgbConst[1] = 1.0f;
    s->rgbConst[2] = 1.0f;
    s->rgbWaveBase = 1.0f;
    s->rgbWaveFreq = 1.0f;
    s->alphaGenMode = GEN_IDENTITY;
    s->alphaConst = 1.0f;
    s->alphaWaveBase = 1.0f;
    s->alphaWaveFreq = 1.0f;
    s->blendMode = STAGE_BLEND_OPAQUE;
    s->alphaFuncMode = ALPHA_FUNC_NONE;
    s->alphaFuncRef = 0.5f;
    s->depthFunc = DEPTH_FUNC_LEQUAL;
    s->depthWrite = 1;
    s->polygonOffset = 0;
    s->tcGenEnvironment = 0;
    s->hasAnimMap = 0;
    s->animMapFps = 0;
    s->animMapFrameCount = 0;
    s->tcModCount = 0;
}

static int ParseMaterialStagesFromShaderFile(app_t *a, const char *shaderFile, const char *materialName, shader_material_t *sm) {
    FILE *f = fopen(shaderFile, "rb");
    char line[2048];
    char pending[512] = {0};
    char current[512] = {0};
    char materialNorm[512] = {0};
    char currentNorm[512] = {0};
    char shaderEditorImage[512] = {0};
    int inShader = 0, inStage = 0;
    int captureShader = 0;
    int stageIndex = -1;
    if (!a || !sm || !f) return 0;
    NormalizeAssetPath(materialName, materialNorm, sizeof(materialNorm));
    while (fgets(line, sizeof(line), f)) {
        char *comment = strstr(line, "//");
        char *tok;
        if (comment) *comment = '\0';
        tok = line;
        while (*tok == ' ' || *tok == '\t' || *tok == '\r' || *tok == '\n') tok++;
        if (!*tok) continue;
        {
            size_t L = strlen(tok);
            while (L > 0 && (tok[L - 1] == '\n' || tok[L - 1] == '\r' || tok[L - 1] == ' ' || tok[L - 1] == '\t')) tok[--L] = '\0';
        }
        if (!inShader) {
            if (strchr(tok, '{') && pending[0]) {
                strncpy(current, pending, sizeof(current) - 1);
                current[sizeof(current) - 1] = '\0';
                NormalizeAssetPath(current, currentNorm, sizeof(currentNorm));
                shaderEditorImage[0] = '\0';
                inShader = 1;
                inStage = 0;
                if (strcasecmp(currentNorm, materialNorm) == 0 && sm->stageCount == 0) {
                    captureShader = 1;
                } else {
                    captureShader = 0;
                }
                stageIndex = -1;
                continue;
            }
            if (!strchr(tok, '{') && !strchr(tok, '}')) {
                strncpy(pending, tok, sizeof(pending) - 1);
                pending[sizeof(pending) - 1] = '\0';
            }
            continue;
        }
        if (strcmp(tok, "{") == 0) {
            inStage = 1;
            if (captureShader && sm->stageCount < MAX_MATERIAL_STAGES) {
                stageIndex = (int)sm->stageCount;
                ResetMaterialStageAnim(&sm->stages[stageIndex]);
                sm->stageCount++;
            } else {
                stageIndex = -1;
            }
            continue;
        }
        if (strcmp(tok, "}") == 0 && inStage) {
            inStage = 0;
            stageIndex = -1;
            continue;
        }
        if (strcmp(tok, "}") == 0 && !inStage) {
            if (captureShader && sm->stageCount == 0 && shaderEditorImage[0] && sm->stageCount < MAX_MATERIAL_STAGES) {
                ResetMaterialStageAnim(&sm->stages[0]);
                strncpy(sm->stages[0].texturePath, shaderEditorImage, sizeof(sm->stages[0].texturePath) - 1);
                sm->stages[0].texturePath[sizeof(sm->stages[0].texturePath) - 1] = '\0';
                sm->stages[0].hasTexture = 1;
                sm->stageCount = 1;
            }
            /* Apply polygonOffset to all stages if shader has it */
            if (captureShader && sm->polygonOffset && sm->stageCount > 0) {
                uint32_t i;
                for (i = 0; i < sm->stageCount; i++) {
                    sm->stages[i].polygonOffset = 1;
                }
            }
            inShader = 0;
            captureShader = 0;
            current[0] = '\0';
            continue;
        }
        if (captureShader && !inStage) {
            char *save = NULL;
            char *w = strtok_r(tok, " \t", &save);
            if (!w) continue;
            if (strcasecmp(w, "qer_editorimage") == 0) {
                char *arg = strtok_r(NULL, " \t", &save);
                if (arg && arg[0] != '$') {
                    NormalizeAssetPath(arg, shaderEditorImage, sizeof(shaderEditorImage));
                }
            } else if (strcasecmp(w, "cull") == 0) {
                char *arg = strtok_r(NULL, " \t", &save);
                if (arg) {
                    if (strcasecmp(arg, "none") == 0 || strcasecmp(arg, "disable") == 0) {
                        sm->cullMode = CULL_NONE;
                    } else if (strcasecmp(arg, "twosided") == 0) {
                        sm->cullMode = CULL_TWOSIDED;
                    } else {
                        sm->cullMode = CULL_BACK;
                    }
                }
            } else if (strcasecmp(w, "surfaceparm") == 0) {
                char *arg = strtok_r(NULL, " \t", &save);
                if (arg) {
                    if (strcasecmp(arg, "alphashadow") == 0) sm->surfaceParm |= SURFPARM_ALPHASHADOW;
                    else if (strcasecmp(arg, "nomarks") == 0) sm->surfaceParm |= SURFPARM_NOMARKS;
                    else if (strcasecmp(arg, "nonsolid") == 0) sm->surfaceParm |= SURFPARM_NONSOLID;
                    else if (strcasecmp(arg, "metalsteps") == 0) sm->surfaceParm |= SURFPARM_METALSTEPS;
                    else if (strcasecmp(arg, "roofsteps") == 0) sm->surfaceParm |= SURFPARM_ROOFSTEPS;
                    else if (strcasecmp(arg, "grasssteps") == 0) sm->surfaceParm |= SURFPARM_GRASSSTEPS;
                    else if (strcasecmp(arg, "nolightmap") == 0) sm->surfaceParm |= SURFPARM_NOLIGHTMAP;
                    else if (strcasecmp(arg, "trans") == 0) sm->surfaceParm |= SURFPARM_TRANS;
                    else if (strcasecmp(arg, "slick") == 0) sm->surfaceParm |= SURFPARM_SLICK;
                    else if (strcasecmp(arg, "water") == 0) sm->surfaceParm |= SURFPARM_WATER;
                    else if (strcasecmp(arg, "lava") == 0) sm->surfaceParm |= SURFPARM_LAVA;
                }
            } else if (strcasecmp(w, "deformvertexes") == 0 || strcasecmp(w, "deformVertexes") == 0) {
                char *type = strtok_r(NULL, " \t", &save);
                if (type && strcasecmp(type, "wave") == 0) {
                    char *div = strtok_r(NULL, " \t", &save);
                    char *func = strtok_r(NULL, " \t", &save);
                    char *base = strtok_r(NULL, " \t", &save);
                    char *amp = strtok_r(NULL, " \t", &save);
                    char *phase = strtok_r(NULL, " \t", &save);
                    char *freq = strtok_r(NULL, " \t", &save);
                    if (div && func && base && amp && phase && freq) {
                        sm->hasDeformVertexes = 1;
                        sm->deformWaveDiv = atoi(div);
                        sm->deformWaveMode = WaveModeFromName(func);
                        sm->deformWaveBase = (float)atof(base);
                        sm->deformWaveAmp = (float)atof(amp);
                        sm->deformWavePhase = (float)atof(phase);
                        sm->deformWaveFreq = (float)atof(freq);
                    }
                }
            } else if (strcasecmp(w, "polygonOffset") == 0 || strcasecmp(w, "polygonoffset") == 0) {
                sm->polygonOffset = 1;
                sm->polygonOffsetFactor = -1.0f;  /* Typical decal offset */
                sm->polygonOffsetUnits = -2.0f;
            } else if (strcasecmp(w, "portal") == 0) {
                sm->isPortal = 1;
            } else if (strcasecmp(w, "nomipmaps") == 0) {
                if (sm->stageCount > 0) sm->stages[sm->stageCount - 1].noMipMaps = 1;
            } else if (strcasecmp(w, "nopicmip") == 0) {
                if (sm->stageCount > 0) sm->stages[sm->stageCount - 1].noPicMip = 1;
            }
            continue;
        }
        if (!inStage || stageIndex < 0) continue;
        if (strcasecmp(currentNorm, materialNorm) != 0) continue;
        {
            material_stage_anim_t *stage = &sm->stages[stageIndex];
            char *save = NULL;
            char *w = strtok_r(tok, " \t", &save);
            if (!w) continue;
            if (strcasecmp(w, "map") == 0 || strcasecmp(w, "clampmap") == 0) {
                char *arg = strtok_r(NULL, " \t", &save);
                if (arg) {
                    if (strcasecmp(arg, "$lightmap") == 0) {
                        stage->useLightmap = 1;
                    } else if (arg[0] != '$') {
                        NormalizeAssetPath(arg, stage->texturePath, sizeof(stage->texturePath));
                        stage->hasTexture = 1;
                    }
                }
            } else if (strcasecmp(w, "blendfunc") == 0 || strcasecmp(w, "blendFunc") == 0) {
                char *a0 = strtok_r(NULL, " \t", &save);
                if (a0 && strcasecmp(a0, "blend") == 0) {
                    stage->blendMode = STAGE_BLEND_ALPHA;
                } else if (a0 && strcasecmp(a0, "add") == 0) {
                    stage->blendMode = STAGE_BLEND_ONE_ONE;
                } else if (a0 && strcasecmp(a0, "filter") == 0) {
                    stage->blendMode = STAGE_BLEND_DSTCOLOR_ZERO;
                } else {
                    char *a1 = strtok_r(NULL, " \t", &save);
                    if (a0 && a1) {
                        if (strcasecmp(a0, "GL_DST_COLOR") == 0 && strcasecmp(a1, "GL_ONE_MINUS_DST_ALPHA") == 0) {
                            stage->blendMode = STAGE_BLEND_DSTCOLOR_ONEMINUSDSTALPHA;
                        } else if (strcasecmp(a0, "GL_ONE") == 0 && strcasecmp(a1, "GL_ONE") == 0) {
                            stage->blendMode = STAGE_BLEND_ONE_ONE;
                        } else if (strcasecmp(a0, "GL_ZERO") == 0 && strcasecmp(a1, "GL_ONE_MINUS_SRC_COLOR") == 0) {
                            stage->blendMode = STAGE_BLEND_ZERO_ONEMINUSSRCCOLOR;
                        } else if (strcasecmp(a0, "GL_SRC_ALPHA") == 0 && strcasecmp(a1, "GL_ONE") == 0) {
                            stage->blendMode = STAGE_BLEND_SRCALPHA_ONE;
                        } else if (strcasecmp(a0, "GL_ONE_MINUS_SRC_ALPHA") == 0 && strcasecmp(a1, "GL_SRC_ALPHA") == 0) {
                            stage->blendMode = STAGE_BLEND_ONEMINUSSRCALPHA_SRCALPHA;
                        } else if (strcasecmp(a0, "GL_DST_COLOR") == 0 && strcasecmp(a1, "GL_ZERO") == 0) {
                            stage->blendMode = STAGE_BLEND_DSTCOLOR_ZERO;
                        } else if (strcasecmp(a0, "GL_ONE") == 0 && strcasecmp(a1, "GL_ZERO") == 0) {
                            stage->blendMode = STAGE_BLEND_OPAQUE;
                        } else if (strcasecmp(a0, "GL_SRC_ALPHA") == 0 && strcasecmp(a1, "GL_ONE_MINUS_SRC_ALPHA") == 0) {
                            stage->blendMode = STAGE_BLEND_SRCALPHA_ONEMINUSSRCALPHA;
                        } else if (strcasecmp(a0, "GL_DST_COLOR") == 0 && strcasecmp(a1, "GL_ONE_MINUS_SRC_COLOR") == 0) {
                            stage->blendMode = STAGE_BLEND_DSTCOLOR_ONEMINUSSRCCOLOR;
                        } else if (strcasecmp(a0, "GL_ONE_MINUS_DST_ALPHA") == 0 && strcasecmp(a1, "GL_ONE") == 0) {
                            stage->blendMode = STAGE_BLEND_ONEMINUSDSTALPHA_ONE;
                        } else if (strcasecmp(a0, "GL_ONE") == 0 && strcasecmp(a1, "GL_ONE_MINUS_SRC_ALPHA") == 0) {
                            stage->blendMode = STAGE_BLEND_ONE_ONEMINUSSRCALPHA;
                        } else if (strcasecmp(a0, "GL_ONE_MINUS_DST_ALPHA") == 0 && strcasecmp(a1, "GL_DST_COLOR") == 0) {
                            stage->blendMode = STAGE_BLEND_ONEMINUSDSTALPHA_DSTCOLOR;
                        } else {
                            stage->blendMode = STAGE_BLEND_ALPHA;
                        }
                    }
                }
            } else if (strcasecmp(w, "alphaFunc") == 0) {
                char *mode = strtok_r(NULL, " \t", &save);
                if (mode) {
                    if (strcasecmp(mode, "GT0") == 0) {
                        stage->alphaFuncMode = ALPHA_FUNC_GT0;
                        stage->alphaFuncRef = 0.0f;
                    } else if (strcasecmp(mode, "LT128") == 0) {
                        stage->alphaFuncMode = ALPHA_FUNC_LT128;
                        stage->alphaFuncRef = 128.0f / 255.0f;
                    } else if (strcasecmp(mode, "GE128") == 0) {
                        stage->alphaFuncMode = ALPHA_FUNC_GE128;
                        stage->alphaFuncRef = 128.0f / 255.0f;
                    }
                }
            } else if (strcasecmp(w, "depthFunc") == 0) {
                char *mode = strtok_r(NULL, " \t", &save);
                if (mode && strcasecmp(mode, "equal") == 0) {
                    stage->depthFunc = DEPTH_FUNC_EQUAL;
                }
            } else if (strcasecmp(w, "depthWrite") == 0) {
                stage->depthWrite = 1;
            } else if (strcasecmp(w, "nomipmaps") == 0) {
                stage->noMipMaps = 1;
            } else if (strcasecmp(w, "nopicmip") == 0) {
                stage->noPicMip = 1;
            } else if (strcasecmp(w, "tcGen") == 0) {
                char *arg = strtok_r(NULL, " \t", &save);
                if (arg && strcasecmp(arg, "environment") == 0) {
                    stage->tcGenEnvironment = 1;
                }
            } else if (strcasecmp(w, "animMap") == 0) {
                char *fpsTok = strtok_r(NULL, " \t", &save);
                if (fpsTok) {
                    stage->animMapFps = (int)atof(fpsTok);
                }
                /* Parse up to 16 frame paths */
                stage->animMapFrameCount = 0;
                while (stage->animMapFrameCount < 16) {
                    char *frame = strtok_r(NULL, " \t", &save);
                    if (!frame) break;
                    NormalizeAssetPath(frame, stage->animMapFrames[stage->animMapFrameCount], sizeof(stage->animMapFrames[0]));
                    stage->animMapFrameCount++;
                }
                if (stage->animMapFrameCount > 0) {
                    stage->hasAnimMap = 1;
                    /* Use first frame as initial texture */
                    strncpy(stage->texturePath, stage->animMapFrames[0], sizeof(stage->texturePath) - 1);
                    stage->texturePath[sizeof(stage->texturePath) - 1] = '\0';
                    stage->hasTexture = 1;
                }
            } else if (strcasecmp(w, "cull") == 0) {
                /* cull is shader-level, handled below */
            } else if (strcasecmp(w, "surfaceparm") == 0) {
                /* surfaceparm is shader-level, handled below */
            } else if (strcasecmp(w, "tcMod") == 0) {
                char *mode = strtok_r(NULL, " \t", &save);
                if (mode && strcasecmp(mode, "scroll") == 0) {
                    char *u = strtok_r(NULL, " \t", &save);
                    char *v = strtok_r(NULL, " \t", &save);
                    if (u && v) {
                        stage->scrollU = (float)atof(u);
                        stage->scrollV = (float)atof(v);
                        a->hasUiShaderAnim = 1;
                    }
                } else if (mode && strcasecmp(mode, "rotate") == 0) {
                    char *d = strtok_r(NULL, " \t", &save);
                    if (d) {
                        stage->rotateDegPerSec = (float)atof(d);
                        a->hasUiShaderAnim = 1;
                    }
                } else if (mode && strcasecmp(mode, "scale") == 0) {
                    char *u = strtok_r(NULL, " \t", &save);
                    char *v = strtok_r(NULL, " \t", &save);
                    if (u && v) {
                        stage->scaleU = (float)atof(u);
                        stage->scaleV = (float)atof(v);
                        a->hasUiShaderAnim = 1;
                    }
                } else if (mode && strcasecmp(mode, "turb") == 0) {
                    char *base = strtok_r(NULL, " \t", &save);
                    char *amp = strtok_r(NULL, " \t", &save);
                    char *phase = strtok_r(NULL, " \t", &save);
                    char *freq = strtok_r(NULL, " \t", &save);
                    if (base && amp && phase && freq) {
                        stage->turbBase = (float)atof(base);
                        stage->turbAmp = (float)atof(amp);
                        stage->turbPhase = (float)atof(phase);
                        stage->turbFreq = (float)atof(freq);
                        stage->hasTurb = 1;
                        a->hasUiShaderAnim = 1;
                    }
                } else if (mode && strcasecmp(mode, "stretch") == 0) {
                    /* tcmod stretch - for liquid pulsing effects */
                    /* Parse stretch parameters: stretch <base> <amp> <phase> <freq> or with 'scale' prefix */
                    char *p1 = strtok_r(NULL, " \t", &save);
                    char *p2 = strtok_r(NULL, " \t", &save);
                    char *p3 = strtok_r(NULL, " \t", &save);
                    char *p4 = strtok_r(NULL, " \t", &save);
                    char *p5 = strtok_r(NULL, " \t", &save);
                    if (p1 && p2 && p3 && p4) {
                        int idx = stage->tcModCount;
                        if (idx < MAX_TCMODS_PER_STAGE) {
                            stage->tcModTypes[idx] = TCMod_STRETCH;
                            /* stretch uses either direct values or parses wave-like params */
                            stage->tcModParams[idx][0] = (float)atof(p1); /* base */
                            stage->tcModParams[idx][1] = (float)atof(p2); /* amp */
                            stage->tcModParams[idx][2] = (float)atof(p3); /* phase */
                            stage->tcModParams[idx][3] = (float)atof(p4); /* freq */
                            stage->tcModCount++;
                            a->hasUiShaderAnim = 1;
                        }
                    }
                } else if (mode && strcasecmp(mode, "transform") == 0) {
                    char *m00 = strtok_r(NULL, " \t", &save);
                    char *m01 = strtok_r(NULL, " \t", &save);
                    char *m10 = strtok_r(NULL, " \t", &save);
                    char *m11 = strtok_r(NULL, " \t", &save);
                    char *t0 = strtok_r(NULL, " \t", &save);
                    char *t1 = strtok_r(NULL, " \t", &save);
                    if (m00 && m01 && m10 && m11 && t0 && t1) {
                        stage->transform[0] = (float)atof(m00);
                        stage->transform[1] = (float)atof(m01);
                        stage->transform[2] = (float)atof(m10);
                        stage->transform[3] = (float)atof(m11);
                        stage->transform[4] = (float)atof(t0);
                        stage->transform[5] = (float)atof(t1);
                        stage->hasTransform = 1;
                        a->hasUiShaderAnim = 1;
                    }
                } else if (mode && strcasecmp(mode, "swap") == 0) {
                    stage->hasSwap = 1;
                    a->hasUiShaderAnim = 1;
                }
            } else if (strcasecmp(w, "rgbGen") == 0) {
                char *mode = strtok_r(NULL, " \t", &save);
                if (!mode) continue;
                if (strcasecmp(mode, "identity") == 0) {
                    stage->rgbGenMode = GEN_IDENTITY;
                } else if (strcasecmp(mode, "vertex") == 0) {
                    stage->rgbGenMode = GEN_VERTEX;
                } else if (strcasecmp(mode, "const") == 0) {
                    char *r = strtok_r(NULL, " \t()", &save);
                    char *g = strtok_r(NULL, " \t()", &save);
                    char *b = strtok_r(NULL, " \t()", &save);
                    if (r && g && b) {
                        stage->rgbGenMode = GEN_CONST;
                        stage->rgbConst[0] = (float)atof(r);
                        stage->rgbConst[1] = (float)atof(g);
                        stage->rgbConst[2] = (float)atof(b);
                    }
                } else if (strcasecmp(mode, "wave") == 0) {
                    char *func = strtok_r(NULL, " \t", &save);
                    char *base = strtok_r(NULL, " \t", &save);
                    char *amp = strtok_r(NULL, " \t", &save);
                    char *phase = strtok_r(NULL, " \t", &save);
                    char *freq = strtok_r(NULL, " \t", &save);
                    int wm = WaveModeFromName(func);
                    if (wm && base && amp && phase && freq) {
                        stage->rgbGenMode = GEN_WAVE;
                        stage->rgbWaveMode = wm;
                        stage->rgbWaveBase = (float)atof(base);
                        stage->rgbWaveAmp = (float)atof(amp);
                        stage->rgbWavePhase = (float)atof(phase);
                        stage->rgbWaveFreq = (float)atof(freq);
                        a->hasUiShaderAnim = 1;
                    }
                } else if (strcasecmp(mode, "diffuse") == 0) {
                    stage->rgbGenMode = GEN_DIFFUSE;
                }
            } else if (strcasecmp(w, "alphaGen") == 0) {
                char *mode = strtok_r(NULL, " \t", &save);
                if (!mode) continue;
                if (strcasecmp(mode, "identity") == 0) {
                    stage->alphaGenMode = GEN_IDENTITY;
                } else if (strcasecmp(mode, "vertex") == 0) {
                    stage->alphaGenMode = GEN_VERTEX;
                } else if (strcasecmp(mode, "const") == 0) {
                    char *a0 = strtok_r(NULL, " \t()", &save);
                    if (a0) {
                        stage->alphaGenMode = GEN_CONST;
                        stage->alphaConst = (float)atof(a0);
                    }
                } else if (strcasecmp(mode, "wave") == 0) {
                    char *func = strtok_r(NULL, " \t", &save);
                    char *base = strtok_r(NULL, " \t", &save);
                    char *amp = strtok_r(NULL, " \t", &save);
                    char *phase = strtok_r(NULL, " \t", &save);
                    char *freq = strtok_r(NULL, " \t", &save);
                    int wm = WaveModeFromName(func);
                    if (wm && base && amp && phase && freq) {
                        stage->alphaGenMode = GEN_WAVE;
                        stage->alphaWaveMode = wm;
                        stage->alphaWaveBase = (float)atof(base);
                        stage->alphaWaveAmp = (float)atof(amp);
                        stage->alphaWavePhase = (float)atof(phase);
                        stage->alphaWaveFreq = (float)atof(freq);
                        a->hasUiShaderAnim = 1;
                    }
                } else if (strcasecmp(mode, "lightingSpecular") == 0) {
                    stage->alphaGenMode = GEN_LIGHTING_SPECULAR;
                }
            } else if (strcasecmp(w, "depthWrite") == 0) {
                stage->depthWrite = 1;
            }
        }
    }
    fclose(f);
    return sm->stageCount > 0;
}

/* Mirror bsp_viewer_gl StageScore + primary/secondary/extra draw order (opaque base first, then blend passes). */
static int MaterialStageGlScore(const material_stage_anim_t *st) {
    int hasBlend = (st->blendMode != STAGE_BLEND_OPAQUE);
    if (!hasBlend) return 400;
    if (st->blendMode == STAGE_BLEND_DSTCOLOR_ZERO) return 300;
    if (st->blendMode == STAGE_BLEND_ALPHA) return 200;
    if (st->blendMode == STAGE_BLEND_ONE_ONE) return 100;
    return 150;
}

static int MaterialStageCanUseLightmapTex(const app_t *a) {
    return a && a->lightmapTextures && a->mesh.lightmapCount > 0 && a->lightmapTextures[0].view != VK_NULL_HANDLE;
}

static void ShaderMaterialBuildDrawOrderGlLike(app_t *a, shader_material_t *sm) {
    int n, i;
    int primary, secondary, best, o;
    if (!a || !sm) return;
    n = (int)sm->stageCount;
    sm->drawOrderCount = 0;
    if (n <= 0) return;

    primary = -1;
    best = -99999;
    for (i = 0; i < n; i++) {
        const material_stage_anim_t *st = &sm->stages[i];
        int score = MaterialStageGlScore(st);
        if (st->useLightmap) score -= 500;
        if (score > best) {
            best = score;
            primary = i;
        }
    }
    if (primary < 0) return;

    secondary = -1;
    best = -99999;
    for (i = 0; i < n; i++) {
        const material_stage_anim_t *st = &sm->stages[i];
        int score = MaterialStageGlScore(st);
        if (st->useLightmap) score -= 500;
        if (i == primary) continue;
        if (st->useLightmap) continue;
        /* bsp_viewer_gl only promotes stages with blend or alphaFunc to secondary; we only have blend. */
        if (st->blendMode == STAGE_BLEND_OPAQUE) continue;
        if (score > best) {
            best = score;
            secondary = i;
        }
    }

    o = 0;
    if (primary >= 0) {
        const material_stage_anim_t *stp = &sm->stages[primary];
        if (!(stp->useLightmap && !MaterialStageCanUseLightmapTex(a))) sm->drawOrder[o++] = (uint32_t)primary;
    }
    if (secondary >= 0) {
        const material_stage_anim_t *sts = &sm->stages[secondary];
        if (!(sts->useLightmap && !MaterialStageCanUseLightmapTex(a))) sm->drawOrder[o++] = (uint32_t)secondary;
    }
    for (i = 0; i < n; i++) {
        const material_stage_anim_t *st = &sm->stages[i];
        if (i == primary || i == secondary) continue;
        if (st->useLightmap) continue;
        /* Extra passes in GL omit opaque also-rans. */
        if (st->blendMode == STAGE_BLEND_OPAQUE) continue;
        sm->drawOrder[o++] = (uint32_t)i;
    }
    for (i = 0; i < n; i++) {
        const material_stage_anim_t *st = &sm->stages[i];
        if (i == primary || i == secondary) continue;
        if (!st->useLightmap) continue;
        if (!MaterialStageCanUseLightmapTex(a)) continue;
        sm->drawOrder[o++] = (uint32_t)i;
    }
    sm->drawOrderCount = (uint32_t)o;
    if (sm->drawOrderCount == 0 && sm->stageCount > 0) {
        for (i = 0; i < n; i++) sm->drawOrder[i] = (uint32_t)i;
        sm->drawOrderCount = (uint32_t)n;
    }
}

void ShaderMaterialFinalizeDrawOrder(app_t *a, shader_material_t *sm) {
    if (!a || !sm || sm->stageCount == 0) return;
    ShaderMaterialBuildDrawOrderGlLike(a, sm);
}

static int ParseMaterialStagesFromScripts(app_t *a, const char *materialName, shader_material_t *sm) {
    char scriptsDir[1400];
    DIR *d;
    struct dirent *ent;
    uint32_t sti;
    if (!a || !sm || !materialName || !materialName[0]) return 0;
    snprintf(scriptsDir, sizeof(scriptsDir), "%s/scripts", a->mesh.gameRoot);
    d = opendir(scriptsDir);
    if (!d) return 0;
    while ((ent = readdir(d)) != NULL) {
        char shaderPath[1700];
        size_t n = strlen(ent->d_name);
        if (n < 8) continue;
        if (strcasecmp(ent->d_name + n - 7, ".shader") != 0) continue;
        snprintf(shaderPath, sizeof(shaderPath), "%s/%s", scriptsDir, ent->d_name);
        sm->stageCount = 0;
        sm->drawOrderCount = 0;
        sm->cullMode = CULL_BACK;
        sm->surfaceParm = 0;
        sm->hasEditorImage = 0;
        sm->editorImage[0] = '\0';
        sm->polygonOffset = 0;
        sm->polygonOffsetFactor = 0.0f;
        sm->polygonOffsetUnits = 0.0f;
        sm->hasDeformVertexes = 0;
        sm->deformWaveDiv = 0;
        sm->deformWaveMode = 0;
        sm->deformWaveBase = 0.0f;
        sm->deformWaveAmp = 0.0f;
        sm->deformWavePhase = 0.0f;
        sm->deformWaveFreq = 0.0f;
        sm->isPortal = 0;
        for (sti = 0; sti < MAX_MATERIAL_STAGES; sti++) ResetMaterialStageAnim(&sm->stages[sti]);
        if (ParseMaterialStagesFromShaderFile(a, shaderPath, materialName, sm)) {
            closedir(d);
            return 1;
        }
    }
    closedir(d);
    return 0;
}

void BspLoadAllShaderMaterials(app_t *a) {
    uint32_t maxP = 0;
    uint32_t i, j;
    shader_material_t *sm;
    if (!a || !a->mesh.shaderMaterials || a->mesh.shaderCount == 0) return;
    for (i = 0; i < a->mesh.shaderCount; i++) {
        sm = &a->mesh.shaderMaterials[i];
        sm->stageCount = 0;
        sm->drawOrderCount = 0;
        for (j = 0; j < MAX_MATERIAL_STAGES; j++) ResetMaterialStageAnim(&sm->stages[j]);
        if (!ParseMaterialStagesFromScripts(a, a->mesh.shaders[i].shader, sm)) continue;
        if (sm->stageCount == 0) continue;
        {
            uint32_t loadedTextureStages = 0;
        for (j = 0; j < sm->stageCount; j++) {
            if (sm->stages[j].hasTexture && sm->stages[j].texturePath[0]) {
                if (!TryLoadTextureByPath(a, sm->stages[j].texturePath, &sm->stages[j].texture)) {
                    fprintf(stderr, "[bsp_viewer_vk] material stage texture load failed: shader=%s stage=%u texture=%s\n",
                            a->mesh.shaders[i].shader, j, sm->stages[j].texturePath);
                    fflush(stderr);
                    sm->stages[j].hasTexture = 0;
                } else {
                    loadedTextureStages++;
                }
            }
            /* Load animMap frames if present */
            if (sm->stages[j].hasAnimMap && sm->stages[j].animMapFrameCount > 0) {
                int fi;
                for (fi = 0; fi < sm->stages[j].animMapFrameCount; fi++) {
                    /* Store animMap textures in unused texture slots - simplified approach */
                    /* For now, just verify they exist */
                    vk_texture_t tempTex;
                    memset(&tempTex, 0, sizeof(tempTex));
                    if (TryLoadTextureByPath(a, sm->stages[j].animMapFrames[fi], &tempTex)) {
                        DestroyTexture(a, &tempTex); /* Just testing for now */
                    }
                }
            }
        }
            if (loadedTextureStages == 0) {
                fprintf(stderr, "[bsp_viewer_vk] material %s has no loaded texture stages; using legacy shader texture fallback\n",
                        a->mesh.shaders[i].shader);
                fflush(stderr);
                sm->stageCount = 0;
                sm->drawOrderCount = 0;
                for (j = 0; j < MAX_MATERIAL_STAGES; j++) ResetMaterialStageAnim(&sm->stages[j]);
                continue;
            }
            {
                uint32_t writeIndex = 0;
                for (j = 0; j < sm->stageCount; j++) {
                    if (!sm->stages[j].useLightmap && !sm->stages[j].hasTexture) continue;
                    if (writeIndex != j) sm->stages[writeIndex] = sm->stages[j];
                    writeIndex++;
                }
                for (j = writeIndex; j < MAX_MATERIAL_STAGES; j++) ResetMaterialStageAnim(&sm->stages[j]);
                sm->stageCount = writeIndex;
            }
        }
        ShaderMaterialFinalizeDrawOrder(a, sm);
        if (sm->drawOrderCount > maxP) maxP = sm->drawOrderCount;
    }
    a->mesh.materialMaxPasses = maxP;
}

#ifdef MATERIAL_VIEWER_VK
static int ParseUiAnimFromShaderFile(app_t *a, const char *shaderFile, const char *materialName) {
    FILE *f = fopen(shaderFile, "rb");
    char line[2048];
    char pending[512] = {0};
    char current[512] = {0};
    int inShader = 0, inStage = 0;
    if (!a || !f) return 0;
    while (fgets(line, sizeof(line), f)) {
        char *comment = strstr(line, "//");
        char *tok;
        if (comment) *comment = '\0';
        tok = line;
        while (*tok == ' ' || *tok == '\t' || *tok == '\r' || *tok == '\n') tok++;
        if (!*tok) continue;
        {
            size_t L = strlen(tok);
            while (L > 0 && (tok[L - 1] == '\n' || tok[L - 1] == '\r' || tok[L - 1] == ' ' || tok[L - 1] == '\t')) tok[--L] = '\0';
        }
        if (!inShader) {
            if (strchr(tok, '{') && pending[0]) {
                strncpy(current, pending, sizeof(current) - 1);
                current[sizeof(current) - 1] = '\0';
                inShader = 1;
                inStage = 0;
                continue;
            }
            if (!strchr(tok, '{') && !strchr(tok, '}')) {
                strncpy(pending, tok, sizeof(pending) - 1);
                pending[sizeof(pending) - 1] = '\0';
            }
            continue;
        }
        if (strcmp(tok, "{") == 0) { inStage = 1; continue; }
        if (strcmp(tok, "}") == 0 && inStage) { inStage = 0; continue; }
        if (strcmp(tok, "}") == 0 && !inStage) { inShader = 0; current[0] = '\0'; continue; }
        if (!inStage) continue;
        if (strcasecmp(current, materialName) != 0) continue;
        {
            char *save = NULL;
            char *w = strtok_r(tok, " \t", &save);
            if (!w) continue;
            if (strcasecmp(w, "tcMod") == 0) {
                char *mode = strtok_r(NULL, " \t", &save);
                if (mode && strcasecmp(mode, "scroll") == 0) {
                    char *u = strtok_r(NULL, " \t", &save);
                    char *v = strtok_r(NULL, " \t", &save);
                    if (u && v) { a->animScrollU = (float)atof(u); a->animScrollV = (float)atof(v); a->hasUiShaderAnim = 1; }
                } else if (mode && strcasecmp(mode, "rotate") == 0) {
                    char *d = strtok_r(NULL, " \t", &save);
                    if (d) { a->animRotateDegPerSec = (float)atof(d); a->hasUiShaderAnim = 1; }
                } else if (mode && strcasecmp(mode, "scale") == 0) {
                    char *u = strtok_r(NULL, " \t", &save);
                    char *v = strtok_r(NULL, " \t", &save);
                    if (u && v) { a->animScaleU = (float)atof(u); a->animScaleV = (float)atof(v); a->hasUiShaderAnim = 1; }
                }
            } else if (strcasecmp(w, "rgbGen") == 0) {
                char *mode = strtok_r(NULL, " \t", &save);
                if (mode && strcasecmp(mode, "wave") == 0) {
                    char *func = strtok_r(NULL, " \t", &save);
                    char *base = strtok_r(NULL, " \t", &save);
                    char *amp = strtok_r(NULL, " \t", &save);
                    char *phase = strtok_r(NULL, " \t", &save);
                    char *freq = strtok_r(NULL, " \t", &save);
                    int wm = WaveModeFromName(func);
                    if (wm && base && amp && phase && freq) {
                        a->rgbWaveMode = wm;
                        a->rgbWaveBase = (float)atof(base);
                        a->rgbWaveAmp = (float)atof(amp);
                        a->rgbWavePhase = (float)atof(phase);
                        a->rgbWaveFreq = (float)atof(freq);
                        a->hasUiShaderAnim = 1;
                    }
                }
            }
        }
    }
    fclose(f);
    return a->hasUiShaderAnim;
}

static int FindMaterialByTextureInShaderFile(const char *shaderFile, const char *texturePath, char *outMaterial, size_t outSize) {
    FILE *f = fopen(shaderFile, "rb");
    char line[2048];
    char pending[512] = {0};
    char current[512] = {0};
    int inShader = 0, inStage = 0;
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        char *comment = strstr(line, "//");
        char *tok;
        if (comment) *comment = '\0';
        tok = line;
        while (*tok == ' ' || *tok == '\t' || *tok == '\r' || *tok == '\n') tok++;
        if (!*tok) continue;
        {
            size_t L = strlen(tok);
            while (L > 0 && (tok[L - 1] == '\n' || tok[L - 1] == '\r' || tok[L - 1] == ' ' || tok[L - 1] == '\t')) {
                tok[--L] = '\0';
            }
        }
        if (!inShader) {
            if (strchr(tok, '{') && pending[0]) {
                strncpy(current, pending, sizeof(current) - 1);
                current[sizeof(current) - 1] = '\0';
                inShader = 1;
                inStage = 0;
                continue;
            }
            if (!strchr(tok, '{') && !strchr(tok, '}')) {
                strncpy(pending, tok, sizeof(pending) - 1);
                pending[sizeof(pending) - 1] = '\0';
            }
            continue;
        }
        if (strcmp(tok, "{") == 0) { inStage = 1; continue; }
        if (strcmp(tok, "}") == 0 && inStage) { inStage = 0; continue; }
        if (strcmp(tok, "}") == 0 && !inStage) { inShader = 0; current[0] = '\0'; continue; }
        if (!inStage) continue;
        {
            char *save = NULL;
            char *w = strtok_r(tok, " \t", &save);
            if (!w) continue;
            if (strcasecmp(w, "map") == 0 || strcasecmp(w, "clampmap") == 0 || strcasecmp(w, "qer_editorimage") == 0) {
                char *arg = strtok_r(NULL, " \t", &save);
                if (arg && strcasecmp(arg, texturePath) == 0) {
                    strncpy(outMaterial, current, outSize - 1);
                    outMaterial[outSize - 1] = '\0';
                    fclose(f);
                    return 1;
                }
            } else if (strcasecmp(w, "animMap") == 0) {
                (void)strtok_r(NULL, " \t", &save); /* fps */
                while (1) {
                    char *fr = strtok_r(NULL, " \t", &save);
                    if (!fr) break;
                    if (strcasecmp(fr, texturePath) == 0) {
                        strncpy(outMaterial, current, outSize - 1);
                        outMaterial[outSize - 1] = '\0';
                        fclose(f);
                        return 1;
                    }
                }
            }
        }
    }
    fclose(f);
    return 0;
}

int ResolveMaterialFromTexture(app_t *a, const char *texturePath, char *outMaterial, size_t outSize) {
    char scriptsDir[1400];
    DIR *d;
    struct dirent *ent;
    if (!a || !texturePath || !texturePath[0]) return 0;
    snprintf(scriptsDir, sizeof(scriptsDir), "%s/scripts", a->mesh.gameRoot);
    d = opendir(scriptsDir);
    if (!d) return 0;
    while ((ent = readdir(d)) != NULL) {
        char shaderPath[1700];
        size_t n = strlen(ent->d_name);
        if (n < 8) continue;
        if (strcasecmp(ent->d_name + n - 7, ".shader") != 0) continue;
        snprintf(shaderPath, sizeof(shaderPath), "%s/%s", scriptsDir, ent->d_name);
        if (FindMaterialByTextureInShaderFile(shaderPath, texturePath, outMaterial, outSize)) {
            closedir(d);
            return 1;
        }
    }
    closedir(d);
    return 0;
}

static int ResolveTextureFromMaterialInShaderFile(const char *shaderFile, const char *materialName, char *outTexture, size_t outSize) {
    FILE *f = fopen(shaderFile, "rb");
    char line[2048];
    char pending[512] = {0};
    char current[512] = {0};
    int inShader = 0, inStage = 0;
    int stageHasBlend = 0;
    int matchedShader = 0;
    char firstTex[512] = {0};
    char blendTex[512] = {0};
    char animTex[512] = {0};
    char stageLastTex[512] = {0};
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        char *comment = strstr(line, "//");
        char *tok;
        if (comment) *comment = '\0';
        tok = line;
        while (*tok == ' ' || *tok == '\t' || *tok == '\r' || *tok == '\n') tok++;
        if (!*tok) continue;
        {
            size_t L = strlen(tok);
            while (L > 0 && (tok[L - 1] == '\n' || tok[L - 1] == '\r' || tok[L - 1] == ' ' || tok[L - 1] == '\t')) tok[--L] = '\0';
        }
        if (!inShader) {
            if (strchr(tok, '{') && pending[0]) {
                strncpy(current, pending, sizeof(current) - 1);
                current[sizeof(current) - 1] = '\0';
                inShader = 1;
                inStage = 0;
                continue;
            }
            if (!strchr(tok, '{') && !strchr(tok, '}')) {
                strncpy(pending, tok, sizeof(pending) - 1);
                pending[sizeof(pending) - 1] = '\0';
            }
            continue;
        }
        if (strcmp(tok, "{") == 0) { inStage = 1; stageHasBlend = 0; stageLastTex[0] = '\0'; continue; }
        if (strcmp(tok, "}") == 0 && inStage) { inStage = 0; stageHasBlend = 0; stageLastTex[0] = '\0'; continue; }
        if (strcmp(tok, "}") == 0 && !inStage) {
            if (matchedShader) {
                const char *picked = blendTex[0] ? blendTex : (firstTex[0] ? firstTex : (animTex[0] ? animTex : NULL));
                if (picked) {
                    strncpy(outTexture, picked, outSize - 1);
                    outTexture[outSize - 1] = '\0';
                    fprintf(stderr, "[material_viewer_vk] script hit: %s -> %s -> %s\n", shaderFile, materialName, outTexture);
                    fflush(stderr);
                    fclose(f);
                    return 1;
                }
            }
            inShader = 0;
            current[0] = '\0';
            matchedShader = 0;
            firstTex[0] = blendTex[0] = animTex[0] = '\0';
            continue;
        }
        if (!inStage) continue;
        if (strcasecmp(current, materialName) != 0) continue;
        matchedShader = 1;
        {
            char *save = NULL;
            char *w = strtok_r(tok, " \t", &save);
            if (!w) continue;
            if (strcasecmp(w, "blendfunc") == 0 || strcasecmp(w, "blendFunc") == 0) {
                char *a0 = strtok_r(NULL, " \t", &save);
                char *a1 = strtok_r(NULL, " \t", &save);
                if (a0 && (strcasecmp(a0, "blend") == 0 || a1)) {
                    stageHasBlend = 1;
                    if (!blendTex[0] && stageLastTex[0]) {
                        strncpy(blendTex, stageLastTex, sizeof(blendTex) - 1);
                        blendTex[sizeof(blendTex) - 1] = '\0';
                    }
                }
                continue;
            }
            if (strcasecmp(w, "map") == 0 || strcasecmp(w, "clampmap") == 0) {
                char *arg = strtok_r(NULL, " \t", &save);
                if (arg && arg[0] != '$') {
                    strncpy(stageLastTex, arg, sizeof(stageLastTex) - 1);
                    stageLastTex[sizeof(stageLastTex) - 1] = '\0';
                    if (stageHasBlend && !blendTex[0]) {
                        strncpy(blendTex, arg, sizeof(blendTex) - 1);
                        blendTex[sizeof(blendTex) - 1] = '\0';
                    }
                    if (!firstTex[0]) {
                        strncpy(firstTex, arg, sizeof(firstTex) - 1);
                        firstTex[sizeof(firstTex) - 1] = '\0';
                    }
                }
            } else if (strcasecmp(w, "animMap") == 0) {
                (void)strtok_r(NULL, " \t", &save); /* fps */
                while (1) {
                    char *fr = strtok_r(NULL, " \t", &save);
                    if (!fr) break;
                    if (fr[0] != '$') {
                        if (!animTex[0]) {
                            strncpy(animTex, fr, sizeof(animTex) - 1);
                            animTex[sizeof(animTex) - 1] = '\0';
                        }
                        if (!firstTex[0]) {
                            strncpy(firstTex, fr, sizeof(firstTex) - 1);
                            firstTex[sizeof(firstTex) - 1] = '\0';
                        }
                        break;
                    }
                }
            }
        }
    }
    fclose(f);
    return 0;
}

int ResolveTextureFromMaterialScripts(app_t *a, const char *materialName, char *outTexture, size_t outSize) {
    char scriptsDir[1400];
    DIR *d;
    struct dirent *ent;
    if (!a || !materialName || !materialName[0]) return 0;
    snprintf(scriptsDir, sizeof(scriptsDir), "%s/scripts", a->mesh.gameRoot);
    fprintf(stderr, "[material_viewer_vk] script-first lookup for material: %s\n", materialName);
    fflush(stderr);
    d = opendir(scriptsDir);
    if (!d) {
        fprintf(stderr, "[material_viewer_vk] scripts dir not found: %s\n", scriptsDir);
        fflush(stderr);
        return 0;
    }
    while ((ent = readdir(d)) != NULL) {
        char shaderPath[1700];
        size_t n = strlen(ent->d_name);
        if (n < 8) continue;
        if (strcasecmp(ent->d_name + n - 7, ".shader") != 0) continue;
        snprintf(shaderPath, sizeof(shaderPath), "%s/%s", scriptsDir, ent->d_name);
        if (ResolveTextureFromMaterialInShaderFile(shaderPath, materialName, outTexture, outSize)) {
            closedir(d);
            return 1;
        }
    }
    closedir(d);
    fprintf(stderr, "[material_viewer_vk] script-first miss for material: %s\n", materialName);
    fflush(stderr);
    return 0;
}

static int LoadAnimMapFromScripts(app_t *a, const char *materialPath) {
    char scriptsDir[1400];
    DIR *d;
    struct dirent *ent;
    if (!a || !materialPath || !materialPath[0]) return 0;
    snprintf(scriptsDir, sizeof(scriptsDir), "%s/scripts", a->mesh.gameRoot);
    d = opendir(scriptsDir);
    if (!d) return 0;
    while ((ent = readdir(d)) != NULL) {
        char shaderPath[1700];
        size_t n = strlen(ent->d_name);
        char **frames = NULL;
        int count = 0;
        float fps = 0.0f;
        if (n < 8) continue;
        if (strcasecmp(ent->d_name + n - 7, ".shader") != 0) continue;
        snprintf(shaderPath, sizeof(shaderPath), "%s/%s", scriptsDir, ent->d_name);
        if (!ParseAnimMapFromShaderFile(shaderPath, materialPath, &frames, &count, &fps)) continue;
        a->materialAnimTextures = (vk_texture_t *)calloc((size_t)count, sizeof(vk_texture_t));
        if (!a->materialAnimTextures) {
            for (int i = 0; i < count; i++) free(frames[i]);
            free(frames);
            break;
        }
        for (int i = 0; i < count; i++) {
            if (!TryLoadTextureByPath(a, frames[i], &a->materialAnimTextures[a->materialAnimCount])) break;
            a->materialAnimCount++;
        }
        for (int i = 0; i < count; i++) free(frames[i]);
        free(frames);
        if (a->materialAnimCount > 1) {
            a->materialAnimFps = fps > 0.0f ? fps : 13.0f;
            closedir(d);
            return 1;
        }
        for (uint32_t i = 0; i < a->materialAnimCount; i++) DestroyTexture(a, &a->materialAnimTextures[i]);
        free(a->materialAnimTextures);
        a->materialAnimTextures = NULL;
        a->materialAnimCount = 0;
    }
    closedir(d);
    return 0;
}
#endif /* MATERIAL_VIEWER_VK */

void UpdateBatchBaseTexture(app_t *a, uint32_t batchIndex, uint32_t frameSlot, const vk_texture_t *baseTex) {
    VkDescriptorImageInfo infos[2];
    VkWriteDescriptorSet wr[2];
    const draw_batch_t *b;
    const vk_texture_t *light = &a->whiteTexture;
    if (!a || batchIndex >= a->mesh.batchCount || frameSlot >= MAX_FRAMES_IN_FLIGHT || !baseTex || baseTex->view == VK_NULL_HANDLE) return;
    b = &a->mesh.batches[batchIndex];
    if (b->descriptorSets[frameSlot] == VK_NULL_HANDLE) return;
    infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    infos[0].imageView = baseTex->view;
    infos[0].sampler = baseTex->sampler;
    infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    infos[1].imageView = light->view;
    infos[1].sampler = light->sampler;
    memset(wr, 0, sizeof(wr));
    wr[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr[0].dstSet = b->descriptorSets[frameSlot];
    wr[0].dstBinding = 0;
    wr[0].descriptorCount = 1;
    wr[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wr[0].pImageInfo = &infos[0];
    wr[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr[1].dstSet = b->descriptorSets[frameSlot];
    wr[1].dstBinding = 1;
    wr[1].descriptorCount = 1;
    wr[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wr[1].pImageInfo = &infos[1];
    vkUpdateDescriptorSets(a->dev, 2, wr, 0, NULL);
}

#ifdef MATERIAL_VIEWER_VK
void InitMaterialInternalAnimation(app_t *a, const char *materialPath) {
    char prefix[512];
    char buf[512];
    int len, end, start, digits, baseNum;
    char resolvedMaterial[512];
    char uiWolfPath[1600];
    shader_material_t *sm;
    if (!a || !materialPath || !materialPath[0]) return;
    sm = a->mesh.shaderMaterials;
    if (!sm) return;
    a->animScaleU = 1.0f;
    a->animScaleV = 1.0f;
    a->animScrollU = 0.0f;
    a->animScrollV = 0.0f;
    a->animRotateDegPerSec = 0.0f;
    a->rgbWaveMode = 0;
    a->rgbWaveBase = 1.0f;
    a->rgbWaveAmp = 0.0f;
    a->rgbWavePhase = 0.0f;
    a->rgbWaveFreq = 1.0f;
    a->hasUiShaderAnim = 0;
    a->materialQuadAspectDirty = 0;
    sm->stageCount = 0;
    sm->drawOrderCount = 0;
    for (int i = 0; i < MAX_MATERIAL_STAGES; i++) ResetMaterialStageAnim(&sm->stages[i]);
    snprintf(uiWolfPath, sizeof(uiWolfPath), "%s/scripts/ui_wolf.shader", a->mesh.gameRoot);
    (void)ParseUiAnimFromShaderFile(a, uiWolfPath, materialPath);
    (void)ParseMaterialStagesFromScripts(a, materialPath, sm);
    for (uint32_t i = 0; i < sm->stageCount; i++) {
        if (sm->stages[i].hasTexture && sm->stages[i].texturePath[0]) {
            if (!TryLoadTextureByPath(a, sm->stages[i].texturePath, &sm->stages[i].texture)) {
                fprintf(stderr, "[material_viewer_vk] stage texture load failed: %s\n", sm->stages[i].texturePath);
                fflush(stderr);
                sm->stages[i].hasTexture = 0;
            }
        }
        if (sm->stages[i].useLightmap) {
            fprintf(stderr, "[material_viewer_vk] stage %u: map $lightmap, blendMode=%d\n", i, sm->stages[i].blendMode);
        } else if (sm->stages[i].hasTexture) {
            fprintf(stderr, "[material_viewer_vk] stage %u: map %s, blendMode=%d\n", i, sm->stages[i].texturePath, sm->stages[i].blendMode);
        } else {
            fprintf(stderr, "[material_viewer_vk] stage %u: no map, blendMode=%d\n", i, sm->stages[i].blendMode);
        }
        fflush(stderr);
    }
    if (sm->stageCount > 0) {
        const char *aspectTex = NULL;
        for (uint32_t i = 0; i < sm->stageCount; i++) {
            if (sm->stages[i].hasTexture && sm->stages[i].blendMode == STAGE_BLEND_ALPHA) {
                aspectTex = sm->stages[i].texturePath;
                break;
            }
        }
        if (!aspectTex) {
            for (uint32_t i = 0; i < sm->stageCount; i++) {
                if (sm->stages[i].hasTexture &&
                    (fabsf(sm->stages[i].scrollU) > 1e-6f ||
                     fabsf(sm->stages[i].scrollV) > 1e-6f ||
                     fabsf(sm->stages[i].rotateDegPerSec) > 1e-6f ||
                     fabsf(sm->stages[i].scaleU - 1.0f) > 1e-6f ||
                     fabsf(sm->stages[i].scaleV - 1.0f) > 1e-6f ||
                     sm->stages[i].hasTurb ||
                     sm->stages[i].hasTransform ||
                     sm->stages[i].hasSwap)) {
                    aspectTex = sm->stages[i].texturePath;
                    break;
                }
            }
        }
        if (!aspectTex) {
            for (uint32_t i = 0; i < sm->stageCount; i++) {
                if (sm->stages[i].hasTexture) {
                    aspectTex = sm->stages[i].texturePath;
                    break;
                }
            }
        }
        if (aspectTex && UpdateMaterialQuadAspect(&a->mesh, a->mesh.gameRoot, aspectTex)) {
            a->materialQuadAspectDirty = 1;
        }
    }
    ShaderMaterialFinalizeDrawOrder(a, sm);
    a->mesh.materialMaxPasses = sm->drawOrderCount;
    if (LoadAnimMapFromScripts(a, materialPath)) return;
    if (ResolveMaterialFromTexture(a, materialPath, resolvedMaterial, sizeof(resolvedMaterial))) {
        (void)ParseUiAnimFromShaderFile(a, uiWolfPath, resolvedMaterial);
        if (sm->stageCount == 0) {
            (void)ParseMaterialStagesFromScripts(a, resolvedMaterial, sm);
            for (uint32_t i = 0; i < sm->stageCount; i++) {
                if (sm->stages[i].hasTexture && sm->stages[i].texturePath[0]) {
                    if (!TryLoadTextureByPath(a, sm->stages[i].texturePath, &sm->stages[i].texture)) {
                        fprintf(stderr, "[material_viewer_vk] stage texture load failed: %s\n", sm->stages[i].texturePath);
                        fflush(stderr);
                        sm->stages[i].hasTexture = 0;
                    }
                }
                if (sm->stages[i].useLightmap) {
                    fprintf(stderr, "[material_viewer_vk] stage %u: map $lightmap, blendMode=%d\n", i, sm->stages[i].blendMode);
                } else if (sm->stages[i].hasTexture) {
                    fprintf(stderr, "[material_viewer_vk] stage %u: map %s, blendMode=%d\n", i, sm->stages[i].texturePath, sm->stages[i].blendMode);
                } else {
                    fprintf(stderr, "[material_viewer_vk] stage %u: no map, blendMode=%d\n", i, sm->stages[i].blendMode);
                }
                fflush(stderr);
            }
        }
        ShaderMaterialFinalizeDrawOrder(a, sm);
        a->mesh.materialMaxPasses = sm->drawOrderCount;
        if (LoadAnimMapFromScripts(a, resolvedMaterial)) return;
    }
    /* If full stage script is present, do not run numeric filename animation fallback. */
    ShaderMaterialFinalizeDrawOrder(a, sm);
    a->mesh.materialMaxPasses = sm->drawOrderCount;
    if (sm->stageCount > 0) return;
    len = (int)strlen(materialPath);
    if (len <= 0 || len >= (int)sizeof(prefix)) return;
    end = len - 1;
    while (end >= 0 && materialPath[end] >= '0' && materialPath[end] <= '9') end--;
    digits = len - 1 - end;
    if (digits <= 0 || digits > 4) return;
    start = end + 1;
    baseNum = atoi(materialPath + start);
    memcpy(prefix, materialPath, (size_t)start);
    prefix[start] = '\0';
    a->materialAnimTextures = (vk_texture_t *)calloc(16, sizeof(vk_texture_t));
    if (!a->materialAnimTextures) return;
    for (int i = 0; i < 16; i++) {
        int num = baseNum + i;
        snprintf(buf, sizeof(buf), "%s%0*d", prefix, digits, num);
        if (!TryLoadTextureByPath(a, buf, &a->materialAnimTextures[a->materialAnimCount])) {
            break;
        }
        a->materialAnimCount++;
    }
    if (a->materialAnimCount < 2) {
        for (uint32_t i = 0; i < a->materialAnimCount; i++) DestroyTexture(a, &a->materialAnimTextures[i]);
        free(a->materialAnimTextures);
        a->materialAnimTextures = NULL;
        a->materialAnimCount = 0;
        return;
    }
    a->materialAnimFps = 13.0f;
}
#endif /* MATERIAL_VIEWER_VK */

#include "vk_local.h"
#include "vk_material.h"
#include "vk_sky.h"
#include "../game/surfaceflags.h"
#include <string.h>

extern void (*vk_surfaceTable[SF_NUM_SURFACE_TYPES])(void *);

extern int vk_worldDrawCount;
extern int vk_worldVboFailCount;

static float vk_originalTime;
static int vk_oldEntityNum = -1;
static cvar_t *r_vkVolumetricFog;

static void VK_FillStagePushConstants(const shader_t *shader, vk_push_constants_t *pc) {
    float proj[16];
    float mvp[16];

    /* Match GL depth distribution: engine projection + Vulkan Y flip + Z remap.
     * Use backEnd.viewParms so portal and main views use the correct projection. */
    memcpy(proj, backEnd.viewParms.projectionMatrix, sizeof(proj));
    VK_ConvertProjectionMatrixToVulkan(proj);
    VK_MatrixMulQ3Clip(mvp, proj, backEnd.or.modelMatrix);
    VK_FillPushConstants(mvp, shader, pc);
}

static void VK_GetFogVectors(vec4_t fogDistanceVector, vec4_t fogDepthVector, float *eyeT) {
    fog_t *fog;
    vec3_t local;

    fog = tr.world->fogs + tess.fogNum;

    VectorSubtract(backEnd.or.origin, backEnd.viewParms.or.origin, local);
    fogDistanceVector[0] = -backEnd.or.modelMatrix[2];
    fogDistanceVector[1] = -backEnd.or.modelMatrix[6];
    fogDistanceVector[2] = -backEnd.or.modelMatrix[10];
    fogDistanceVector[3] = DotProduct(local, backEnd.viewParms.or.axis[0]);

    /* Match OpenGL's RB_CalcFogTexCoords: scale all four components of the
     * distance vector by fog->tcScale, including the translation component. */
    fogDistanceVector[0] *= fog->tcScale;
    fogDistanceVector[1] *= fog->tcScale;
    fogDistanceVector[2] *= fog->tcScale;
    fogDistanceVector[3] *= fog->tcScale;

    if (fog->hasSurface) {
        fogDepthVector[0] = fog->surface[0] * backEnd.or.axis[0][0] +
                            fog->surface[1] * backEnd.or.axis[0][1] +
                            fog->surface[2] * backEnd.or.axis[0][2];
        fogDepthVector[1] = fog->surface[0] * backEnd.or.axis[1][0] +
                            fog->surface[1] * backEnd.or.axis[1][1] +
                            fog->surface[2] * backEnd.or.axis[1][2];
        fogDepthVector[2] = fog->surface[0] * backEnd.or.axis[2][0] +
                            fog->surface[1] * backEnd.or.axis[2][1] +
                            fog->surface[2] * backEnd.or.axis[2][2];
        fogDepthVector[3] = -fog->surface[3] + DotProduct(backEnd.or.origin, fog->surface);

        *eyeT = DotProduct(backEnd.or.viewOrigin, fogDepthVector) + fogDepthVector[3];
    } else {
        VectorClear(fogDepthVector);
        *eyeT = 1.0f;
    }

    fogDistanceVector[3] += 1.0f / 512.0f;
}

static void VK_FogPass(drawSurf_t *drawSurf, surfaceType_t type, VkCommandBuffer cmd) {
    fog_t *fog;
    vk_push_constants_t pc;
    vec4_t fogDistanceVector;
    vec4_t fogDepthVector;
    float eyeT;
    int pipelineIdx;
    unsigned color;

    if (!tess.fogNum || !tess.shader->fogPass) {
        return;
    }
    if (backEnd.refdef.rdflags & RDF_SNOOPERVIEW) {
        return;
    }
    if (!cmd) {
        return;
    }

    fog = tr.world->fogs + tess.fogNum;

    pipelineIdx = (tess.shader->fogPass == FP_EQUAL) ? VK_PIPELINE_FOG_EQUAL : VK_PIPELINE_FOG;

    VK_FillStagePushConstants(tess.shader, &pc);

    /* Use the volumetric fog volume color. params16.w == 4.0 selects the fog
     * pass path in the shaders; it also disables the distance-fog branch.
     * Modes 1..3 are reserved for distance fog (linear, exp, exp2). */
    color = fog->colorInt;
    pc.params[VK_FOG_COLOR_PARAM][0] = (float)(color & 0xFF) / 255.0f;
    pc.params[VK_FOG_COLOR_PARAM][1] = (float)((color >> 8) & 0xFF) / 255.0f;
    pc.params[VK_FOG_COLOR_PARAM][2] = (float)((color >> 16) & 0xFF) / 255.0f;
    pc.params[VK_FOG_COLOR_PARAM][3] = 4.0f;

    /* Disable distance fog modulation for this pass. */
    pc.params[VK_FOG_RANGE_PARAM][0] = 0.0f;
    pc.params[VK_FOG_RANGE_PARAM][1] = 0.0f;
    pc.params[VK_FOG_RANGE_PARAM][2] = 0.0f;
    pc.params[VK_FOG_RANGE_PARAM][3] = 0.0f;

    VK_GetFogVectors(fogDistanceVector, fogDepthVector, &eyeT);
    (void)eyeT;
    pc.params[18][0] = fogDistanceVector[0];
    pc.params[18][1] = fogDistanceVector[1];
    pc.params[18][2] = fogDistanceVector[2];
    pc.params[18][3] = fogDistanceVector[3];
    pc.params[19][0] = fogDepthVector[0];
    pc.params[19][1] = fogDepthVector[1];
    pc.params[19][2] = fogDepthVector[2];
    pc.params[19][3] = fogDepthVector[3];

    /* Force a fog-texture draw: no alpha test, no lightmap, no vertex color,
     * no environment mapping, no dlight pass flag, no water fog. */
    pc.params[0][2] = 0.0f;                 /* alpha test off */
    pc.params[7][0] = 0.0f;                 /* rgbGen vertex off */
    pc.params[7][1] = 0.0f;                 /* alphaGen vertex off */
    pc.params[7][2] = 1.0f;                 /* no lightmap */
    pc.params[7][3] = 0.0f;                 /* no environment mapping */
    pc.params[11][2] = 0.0f;                /* water fog off */
    pc.params[13][0] = 0.0f;                /* portal clip plane off */
    pc.params[13][1] = 0.0f;
    pc.params[13][2] = 0.0f;
    pc.params[13][3] = 0.0f;
    pc.params[14][3] = 0.0f;                /* not a dlight pass */
    pc.params[15][3] = 0.0f;                /* normal Z fade off */

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipelineIdx]);
    vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);
    vkCmdPushConstants(cmd, vk_state.pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vk_state.pipelineLayout, 0, 1, &vk_state.fogDescSet, 0, NULL);

    tess.numVertexes = 0;
    tess.numIndexes = 0;
    vk_surfaceTable[type](drawSurf->surface);
}

static qboolean vk_depthRange = qfalse;

static void VK_SetViewViewport(void) {
    VkCommandBuffer cmd;
    VkViewport vp;
    VkRect2D sc;
    int vpY;

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    if (!cmd) {
        return;
    }

    if (backEnd.viewParms.viewportWidth <= 0 || backEnd.viewParms.viewportHeight <= 0) {
        vp.x = 0.0f;
        vp.y = 0.0f;
        vp.width = (float)vk_state.swapExtent.width;
        vp.height = (float)vk_state.swapExtent.height;
        sc.offset.x = 0;
        sc.offset.y = 0;
        sc.extent = vk_state.swapExtent;
    } else {
        vpY = (int)vk_state.swapExtent.height
            - backEnd.viewParms.viewportY
            - backEnd.viewParms.viewportHeight;
        vp.x = (float)backEnd.viewParms.viewportX;
        vp.y = (float)vpY;
        vp.width = (float)backEnd.viewParms.viewportWidth;
        vp.height = (float)backEnd.viewParms.viewportHeight;
        sc.offset.x = backEnd.viewParms.viewportX;
        sc.offset.y = vpY;
        sc.extent.width = (uint32_t)backEnd.viewParms.viewportWidth;
        sc.extent.height = (uint32_t)backEnd.viewParms.viewportHeight;
    }

    vp.minDepth = 0.0f;
    vp.maxDepth = vk_depthRange ? 0.3f : 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
}

static void VK_SetupEntity(int entityNum) {
    if (entityNum != ENTITYNUM_WORLD) {
        backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
        backEnd.refdef.floatTime = vk_originalTime - backEnd.currentEntity->e.shaderTime;
        R_SetupEntityLighting(&backEnd.refdef, backEnd.currentEntity);
        R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.or);
        if (backEnd.currentEntity->needDlights) {
            R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or);
        }
    } else {
        backEnd.currentEntity = &tr.worldEntity;
        backEnd.refdef.floatTime = vk_originalTime;
        backEnd.or = backEnd.viewParms.world;
        R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or);
    }
}

static qboolean VK_ShouldDrawShader(const shader_t *shader) {
    if (!shader || shader->sort == SS_BAD) {
        return qfalse;
    }
    if (shader->surfaceFlags & SURF_NODRAW) {
        return qfalse;
    }
    return qtrue;
}

static int VK_SurfaceDlightBits(surfaceType_t *surf) {
    switch (*surf) {
    case SF_FACE:
        return ((srfSurfaceFace_t *)surf)->dlightBits[backEnd.smpFrame];
    case SF_GRID:
        return ((srfGridMesh_t *)surf)->dlightBits[backEnd.smpFrame];
    case SF_TRIANGLES:
        return ((srfTriangles_t *)surf)->dlightBits[backEnd.smpFrame];
    default:
        return 0;
    }
}

static void VK_FillDlightPushConstants(const dlight_t *dl, vk_push_constants_t *pc) {
    float proj[16];
    float mvp[16];

    memcpy(proj, backEnd.viewParms.projectionMatrix, sizeof(proj));
    VK_ConvertProjectionMatrixToVulkan(proj);
    VK_MatrixMulQ3Clip(mvp, proj, backEnd.or.modelMatrix);
    memcpy(pc->mvp, mvp, sizeof(pc->mvp));
    memset(pc->params, 0, sizeof(pc->params));

    pc->params[1][0] = dl->transformed[0];
    pc->params[1][1] = dl->transformed[1];
    pc->params[1][2] = dl->transformed[2];
    pc->params[1][3] = dl->radius;

    pc->params[2][0] = dl->color[0];
    pc->params[2][1] = dl->color[1];
    pc->params[2][2] = dl->color[2];

    pc->params[14][0] = backEnd.or.viewOrigin[0];
    pc->params[14][1] = backEnd.or.viewOrigin[1];
    pc->params[14][2] = backEnd.or.viewOrigin[2];
    pc->params[14][3] = 1.0f; /* dlight pass flag */
}

static void VK_ProjectDlightTexture(drawSurf_t *drawSurf, int dlightBits) {
    surfaceType_t *surf;
    surfaceType_t type;
    VkCommandBuffer cmd;
    vk_push_constants_t pc;
    int i;

    surf = drawSurf->surface;
    if (!surf) {
        return;
    }

    type = *surf;
    if (type <= SF_BAD || type >= SF_NUM_SURFACE_TYPES) {
        return;
    }

    if (!backEnd.refdef.num_dlights || !dlightBits) {
        return;
    }

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    if (!cmd) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[VK_PIPELINE_DLIGHT]);
    vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);

    {
        VkDescriptorSet descSet = VK_GetDescriptorSetForImages(tr.whiteImage, tr.whiteImage);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);
    }

    for (i = 0; i < backEnd.refdef.num_dlights; i++) {
        const dlight_t *dl;

        if (!(dlightBits & (1 << i))) {
            continue;
        }

        dl = &backEnd.refdef.dlights[i];

        VK_FillDlightPushConstants(dl, &pc);
        vkCmdPushConstants(cmd, vk_state.pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(pc), &pc);

        /* Surface functions append geometry and issue the draw call. */
        tess.numVertexes = 0;
        tess.numIndexes = 0;
        vk_surfaceTable[type](surf);
    }
}

static void VK_DrawSurfaceStages(drawSurf_t *drawSurf, shader_t *shader) {
    surfaceType_t *surf;
    surfaceType_t type;
    VkCommandBuffer cmd;
    int pass;
    int stageIdx;
    int currentPipe;
    int fogNum;
    int entityNum;
    int dlighted;
    int atiTess;
    shader_t *sortShader;
    vk_push_constants_t pc;

    surf = drawSurf->surface;
    if (!surf) {
        return;
    }

    R_DecomposeSort(drawSurf->sort, &entityNum, &sortShader, &fogNum, &dlighted, &atiTess);
    tess.shader = shader;
    tess.fogNum = fogNum;

    /* Match GL skyboxportal logic: main view skips sky, portal view draws only sky (unless drawskyboxportal). */
    if (skyboxportal) {
        if (!(backEnd.refdef.rdflags & RDF_SKYBOXPORTAL)) {
            if (shader->isSky) {
                return;
            }
        } else {
            if (!drawskyboxportal && !shader->isSky) {
                return;
            }
        }
    }

    type = *surf;
    if (type <= SF_BAD || type >= SF_NUM_SURFACE_TYPES) {
        return;
    }

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    currentPipe = -1;

    if (shader->isSky) {
        VK_DrawSky(shader, surf, cmd);
        return;
    }

    for (pass = 0; pass < 4; pass++) {
        for (stageIdx = 0; stageIdx < MAX_SHADER_STAGES && shader->stages[stageIdx]; stageIdx++) {
            shaderStage_t *stage = shader->stages[stageIdx];
            qboolean blended;
            int pipeIdx;
            VkDescriptorSet descSet;

            if (!stage->active) {
                continue;
            }

            blended = VK_StageIsBlended(stage);
            if (!VK_RenderPassMatchesStage(pass, shader->polygonOffset, blended)) {
                continue;
            }

            VK_SetStageStateFromShader(shader, stage);
            VK_FillStagePushConstants(shader, &pc);

            /* Match OpenGL's per-stage fog toggling: shaders with noFog only get
             * distance fog on stages explicitly marked as isFogged. */
            if (shader->noFog && !stage->isFogged) {
                pc.params[VK_FOG_COLOR_PARAM][3] = 0.0f;
            }

            /* Volumetric fog modulation for translucent surfaces inside fog volumes.
             * params17.w encodes the modulation mode (2=RGB, 3=RGBA, 4=ALPHA)
             * and params18/19 hold the fog distance/depth vectors. */
            if (!r_vkVolumetricFog) {
                r_vkVolumetricFog = ri.Cvar_Get("r_vkVolumetricFog", "1", CVAR_ARCHIVE);
            }
            if (r_vkVolumetricFog->integer && tess.fogNum && stage->adjustColorsForFog != ACFF_NONE) {
                vec4_t distVec;
                vec4_t depthVec;
                float eyeT;

                VK_GetFogVectors(distVec, depthVec, &eyeT);
                (void)eyeT;
                pc.params[18][0] = distVec[0];
                pc.params[18][1] = distVec[1];
                pc.params[18][2] = distVec[2];
                pc.params[18][3] = distVec[3];
                pc.params[19][0] = depthVec[0];
                pc.params[19][1] = depthVec[1];
                pc.params[19][2] = depthVec[2];
                pc.params[19][3] = depthVec[3];

                switch (stage->adjustColorsForFog) {
                case ACFF_MODULATE_RGB:
                    pc.params[17][3] = 2.0f;
                    break;
                case ACFF_MODULATE_RGBA:
                    pc.params[17][3] = 3.0f;
                    break;
                case ACFF_MODULATE_ALPHA:
                    pc.params[17][3] = 4.0f;
                    break;
                default:
                    pc.params[17][3] = 0.0f;
                    break;
                }
            }

            pipeIdx = VK_PipelineForStage(stage);
            if (pipeIdx != currentPipe) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipeIdx]);
                currentPipe = pipeIdx;
            }

            if (VK_MaterialPolygonOffset()) {
                vkCmdSetDepthBias(cmd, VK_MaterialPolyOffsetUnits(), 0.0f, VK_MaterialPolyOffsetFactor());
            } else {
                vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);
            }

            vkCmdPushConstants(cmd, vk_state.pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(pc), &pc);

            descSet = VK_StageDescriptorSet(shader, stage);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);
            vk_surfaceTable[type](surf);
        }
    }

    /*
     * Forward dynamic lighting pass, matching OpenGL's ProjectDlightTexture.
     * Only opaque world/brush surfaces receive dlights; MD3 entities do not
     * in the original renderer.
     */
    if (shader->sort <= SS_OPAQUE &&
        !(shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY))) {
        int surfDlightBits = VK_SurfaceDlightBits(surf);
        if (surfDlightBits) {
            VK_ProjectDlightTexture(drawSurf, surfDlightBits);
        }
    }

    /*
     * Volumetric fog volume pass, mirroring OpenGL's RB_FogPass.
     */
    if (!r_vkVolumetricFog) {
        r_vkVolumetricFog = ri.Cvar_Get("r_vkVolumetricFog", "1", CVAR_ARCHIVE);
    }
    if (r_vkVolumetricFog->integer && tess.fogNum && tess.shader->fogPass) {
        VK_FogPass(drawSurf, type, cmd);
    }
}

void VK_RenderFlares(VkCommandBuffer cmd) {
    flare_t *f;
    flare_t **prev;
    qboolean draw;
    shaderCommands_t savedTess;
    VkViewport vp;
    VkRect2D sc;
    int vpX, vpY, vpYBottom, vpW, vpH;

    if (!cmd || !vk_state.renderPassActive) {
        return;
    }

    if (!r_flares->integer) {
        return;
    }

    /* Collect dynamic-light and corona flares, mirroring OpenGL. */
    RB_AddDlightFlares();
    RB_AddCoronaFlares();

    /* Age out stale flares, test visibility, and keep only relevant ones. */
    draw = qfalse;
    prev = &r_activeFlares;
    while ((f = *prev) != NULL) {
        if (f->addedFrame < backEnd.viewParms.frameCount - 1) {
            *prev = f->next;
            f->next = r_inactiveFlares;
            r_inactiveFlares = f;
            continue;
        }

        f->drawIntensity = 0.0f;
        if (f->frameSceneNum == backEnd.viewParms.frameSceneNum
            && f->inPortal == backEnd.viewParms.isPortal) {
            RB_TestFlare(f);
            if (f->drawIntensity) {
                draw = qtrue;
            } else {
                *prev = f->next;
                f->next = r_inactiveFlares;
                r_inactiveFlares = f;
                continue;
            }
        }

        prev = &f->next;
    }

    if (!draw) {
        return;
    }

    savedTess = tess;
    Com_Memset(&tess, 0, sizeof(tess));

    /* Restrict rendering to the current view's viewport. */
    if (backEnd.viewParms.viewportWidth <= 0 || backEnd.viewParms.viewportHeight <= 0) {
        vpX = 0;
        vpY = 0;
        vpYBottom = 0;
        vpW = (int)vk_state.swapExtent.width;
        vpH = (int)vk_state.swapExtent.height;
        vp.x = 0.0f;
        vp.y = 0.0f;
        vp.width = (float)vpW;
        vp.height = (float)vpH;
        sc.offset.x = 0;
        sc.offset.y = 0;
        sc.extent = vk_state.swapExtent;
    } else {
        vpX = backEnd.viewParms.viewportX;
        vpW = backEnd.viewParms.viewportWidth;
        vpH = backEnd.viewParms.viewportHeight;
        vpYBottom = backEnd.viewParms.viewportY;
        vpY = (int)vk_state.swapExtent.height - vpYBottom - vpH;
        vp.x = (float)vpX;
        vp.y = (float)vpY;
        vp.width = (float)vpW;
        vp.height = (float)vpH;
        sc.offset.x = (int32_t)vpX;
        sc.offset.y = (int32_t)vpY;
        sc.extent.width = (uint32_t)vpW;
        sc.extent.height = (uint32_t)vpH;
    }
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
    vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);

    for (f = r_activeFlares; f; f = f->next) {
        shader_t *shader;
        shaderStage_t *stage;
        float size;
        float x0, x1, y0, y1;
        float mvp[16];
        int vboSize, iboSize, vboOff, iboOff;
        drawVert_t *verts;
        int *idx;
        VkDeviceSize offsets[1];
        int pipelineIdx;
        image_t *image;
        VkDescriptorSet descSet;
        byte alpha;
        vec3_t color;
        int iColor[3];
        int i;

        if (f->frameSceneNum != backEnd.viewParms.frameSceneNum
            || f->inPortal != backEnd.viewParms.isPortal
            || f->drawIntensity <= 0.0f) {
            continue;
        }

        shader = (f->flags & 2) ? tr.spotFlareShader : tr.flareShader;
        if (!shader || !shader->stages[0] || !shader->stages[0]->active) {
            continue;
        }
        stage = shader->stages[0];

        VectorScale(f->color, tr.identityLight, color);
        iColor[0] = (int)(color[0] * 255.0f);
        iColor[1] = (int)(color[1] * 255.0f);
        iColor[2] = (int)(color[2] * 255.0f);
        alpha = (byte)(f->drawIntensity * 255.0f);

        size = backEnd.viewParms.viewportWidth *
               ((r_flareSize->value * f->scale) / 640.0f + 8.0f / -f->eyeZ);

        x0 = f->windowXF - size;
        x1 = f->windowXF + size;
        y0 = f->windowYF - size;
        y1 = f->windowYF + size;

        /* Pick the 2D pipeline that matches the flare shader's blend mode.
         * flareShader uses GL_SRC_ALPHA GL_ONE; spotLight uses
         * GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA. */
        {
            int srcBlend = stage->stateBits & GLS_SRCBLEND_BITS;
            int dstBlend = stage->stateBits & GLS_DSTBLEND_BITS;
            if (srcBlend == GLS_SRCBLEND_SRC_ALPHA && dstBlend == GLS_DSTBLEND_ONE) {
                pipelineIdx = VK_PIPELINE_2D_SRC_ALPHA_ONE;
            } else if (dstBlend == GLS_DSTBLEND_ONE) {
                pipelineIdx = VK_PIPELINE_2D_ADDITIVE;
            } else {
                pipelineIdx = VK_PIPELINE_2D;
            }
        }

        /* Ortho matrix that maps OpenGL-style screen pixels to Vulkan NDC.
         * Flare window coordinates are bottom-left origin (y=0 at the bottom),
         * matching OpenGL's qglOrtho. Vulkan NDC has y=-1 at the top and y=+1
         * at the bottom, so a negative Y scale maps bottom-left screen coords
         * correctly. vpYBottom is the viewport's bottom edge in screen space. */
        memset(mvp, 0, sizeof(mvp));
        mvp[0]  = 2.0f / (float)vpW;
        mvp[5]  = -2.0f / (float)vpH;
        mvp[10] = 1.0f;
        mvp[12] = -1.0f - 2.0f * (float)vpX / (float)vpW;
        mvp[13] = 1.0f + 2.0f * (float)vpYBottom / (float)vpH;
        mvp[15] = 1.0f;

        vboSize = 4 * (int)sizeof(drawVert_t);
        iboSize = 6 * (int)sizeof(int);
        vboOff = VK_ReserveDynamicVBO(vboSize + iboSize);
        if (vboOff < 0) {
            continue;
        }
        iboOff = vboOff + vboSize;

        verts = (drawVert_t *)((uint8_t *)vk_dyn.mapped + vboOff);
        idx = (int *)((uint8_t *)vk_dyn.mapped + iboOff);

        verts[0].xyz[0] = x0; verts[0].xyz[1] = y0; verts[0].xyz[2] = 0.0f;
        verts[0].st[0] = 0.0f; verts[0].st[1] = 0.0f;
        verts[1].xyz[0] = x0; verts[1].xyz[1] = y1; verts[1].xyz[2] = 0.0f;
        verts[1].st[0] = 0.0f; verts[1].st[1] = 1.0f;
        verts[2].xyz[0] = x1; verts[2].xyz[1] = y1; verts[2].xyz[2] = 0.0f;
        verts[2].st[0] = 1.0f; verts[2].st[1] = 1.0f;
        verts[3].xyz[0] = x1; verts[3].xyz[1] = y0; verts[3].xyz[2] = 0.0f;
        verts[3].st[0] = 1.0f; verts[3].st[1] = 0.0f;

        for (i = 0; i < 4; i++) {
            verts[i].lightmap[0] = 0.0f;
            verts[i].lightmap[1] = 0.0f;
            VectorClear(verts[i].normal);
            verts[i].color[0] = (byte)iColor[0];
            verts[i].color[1] = (byte)iColor[1];
            verts[i].color[2] = (byte)iColor[2];
            verts[i].color[3] = alpha;
        }

        idx[0] = 0; idx[1] = 1; idx[2] = 2;
        idx[3] = 0; idx[4] = 2; idx[5] = 3;

        image = VK_BundleImage(&stage->bundle[0], shader);
        descSet = VK_GetDescriptorSetForImage(image ? image : tr.whiteImage);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipelineIdx]);
        offsets[0] = (VkDeviceSize)vboOff;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vk_dyn.buffer, offsets);
        vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
        vkCmdPushConstants(cmd, vk_state.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(mvp), mvp);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);
        vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
    }

    tess = savedTess;
}

void VK_DrawSurfList(drawSurf_t *drawSurfs, int numDrawSurfs, int cmdGlfogNum, const glfog_t *glfog) {
    shader_t *shader;
    int fogNum;
    int entityNum;
    int dlighted;
    int atiTess;
    int i;
    int savedGlfogNum;
    glfog_t savedCurrentFog;

    if (!vk_state.renderPassActive || numDrawSurfs <= 0) {
        return;
    }

    /* The backend may execute after the front-end has already advanced global
     * fog state for the next frame.  Snapshot the fog that was active when
     * this view was generated and use it for the entire surf list. */
    savedGlfogNum = glfogNum;
    savedCurrentFog = glfogsettings[FOG_CURRENT];
    if (glfog && glfog->registered) {
        glfogNum = cmdGlfogNum;
        glfogsettings[FOG_CURRENT] = *glfog;
    }

    vk_worldDrawCount = 0;
    vk_worldVboFailCount = 0;
    vk_originalTime = backEnd.refdef.floatTime;
    vk_oldEntityNum = -1;
    vk_depthRange = qfalse;
    backEnd.currentEntity = &tr.worldEntity;
    backEnd.or = backEnd.viewParms.world;

    VK_SetViewViewport();

    /* The portal sky view and the main world view use different camera transforms.
       OpenGL clears depth before the main view; do the same in Vulkan.
       Portal/mirror views must keep the previous depth contents. */
    if (!backEnd.viewParms.isPortal) {
        VkClearAttachment attachment;
        VkClearRect rect;

        memset(&attachment, 0, sizeof(attachment));
        memset(&rect, 0, sizeof(rect));
        attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        attachment.clearValue.depthStencil.depth = 1.0f;
        rect.rect.offset.x = 0;
        rect.rect.offset.y = 0;
        rect.rect.extent = vk_state.swapExtent;
        rect.layerCount = 1;
        vkCmdClearAttachments(vk_state.cmdBuffers[vk_state.currentImageIndex], 1, &attachment, 1, &rect);
    }

    for (i = 0; i < numDrawSurfs; i++) {
        qboolean wantsDepthRange;

        R_DecomposeSort(drawSurfs[i].sort, &entityNum, &shader, &fogNum, &dlighted, &atiTess);

        if (!VK_ShouldDrawShader(shader)) {
            continue;
        }

        if (shader->remappedShader) {
            shader = shader->remappedShader;
        }

        if (entityNum != vk_oldEntityNum) {
            VK_SetupEntity(entityNum);
            vk_oldEntityNum = entityNum;

            wantsDepthRange = ( entityNum != ENTITYNUM_WORLD &&
                                ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) ) ? qtrue : qfalse;
            if (wantsDepthRange != vk_depthRange) {
                vk_depthRange = wantsDepthRange;
                VK_SetViewViewport();
            }
        }

        VK_DrawSurfaceStages(&drawSurfs[i], shader);
    }

    /* Restore world orientation so later per-view commands (sun, flares)
     * use the same modelview state as in the OpenGL path. */
    backEnd.currentEntity = &tr.worldEntity;
    backEnd.or = backEnd.viewParms.world;

    /* Restore global fog state so later commands / frames see the current
     * front-end values rather than the snapshot used for this view. */
    glfogNum = savedGlfogNum;
    glfogsettings[FOG_CURRENT] = savedCurrentFog;
}

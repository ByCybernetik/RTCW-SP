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

/* Mesh/MDS Surface* re-push materials; FogPass sets these so they preserve
 * volumetric fog params (mode 4 + fog vectors) instead of wiping them. */
int vk_volumetricFogPass;
vk_push_constants_t vk_fogPassPush;

static void VK_FillStagePushConstants(const shader_t *shader, vk_push_constants_t *pc) {
    /* The MVP comes from the per-view UBO slot computed once per entity; only
     * the 4-byte slot index and the stage parameters go through push
     * constants now. */
    VK_FillPushConstants(vk_currentMvpSlot, shader, pc);
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
    VK_CmdPushMaterial(cmd, &pc);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vk_state.pipelineLayout, 0, 1, &vk_state.fogDescSet, 0, NULL);

    /* Mesh/MDS draw paths call FillPushConstants again; keep a copy so they
     * can re-push without losing volumetric fog mode/vectors. */
    vk_fogPassPush = pc;
    vk_volumetricFogPass = 1;
    tess.numVertexes = 0;
    tess.numIndexes = 0;
    vk_surfaceTable[type](drawSurf->surface);
    vk_volumetricFogPass = 0;
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
            VK_UploadDlights();
        }
    } else {
        backEnd.currentEntity = &tr.worldEntity;
        backEnd.refdef.floatTime = vk_originalTime;
        backEnd.or = backEnd.viewParms.world;
        R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or);
        VK_UploadDlights();
    }

    /* One MVP slot per (view, entity): computed here once instead of per
     * surface per shader stage. */
    VK_ViewSetEntityMvp();
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

static void VK_FillDlightPushConstants(int dlightBits, vk_push_constants_t *pc) {
    /* Zero everything: leftover pad[1]/meshShortMode from the stack used to
     * enable the MD3 short path on world verts and scramble vWorldPos. */
    memset(pc, 0, sizeof(*pc));
    pc->mvpIndex = vk_currentMvpSlot;
    /* pad[0]/boneSet carries the surface light mask as a raw uint (safe;
     * floatBitsToUint via params[0] can be NaN-canonicalized by some GPUs). */
    pc->pad[0] = (uint32_t)dlightBits;

    pc->params[14][0] = backEnd.or.viewOrigin[0];
    pc->params[14][1] = backEnd.or.viewOrigin[1];
    pc->params[14][2] = backEnd.or.viewOrigin[2];
    pc->params[14][3] = 1.0f; /* dlight pass flag */
}

static void VK_SetDlightDepthBias(VkCommandBuffer cmd) {
    /* Must match the opaque pass depth that DEPTH_EQUAL tests against. */
    if (VK_MaterialPolygonOffset()) {
        vkCmdSetDepthBias(cmd, VK_MaterialPolyOffsetUnits(), 0.0f,
                          VK_MaterialPolyOffsetFactor());
    } else {
        vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);
    }
}

static void VK_ProjectDlightTexture(drawSurf_t *drawSurf, int dlightBits) {
    surfaceType_t *surf;
    surfaceType_t type;
    VkCommandBuffer cmd;
    vk_push_constants_t pc;

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
    VK_SetDlightDepthBias(cmd);

    {
        VkDescriptorSet descSet = VK_GetDescriptorSetForImages(tr.dlightImage, tr.whiteImage);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);
    }

    VK_FillDlightPushConstants(dlightBits, &pc);
    VK_CmdPushMaterial(cmd, &pc);

    tess.numVertexes = 0;
    tess.numIndexes = 0;
    vk_surfaceTable[type](surf);
}

/* ---------------------------------------------------------------------------
 * Draw-call batching for static world surfaces.
 *
 * Consecutive world surfaces with identical (shader, entity, fog) are merged
 * into one batch: their indices are re-based into the dynamic IBO while the
 * vertices stay in the static world VBO. A flush draws the whole batch with
 * one vkCmdDrawIndexed per shader stage instead of one per surface, matching
 * (and exceeding) the batching level of the OpenGL backend.
 * ------------------------------------------------------------------------- */

typedef struct {
    qboolean active;
    const shader_t *shader;
    int entityNum;
    int fogNum;
    int dlightBits;
    int iboStart;
    int numIdx;
} vk_world_batch_t;

static vk_world_batch_t vk_batch;

static void VK_BatchReset(void) {
    vk_batch.active = qfalse;
    vk_batch.shader = NULL;
    vk_batch.entityNum = -1;
    vk_batch.fogNum = 0;
    vk_batch.dlightBits = 0;
    vk_batch.iboStart = -1;
    vk_batch.numIdx = 0;
}

static qboolean VK_SurfaceIsBatchable(surfaceType_t type) {
    return type == SF_FACE || type == SF_GRID || type == SF_TRIANGLES;
}

static void VK_BatchAppendSurface(void *surfData) {
    const vk_world_surf_t *ws = VK_WorldGetStaticSurf(surfData);
    const uint32_t *cpuIndices = VK_WorldGetCpuIndices();
    int iboOff;
    uint32_t *dst;
    uint32_t j;

    if (!ws || !cpuIndices) {
        return;
    }

    if (!vk_batch.active) {
        vk_batch.active = qtrue;
        vk_batch.iboStart = -1;
        vk_batch.numIdx = 0;
        vk_batch.dlightBits = 0;
    }

    iboOff = VK_ReserveDynamicVBO(ws->indexCount * sizeof(uint32_t));
    if (iboOff < 0) {
        /* Dynamic arena exhausted: skip these indices (counted by the
         * reserve function). The batch stays contiguous because a failed
         * reserve does not advance the arena offset. */
        return;
    }
    if (vk_batch.iboStart < 0) {
        vk_batch.iboStart = iboOff;
    }

    /* Store absolute static-VBO vertex indices. The surfaces are sorted by
     * shader, not by VBO order, so re-basing against the first surface's
     * vertexOffset could produce negative deltas that wrap around as uint32
     * and read gigabytes past the buffer. With absolute indices the draw
     * simply uses vertexOffset = 0. */
    dst = (uint32_t *)((uint8_t *)vk_dyn.mapped + iboOff);
    for (j = 0; j < ws->indexCount; j++) {
        uint32_t idx = cpuIndices[ws->firstIndex + j] + ws->vertexOffset;
        if (idx >= vk_world.totalVerts) {
            static int corruptLogged = 0;
            if (corruptLogged < 5) {
                corruptLogged++;
                ri.Printf(PRINT_WARNING,
                          "VK_BatchAppendSurface: index %u out of range (totalVerts=%u, firstIndex=%u, vertexOffset=%d, j=%u)\n",
                          idx, vk_world.totalVerts, ws->firstIndex, ws->vertexOffset, j);
            }
            /* Rewind this reservation so the batch IBO stays contiguous. */
            vk_dyn.offset = iboOff;
            if (vk_batch.iboStart == iboOff) {
                vk_batch.iboStart = -1;
            }
            return;
        }
        dst[j] = idx;
    }
    vk_batch.numIdx += (int)ws->indexCount;
    vk_batch.dlightBits |= VK_SurfaceDlightBits((surfaceType_t *)surfData);
}

static void VK_BatchDrawGeometry(VkCommandBuffer cmd) {
    VkDeviceSize vboOffset = 0;

    if (!vk_world.vbo || vk_batch.numIdx <= 0 || vk_batch.iboStart < 0) {
        static int badDrawLogged = 0;
        if (badDrawLogged < 5) {
            badDrawLogged++;
            ri.Printf(PRINT_WARNING,
                      "VK_BatchDrawGeometry: skipped (vbo=%p numIdx=%d iboStart=%d)\n",
                      (void *)vk_world.vbo, vk_batch.numIdx, vk_batch.iboStart);
        }
        return;
    }

    VK_BindMeshVertexBuffers(cmd, vk_world.vbo, vboOffset, VK_NULL_HANDLE, 0);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)vk_batch.iboStart,
                         VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, (uint32_t)vk_batch.numIdx, 1, 0, 0, 0);
    vk_worldDrawCount++;
}

static void VK_FlushBatchDlights(VkCommandBuffer cmd) {
    vk_push_constants_t pc;

    if (!vk_batch.dlightBits || !backEnd.refdef.num_dlights) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      vk_state.pipelines[VK_PIPELINE_DLIGHT]);
    VK_SetDlightDepthBias(cmd);

    {
        VkDescriptorSet descSet = VK_GetDescriptorSetForImages(tr.dlightImage, tr.whiteImage);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);
    }

    VK_FillDlightPushConstants(vk_batch.dlightBits, &pc);
    VK_CmdPushMaterial(cmd, &pc);
    VK_BatchDrawGeometry(cmd);
}

static void VK_FlushBatchFog(VkCommandBuffer cmd) {
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

    fog = tr.world->fogs + tess.fogNum;

    pipelineIdx = (tess.shader->fogPass == FP_EQUAL) ? VK_PIPELINE_FOG_EQUAL : VK_PIPELINE_FOG;

    VK_FillStagePushConstants(tess.shader, &pc);

    color = fog->colorInt;
    pc.params[VK_FOG_COLOR_PARAM][0] = (float)(color & 0xFF) / 255.0f;
    pc.params[VK_FOG_COLOR_PARAM][1] = (float)((color >> 8) & 0xFF) / 255.0f;
    pc.params[VK_FOG_COLOR_PARAM][2] = (float)((color >> 16) & 0xFF) / 255.0f;
    pc.params[VK_FOG_COLOR_PARAM][3] = 4.0f;

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

    pc.params[0][2] = 0.0f;
    pc.params[7][0] = 0.0f;
    pc.params[7][1] = 0.0f;
    pc.params[7][2] = 1.0f;
    pc.params[7][3] = 0.0f;
    pc.params[11][2] = 0.0f;
    pc.params[13][0] = 0.0f;
    pc.params[13][1] = 0.0f;
    pc.params[13][2] = 0.0f;
    pc.params[13][3] = 0.0f;
    pc.params[14][3] = 0.0f;
    pc.params[15][3] = 0.0f;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipelineIdx]);
    vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);
    VK_CmdPushMaterial(cmd, &pc);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vk_state.pipelineLayout, 0, 1, &vk_state.fogDescSet, 0, NULL);

    VK_BatchDrawGeometry(cmd);
}

static void VK_FlushWorldBatch(VkCommandBuffer cmd) {
    const shader_t *shader;
    int pass;
    int stageIdx;
    int currentPipe;
    vk_push_constants_t pc;

    if (!vk_batch.active || vk_batch.numIdx <= 0) {
        VK_BatchReset();
        return;
    }

    shader = vk_batch.shader;

    tess.shader = (shader_t *)shader;
    tess.fogNum = vk_batch.fogNum;

    currentPipe = -1;

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

            if (shader->noFog && !stage->isFogged) {
                pc.params[VK_FOG_COLOR_PARAM][3] = 0.0f;
            }

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
            if (pipeIdx < 0 || pipeIdx >= VK_PIPELINE_COUNT || !vk_state.pipelines[pipeIdx]) {
                static int nullPipeLogged = 0;
                if (nullPipeLogged < 5) {
                    nullPipeLogged++;
                    ri.Printf(PRINT_WARNING, "VK_FlushWorldBatch: pipeline %d missing for shader '%s' stage %d\n",
                              pipeIdx, shader->name, stageIdx);
                }
                continue;
            }
            if (pipeIdx != currentPipe) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipeIdx]);
                currentPipe = pipeIdx;
            }

            if (VK_MaterialPolygonOffset()) {
                vkCmdSetDepthBias(cmd, VK_MaterialPolyOffsetUnits(), 0.0f, VK_MaterialPolyOffsetFactor());
            } else {
                vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);
            }

            VK_CmdPushMaterial(cmd, &pc);

            descSet = VK_StageDescriptorSet(shader, stage);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);

            VK_BatchDrawGeometry(cmd);
        }
    }

    if (shader->sort <= SS_OPAQUE &&
        !(shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY))) {
        VK_FlushBatchDlights(cmd);
    }

    if (r_vkVolumetricFog && r_vkVolumetricFog->integer && tess.fogNum && shader->fogPass) {
        VK_FlushBatchFog(cmd);
    }

    VK_BatchReset();
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

            VK_CmdPushMaterial(cmd, &pc);

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
    uint32_t flareMvpSlot = 0;
    typedef struct {
        flare_t *flare;
        int pipelineIdx;
        image_t *image;
        float x0, y0, x1, y1;
        byte color[4];
    } vk_flare_item_t;
    vk_flare_item_t items[128];
    int itemCount;
    int runStart;

    if (!cmd || !vk_state.renderPassActive) {
        return;
    }

    VK_Flush2DBatch();

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

    /* Ortho matrix that maps OpenGL-style screen pixels to Vulkan NDC,
     * shared by all flares in this call. */
    {
        float mvp[16];

        memset(mvp, 0, sizeof(mvp));
        mvp[0]  = 2.0f / (float)vpW;
        mvp[5]  = -2.0f / (float)vpH;
        mvp[10] = 1.0f;
        mvp[12] = -1.0f - 2.0f * (float)vpX / (float)vpW;
        mvp[13] = 1.0f + 2.0f * (float)vpYBottom / (float)vpH;
        mvp[15] = 1.0f;
        flareMvpSlot = VK_ViewAllocMvp(mvp);
    }

    /* Build draw list (screen quads + pipeline/image key). */
    itemCount = 0;
    for (f = r_activeFlares; f && itemCount < (int)(sizeof(items) / sizeof(items[0])); f = f->next) {
        shader_t *shader;
        shaderStage_t *stage;
        float size;
        vec3_t color;
        int iColor[3];
        int srcBlend, dstBlend;
        vk_flare_item_t *it;

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

        size = backEnd.viewParms.viewportWidth *
               ((r_flareSize->value * f->scale) / 640.0f + 8.0f / -f->eyeZ);

        it = &items[itemCount++];
        it->flare = f;
        it->x0 = f->windowXF - size;
        it->x1 = f->windowXF + size;
        it->y0 = f->windowYF - size;
        it->y1 = f->windowYF + size;
        it->color[0] = (byte)iColor[0];
        it->color[1] = (byte)iColor[1];
        it->color[2] = (byte)iColor[2];
        it->color[3] = (byte)(f->drawIntensity * 255.0f);
        it->image = VK_BundleImage(&stage->bundle[0], shader);
        if (!it->image) {
            it->image = tr.whiteImage;
        }

        srcBlend = stage->stateBits & GLS_SRCBLEND_BITS;
        dstBlend = stage->stateBits & GLS_DSTBLEND_BITS;
        if (srcBlend == GLS_SRCBLEND_SRC_ALPHA && dstBlend == GLS_DSTBLEND_ONE) {
            it->pipelineIdx = VK_PIPELINE_2D_SRC_ALPHA_ONE;
        } else if (dstBlend == GLS_DSTBLEND_ONE) {
            it->pipelineIdx = VK_PIPELINE_2D_ADDITIVE;
        } else {
            it->pipelineIdx = VK_PIPELINE_2D;
        }
    }

    /* Sort by (pipeline, image) so consecutive flares share one draw. */
    {
        int a, b;
        for (a = 0; a < itemCount - 1; a++) {
            for (b = a + 1; b < itemCount; b++) {
                if (items[b].pipelineIdx < items[a].pipelineIdx ||
                    (items[b].pipelineIdx == items[a].pipelineIdx &&
                     items[b].image < items[a].image)) {
                    vk_flare_item_t tmp = items[a];
                    items[a] = items[b];
                    items[b] = tmp;
                }
            }
        }
    }

    runStart = 0;
    while (runStart < itemCount) {
        int runEnd = runStart + 1;
        int nQuads;
        int vboSize, iboSize, vboOff, iboOff;
        drawVert_t *verts;
        int *idx;
        VkDeviceSize offsets[1];
        VkDescriptorSet descSet;
        int q;

        while (runEnd < itemCount &&
               items[runEnd].pipelineIdx == items[runStart].pipelineIdx &&
               items[runEnd].image == items[runStart].image) {
            runEnd++;
        }

        nQuads = runEnd - runStart;
        vboSize = nQuads * 4 * (int)sizeof(drawVert_t);
        iboSize = nQuads * 6 * (int)sizeof(int);
        vboOff = VK_ReserveDynamicVBO(vboSize + iboSize);
        if (vboOff < 0) {
            runStart = runEnd;
            continue;
        }
        iboOff = vboOff + vboSize;

        verts = (drawVert_t *)((uint8_t *)vk_dyn.mapped + vboOff);
        idx = (int *)((uint8_t *)vk_dyn.mapped + iboOff);

        for (q = 0; q < nQuads; q++) {
            const vk_flare_item_t *it = &items[runStart + q];
            drawVert_t *v = verts + q * 4;
            int *ii = idx + q * 6;
            int base = q * 4;
            int c;

            v[0].xyz[0] = it->x0; v[0].xyz[1] = it->y0; v[0].xyz[2] = 0.0f;
            v[0].st[0] = 0.0f; v[0].st[1] = 0.0f;
            v[1].xyz[0] = it->x0; v[1].xyz[1] = it->y1; v[1].xyz[2] = 0.0f;
            v[1].st[0] = 0.0f; v[1].st[1] = 1.0f;
            v[2].xyz[0] = it->x1; v[2].xyz[1] = it->y1; v[2].xyz[2] = 0.0f;
            v[2].st[0] = 1.0f; v[2].st[1] = 1.0f;
            v[3].xyz[0] = it->x1; v[3].xyz[1] = it->y0; v[3].xyz[2] = 0.0f;
            v[3].st[0] = 1.0f; v[3].st[1] = 0.0f;

            for (c = 0; c < 4; c++) {
                v[c].lightmap[0] = 0.0f;
                v[c].lightmap[1] = 0.0f;
                VectorClear(v[c].normal);
                v[c].color[0] = it->color[0];
                v[c].color[1] = it->color[1];
                v[c].color[2] = it->color[2];
                v[c].color[3] = it->color[3];
            }

            ii[0] = base + 0; ii[1] = base + 1; ii[2] = base + 2;
            ii[3] = base + 0; ii[4] = base + 2; ii[5] = base + 3;
        }

        descSet = VK_GetDescriptorSetForImage(items[runStart].image);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          vk_state.pipelines[items[runStart].pipelineIdx]);
        offsets[0] = (VkDeviceSize)vboOff;
        VK_BindMeshVertexBuffers(cmd, vk_dyn.buffer, offsets[0], VK_NULL_HANDLE, 0);
        vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
        vkCmdPushConstants(cmd, vk_state.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(flareMvpSlot), &flareMvpSlot);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);
        vkCmdDrawIndexed(cmd, (uint32_t)(nQuads * 6), 1, 0, 0, 0);

        runStart = runEnd;
    }

    tess = savedTess;
}

static void VK_SwitchEntity(int entityNum) {
    qboolean wantsDepthRange;

    VK_SetupEntity(entityNum);
    vk_oldEntityNum = entityNum;

    wantsDepthRange = ( entityNum != ENTITYNUM_WORLD &&
                        ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) ) ? qtrue : qfalse;
    if (wantsDepthRange != vk_depthRange) {
        vk_depthRange = wantsDepthRange;
        VK_SetViewViewport();
    }
}

static void VK_ClearViewDepth(VkCommandBuffer cmd) {
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
    vkCmdClearAttachments(cmd, 1, &attachment, 1, &rect);
}

/* Match OpenGL RB_BeginDrawingView color clears. When fog uses clearscreen /
 * !drawsky the sky is skipped — without this the offscreen clear stays black
 * and fogsky looks like a hard black void above the fog line. */
static void VK_ClearViewFogSky(VkCommandBuffer cmd) {
    const glfog_t *fog;
    VkClearAttachment attachment;
    VkClearRect rect;
    float color[4];
    qboolean needClear;

    if (r_uiFullScreen && r_uiFullScreen->integer) {
        return;
    }
    if (backEnd.refdef.rdflags & RDF_NOWORLDMODEL) {
        return;
    }

    fog = NULL;
    if (backEnd.viewParms.glFog.registered) {
        fog = &backEnd.viewParms.glFog;
    } else if (glfogNum > FOG_NONE && glfogsettings[FOG_CURRENT].registered) {
        fog = &glfogsettings[FOG_CURRENT];
    }

    color[0] = 0.5f;
    color[1] = 0.5f;
    color[2] = 0.5f;
    color[3] = 1.0f;
    needClear = qfalse;

    if (r_fastsky && r_fastsky->integer) {
        needClear = qtrue;
        if (fog) {
            color[0] = fog->color[0];
            color[1] = fog->color[1];
            color[2] = fog->color[2];
            color[3] = fog->color[3];
        }
    } else if (fog && (fog->clearscreen || !fog->drawsky)) {
        needClear = qtrue;
        color[0] = fog->color[0];
        color[1] = fog->color[1];
        color[2] = fog->color[2];
        color[3] = fog->color[3];
    } else if (skyboxportal && !(backEnd.refdef.rdflags & RDF_SKYBOXPORTAL)
               && fog && r_portalsky && !r_portalsky->integer) {
        needClear = qtrue;
        color[0] = fog->color[0];
        color[1] = fog->color[1];
        color[2] = fog->color[2];
        color[3] = fog->color[3];
    }

    if (!needClear) {
        return;
    }

    memset(&attachment, 0, sizeof(attachment));
    memset(&rect, 0, sizeof(rect));
    attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachment.clearValue.color.float32[0] = color[0];
    attachment.clearValue.color.float32[1] = color[1];
    attachment.clearValue.color.float32[2] = color[2];
    attachment.clearValue.color.float32[3] = color[3];
    rect.rect.offset.x = 0;
    rect.rect.offset.y = 0;
    rect.rect.extent = vk_state.swapExtent;
    rect.layerCount = 1;
    vkCmdClearAttachments(cmd, 1, &attachment, 1, &rect);
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
    VkCommandBuffer cmd;

    /* backEnd.viewParms was set by RB_DrawSurfs for THIS scene: compute the
     * view projection and the world-entity MVP slot now, on the back end,
     * where they are correct even with multiple scenes per frame. */
    VK_ViewBegin();

    if (!vk_state.renderPassActive || numDrawSurfs <= 0) {
        return;
    }

    VK_Flush2DBatch();

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];

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
    VK_BatchReset();

    VK_SetViewViewport();

    /* The portal sky view and the main world view use different camera transforms.
       OpenGL clears depth before the main view; do the same in Vulkan.
       Portal/mirror views must keep the previous depth contents. */
    if (!backEnd.viewParms.isPortal) {
        VK_ClearViewDepth(cmd);
        VK_ClearViewFogSky(cmd);
    }

    for (i = 0; i < numDrawSurfs; i++) {
        surfaceType_t surfType;

        R_DecomposeSort(drawSurfs[i].sort, &entityNum, &shader, &fogNum, &dlighted, &atiTess);

        if (!VK_ShouldDrawShader(shader)) {
            continue;
        }

        if (shader->remappedShader) {
            shader = shader->remappedShader;
        }

        /* Match GL skyboxportal logic: main view skips sky, portal view draws
         * only sky (unless drawskyboxportal). */
        if (skyboxportal) {
            if (!(backEnd.refdef.rdflags & RDF_SKYBOXPORTAL)) {
                if (shader->isSky) {
                    continue;
                }
            } else {
                if (!drawskyboxportal && !shader->isSky) {
                    continue;
                }
            }
        }

        surfType = drawSurfs[i].surface ? *drawSurfs[i].surface : SF_BAD;

        /* Sky, CPU-deform shaders, and non-static types use the per-surface path. */
        if (shader->isSky || VK_ShaderNeedsCpuDeform(shader) ||
            !VK_SurfaceIsBatchable(surfType)) {
            VK_FlushWorldBatch(cmd);
            if (entityNum != vk_oldEntityNum) {
                VK_SwitchEntity(entityNum);
            }
            VK_DrawSurfaceStages(&drawSurfs[i], shader);
            continue;
        }

        /* Batch key change: flush before switching entity/fog state. */
        if (vk_batch.active &&
            (vk_batch.shader != shader || vk_batch.entityNum != entityNum ||
             vk_batch.fogNum != fogNum)) {
            VK_FlushWorldBatch(cmd);
        }

        if (entityNum != vk_oldEntityNum) {
            VK_SwitchEntity(entityNum);
        }

        if (!vk_batch.active) {
            vk_batch.shader = shader;
            vk_batch.entityNum = entityNum;
            vk_batch.fogNum = fogNum;
        }

        if (VK_WorldGetStaticSurf(drawSurfs[i].surface)) {
            VK_BatchAppendSurface(drawSurfs[i].surface);
        } else {
            /* Surface is missing from the static world buffers (should not
             * happen for SF_FACE/GRID/TRIANGLES): per-surface fallback. */
            VK_FlushWorldBatch(cmd);
            VK_DrawSurfaceStages(&drawSurfs[i], shader);
        }
    }

    VK_FlushWorldBatch(cmd);

    /* Restore world orientation so later per-view commands (sun, flares)
     * use the same modelview state as in the OpenGL path. */
    backEnd.currentEntity = &tr.worldEntity;
    backEnd.or = backEnd.viewParms.world;

    /* Restore global fog state so later commands / frames see the current
     * front-end values rather than the snapshot used for this view. */
    glfogNum = savedGlfogNum;
    glfogsettings[FOG_CURRENT] = savedCurrentFog;
}

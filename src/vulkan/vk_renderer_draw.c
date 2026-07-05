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
}

void VK_DrawSurfList(drawSurf_t *drawSurfs, int numDrawSurfs) {
    shader_t *shader;
    int fogNum;
    int entityNum;
    int dlighted;
    int atiTess;
    int i;

    if (!vk_state.renderPassActive || numDrawSurfs <= 0) {
        return;
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

}

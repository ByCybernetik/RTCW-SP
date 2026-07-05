#include "vk_local.h"
#include "vk_material.h"
#include <string.h>

extern void (*vk_surfaceTable[SF_NUM_SURFACE_TYPES])(void *);

void VK_BeginFrame(stereoFrame_t stereoFrame) {
    (void)stereoFrame;

    vk_state.renderPassActive = qfalse;

    vkWaitForFences(vk_state.dev, 1, &vk_state.inFlight[vk_state.frameIndex],
                    VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult res = vkAcquireNextImageKHR(vk_state.dev, vk_state.swapchain,
                                          UINT64_MAX,
                                          vk_state.imageAvailable[vk_state.frameIndex],
                                          VK_NULL_HANDLE, &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        VK_UpdateSwapchain(vk_state.swapExtent.width, vk_state.swapExtent.height);
        res = vkAcquireNextImageKHR(vk_state.dev, vk_state.swapchain,
                                    UINT64_MAX,
                                    vk_state.imageAvailable[vk_state.frameIndex],
                                    VK_NULL_HANDLE, &imageIndex);
    }
    vk_state.currentImageIndex = imageIndex;

    vkResetFences(vk_state.dev, 1, &vk_state.inFlight[vk_state.frameIndex]);

    VkCommandBuffer cmd = vk_state.cmdBuffers[imageIndex];
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

    if (!cmd || !vk_state.framebuffers[imageIndex]) {
        return;
    }

    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkResetCommandBuffer(cmd, 0) != VK_SUCCESS ||
        vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) {
        return;
    }

    VkClearValue clears[2];

    /* Match GL clear color: current fog color when registered, otherwise neutral gray. */
    if (glfogsettings[FOG_CURRENT].registered) {
        clears[0].color.float32[0] = glfogsettings[FOG_CURRENT].color[0];
        clears[0].color.float32[1] = glfogsettings[FOG_CURRENT].color[1];
        clears[0].color.float32[2] = glfogsettings[FOG_CURRENT].color[2];
        clears[0].color.float32[3] = glfogsettings[FOG_CURRENT].color[3];
    } else {
        clears[0].color.float32[0] = 0.5f;
        clears[0].color.float32[1] = 0.5f;
        clears[0].color.float32[2] = 0.5f;
        clears[0].color.float32[3] = 1.0f;
    }
    clears[1].depthStencil.depth = 1.0f;
    clears[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo rpbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rpbi.renderPass = vk_state.renderPass;
    rpbi.framebuffer = vk_state.framebuffers[imageIndex];
    rpbi.renderArea.extent = vk_state.swapExtent;
    rpbi.clearValueCount = 2;
    rpbi.pClearValues = clears;

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    vk_state.renderPassActive = qtrue;

    {
        VkViewport vp;
        VkRect2D scissor;

        vp.x = 0.0f;
        vp.y = 0.0f;
        vp.width = (float)vk_state.swapExtent.width;
        vp.height = (float)vk_state.swapExtent.height;
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent = vk_state.swapExtent;
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

    VK_ResetDynamicVBO();
}

void VK_EndFrame(int *frontEndMsec, int *backEndMsec) {
    VkCommandBuffer cmd;

    if (!vk_state.renderPassActive) {
        return;
    }

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    if (!cmd) {
        vk_state.renderPassActive = qfalse;
        return;
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &vk_state.imageAvailable[vk_state.frameIndex];
    si.pWaitDstStageMask = &waitStages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &vk_state.renderFinished[vk_state.frameIndex];
    vkQueueSubmit(vk_state.gfxQueue, 1, &si, vk_state.inFlight[vk_state.frameIndex]);

    VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &vk_state.renderFinished[vk_state.frameIndex];
    pi.swapchainCount = 1;
    pi.pSwapchains = &vk_state.swapchain;
    pi.pImageIndices = &vk_state.currentImageIndex;
    vkQueuePresentKHR(vk_state.presentQueue, &pi);

    vk_state.renderPassActive = qfalse;
    vk_state.frameIndex = (vk_state.frameIndex + 1) % VK_MAX_FRAMES_IN_FLIGHT;

    if (frontEndMsec) *frontEndMsec = 0;
    if (backEndMsec) *backEndMsec = 0;
}

void VK_RenderView(viewParms_t *parms) {
    int firstDrawSurf;

    if (parms->viewportWidth <= 0 || parms->viewportHeight <= 0) {
        return;
    }

    tr.viewCount++;

    tr.viewParms = *parms;
    tr.viewParms.frameSceneNum = tr.frameSceneNum;
    tr.viewParms.frameCount = tr.frameCount;

    firstDrawSurf = tr.refdef.numDrawSurfs;

    tr.viewCount++;

    R_RotateForViewer();
    R_SetupFrustum();
    R_GenerateDrawSurfs();
    R_SortDrawSurfs(tr.refdef.drawSurfs + firstDrawSurf,
                    tr.refdef.numDrawSurfs - firstDrawSurf);
}

static float vk_2dColor[4] = { 1, 1, 1, 1 };

void VK_SetColor(const float *rgba) {
    if (rgba) {
        vk_2dColor[0] = rgba[0];
        vk_2dColor[1] = rgba[1];
        vk_2dColor[2] = rgba[2];
        vk_2dColor[3] = rgba[3];
    }
}

static void VK_Set2DMVP(float *mvp, int width, int height) {
    memset(mvp, 0, sizeof(float) * 16);
    /* Quake 2D: y=0 at top; Vulkan NDC: y=-1 at top, y=+1 at bottom */
    mvp[0] = 2.0f / (float)width;
    mvp[5] = 2.0f / (float)height;
    mvp[10] = 1.0f;
    mvp[12] = -1.0f;
    mvp[13] = -1.0f;
    mvp[15] = 1.0f;
}

static void VK_SetPicViewport(VkCommandBuffer cmd) {
    int width = vk_state.swapExtent.width;
    int height = vk_state.swapExtent.height;
    VkViewport vp;
    VkRect2D scissor;

    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width = (float)width;
    vp.height = (float)height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = (uint32_t)width;
    scissor.extent.height = (uint32_t)height;
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

static float VK_PicShaderTime(const shader_t *shader) {
    float shaderTime = backEnd.refdef.floatTime;

    if (shader) {
        shaderTime -= shader->timeOffset;
        if (shader->clampTime && shaderTime >= shader->clampTime) {
            shaderTime = shader->clampTime;
        }
    }

    return shaderTime;
}

static void VK_ResetPicTexCoords(drawVert_t *verts, float s1, float t1, float s2, float t2) {
    verts[0].st[0] = s1; verts[0].st[1] = t1;
    verts[1].st[0] = s2; verts[1].st[1] = t1;
    verts[2].st[0] = s2; verts[2].st[1] = t2;
    verts[3].st[0] = s1; verts[3].st[1] = t2;
}

static void VK_ApplyPicTexMods(const shader_t *shader, const shaderStage_t *stage,
                               drawVert_t *verts) {
    shaderCommands_t savedTess;
    int i;
    int tm;

    if (!stage || stage->bundle[0].numTexMods <= 0) {
        return;
    }

    savedTess = tess;
    Com_Memset(&tess, 0, sizeof(tess));
    tess.numVertexes = 4;
    tess.shader = (shader_t *)shader;
    tess.shaderTime = VK_PicShaderTime(shader);

    for (i = 0; i < 4; i++) {
        VectorCopy(verts[i].xyz, tess.xyz[i]);
        tess.xyz[i][3] = 0.0f;
        VectorClear(tess.normal[i]);
        tess.texCoords[i][0][0] = verts[i].st[0];
        tess.texCoords[i][0][1] = verts[i].st[1];
        tess.svars.texcoords[0][i][0] = verts[i].st[0];
        tess.svars.texcoords[0][i][1] = verts[i].st[1];
    }

    for (tm = 0; tm < stage->bundle[0].numTexMods; tm++) {
        switch (stage->bundle[0].texMods[tm].type) {
        case TMOD_NONE:
            tm = TR_MAX_TEXMODS;
            break;
        case TMOD_SWAP:
            RB_CalcSwapTexCoords((float *)tess.svars.texcoords[0]);
            break;
        case TMOD_TURBULENT:
            RB_CalcTurbulentTexCoords(&stage->bundle[0].texMods[tm].wave,
                                      (float *)tess.svars.texcoords[0]);
            break;
        case TMOD_ENTITY_TRANSLATE:
            RB_CalcScrollTexCoords(backEnd.currentEntity->e.shaderTexCoord,
                                   (float *)tess.svars.texcoords[0]);
            break;
        case TMOD_SCROLL:
            RB_CalcScrollTexCoords(stage->bundle[0].texMods[tm].scroll,
                                   (float *)tess.svars.texcoords[0]);
            break;
        case TMOD_SCALE:
            RB_CalcScaleTexCoords(stage->bundle[0].texMods[tm].scale,
                                  (float *)tess.svars.texcoords[0]);
            break;
        case TMOD_STRETCH:
            RB_CalcStretchTexCoords(&stage->bundle[0].texMods[tm].wave,
                                    (float *)tess.svars.texcoords[0]);
            break;
        case TMOD_TRANSFORM:
            RB_CalcTransformTexCoords(&stage->bundle[0].texMods[tm],
                                      (float *)tess.svars.texcoords[0]);
            break;
        case TMOD_ROTATE:
            RB_CalcRotateTexCoords(stage->bundle[0].texMods[tm].rotateSpeed,
                                   (float *)tess.svars.texcoords[0]);
            break;
        default:
            break;
        }
    }

    for (i = 0; i < 4; i++) {
        verts[i].st[0] = tess.svars.texcoords[0][i][0];
        verts[i].st[1] = tess.svars.texcoords[0][i][1];
    }

    tess = savedTess;
}

static qboolean VK_UIStageUses2DPipeline(const shader_t *shader, const shaderStage_t *stage) {
    if (!stage) {
        return qtrue;
    }

    if (shader && shader->numDeforms > 0) {
        return qfalse;
    }

    if (stage->bundle[0].tcGen == TCGEN_ENVIRONMENT_MAPPED ||
        stage->bundle[0].tcGen == TCGEN_FIRERISEENV_MAPPED) {
        return qfalse;
    }

    if (stage->bundle[0].isVideoMap) {
        return qfalse;
    }

    return qtrue;
}

static void VK_DrawPicQuad(float x, float y, float w, float h,
                           float s1, float t1, float s2, float t2,
                           shader_t *shader,
                           const byte color0[4], const byte color1[4],
                           const byte color2[4], const byte color3[4]) {
    VkCommandBuffer cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    shader_t *state;
    float mvp[16];
    vk_push_constants_t pc;
    int vboSize;
    int iboSize;
    int iboOff;
    int stageIdx;
    int currentPipe;
    int *idx;
    drawVert_t baseVerts[4];
    drawVert_t stageVerts[4];
    int stageVboOff;
    int corner;
    VkDeviceSize offsets[1];
    byte stageColors[4][4];

    if (!cmd || !vk_state.renderPassActive || !shader) {
        return;
    }

    state = shader->remappedShader ? shader->remappedShader : shader;
    if (!state->stages[0]) {
        return;
    }

    VK_SetPicViewport(cmd);
    VK_Set2DMVP(mvp, vk_state.swapExtent.width, vk_state.swapExtent.height);

    vboSize = 4 * (int)sizeof(drawVert_t);
    iboSize = 6 * (int)sizeof(int);

    baseVerts[0].xyz[0] = x;     baseVerts[0].xyz[1] = y;     baseVerts[0].xyz[2] = 0;
    baseVerts[0].st[0] = s1;     baseVerts[0].st[1] = t1;
    baseVerts[1].xyz[0] = x + w; baseVerts[1].xyz[1] = y;     baseVerts[1].xyz[2] = 0;
    baseVerts[1].st[0] = s2;     baseVerts[1].st[1] = t1;
    baseVerts[2].xyz[0] = x + w; baseVerts[2].xyz[1] = y + h; baseVerts[2].xyz[2] = 0;
    baseVerts[2].st[0] = s2;     baseVerts[2].st[1] = t2;
    baseVerts[3].xyz[0] = x;     baseVerts[3].xyz[1] = y + h; baseVerts[3].xyz[2] = 0;
    baseVerts[3].st[0] = s1;     baseVerts[3].st[1] = t2;

    for (corner = 0; corner < 4; corner++) {
        baseVerts[corner].lightmap[0] = 0;
        baseVerts[corner].lightmap[1] = 0;
        VectorClear(baseVerts[corner].normal);
    }

    iboOff = VK_ReserveDynamicVBO(iboSize);
    if (iboOff < 0) {
        return;
    }
    idx = (int *)((uint8_t *)vk_dyn.mapped + iboOff);
    idx[0] = 0; idx[1] = 1; idx[2] = 2;
    idx[3] = 0; idx[4] = 2; idx[5] = 3;
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);

    currentPipe = -1;
    for (stageIdx = 0; stageIdx < MAX_SHADER_STAGES && state->stages[stageIdx]; stageIdx++) {
        shaderStage_t *stage = state->stages[stageIdx];
        int pipeIdx;
        VkDescriptorSet descSet;
        qboolean use2D;

        if (!stage->active) {
            continue;
        }
        if (stage->bundle[0].isLightmap) {
            continue;
        }
        if (!stage->bundle[0].image[0] && !stage->bundle[0].isVideoMap) {
            continue;
        }

        VK_ResetPicTexCoords(stageVerts, s1, t1, s2, t2);
        for (corner = 0; corner < 4; corner++) {
            VectorCopy(baseVerts[corner].xyz, stageVerts[corner].xyz);
            stageVerts[corner].xyz[2] = 0;
            stageVerts[corner].lightmap[0] = 0;
            stageVerts[corner].lightmap[1] = 0;
            VectorClear(stageVerts[corner].normal);
        }
        VK_ApplyPicTexMods(state, stage, stageVerts);

        VK_FillPicStageColors(state, stage, color0, color1, color2, color3, stageColors);

        for (corner = 0; corner < 4; corner++) {
            stageVerts[corner].color[0] = stageColors[corner][0];
            stageVerts[corner].color[1] = stageColors[corner][1];
            stageVerts[corner].color[2] = stageColors[corner][2];
            stageVerts[corner].color[3] = stageColors[corner][3];
        }

        stageVboOff = VK_ReserveDynamicVBO(vboSize);
        if (stageVboOff < 0) {
            continue;
        }
        Com_Memcpy((uint8_t *)vk_dyn.mapped + stageVboOff, stageVerts, vboSize);
        offsets[0] = (VkDeviceSize)stageVboOff;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vk_dyn.buffer, offsets);

        use2D = VK_UIStageUses2DPipeline(state, stage);
        if (use2D) {
            image_t *img = VK_BundleImage(&stage->bundle[0], state);

            pipeIdx = VK_PipelineFor2DPic(stage);
            if (currentPipe != pipeIdx) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipeIdx]);
                currentPipe = pipeIdx;
            }

            vkCmdPushConstants(cmd, vk_state.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(mvp), mvp);

            descSet = VK_GetDescriptorSetForImage(img ? img : tr.whiteImage);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);
            vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
            continue;
        }

        VK_SetUIStageStateFromShader(state, stage);
        VK_FillPushConstants(mvp, state, &pc);
        pc.params[14][3] = 0.2f;

        pipeIdx = VK_PipelineForUIStage(stage);
        if (pipeIdx != currentPipe) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipeIdx]);
            currentPipe = pipeIdx;
        }

        vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);
        vkCmdPushConstants(cmd, vk_state.pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(pc), &pc);

        descSet = VK_StageDescriptorSet(state, stage);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_state.pipelineLayout, 0, 1,
                                &descSet, 0, NULL);
        vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
    }
}

void VK_StretchPic(float x, float y, float w, float h,
                   float s1, float t1, float s2, float t2, shader_t *shader) {
    byte color[4];

    color[0] = (byte)(vk_2dColor[0] * 255.0f);
    color[1] = (byte)(vk_2dColor[1] * 255.0f);
    color[2] = (byte)(vk_2dColor[2] * 255.0f);
    color[3] = (byte)(vk_2dColor[3] * 255.0f);

    VK_DrawPicQuad(x, y, w, h, s1, t1, s2, t2, shader,
                   color, color, color, color);
}

void VK_StretchPicGradient(float x, float y, float w, float h,
                           float s1, float t1, float s2, float t2, shader_t *shader,
                           const byte gradientColor[4]) {
    byte top[4];
    byte bottom[4];

    top[0] = (byte)(vk_2dColor[0] * 255.0f);
    top[1] = (byte)(vk_2dColor[1] * 255.0f);
    top[2] = (byte)(vk_2dColor[2] * 255.0f);
    top[3] = (byte)(vk_2dColor[3] * 255.0f);

    if (gradientColor) {
        bottom[0] = gradientColor[0];
        bottom[1] = gradientColor[1];
        bottom[2] = gradientColor[2];
        bottom[3] = gradientColor[3];
    } else {
        bottom[0] = top[0];
        bottom[1] = top[1];
        bottom[2] = top[2];
        bottom[3] = top[3];
    }

    VK_DrawPicQuad(x, y, w, h, s1, t1, s2, t2, shader,
                   top, top, bottom, bottom);
}

void VK_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, int client) {
    VkCommandBuffer cmd;
    image_t *image;
    float mvp[16];
    float s1, t1, s2, t2;
    int vboSize;
    int iboSize;
    int vboOff;
    int iboOff;
    drawVert_t *verts;
    int *idx;
    VkDeviceSize offsets[1];
    VkDescriptorSet descSet;
    byte colorByte;
    int i;

    if (client < 0 || client >= 32 || cols <= 0 || rows <= 0) {
        return;
    }

    image = tr.scratchImage[client];
    if (!image || !vk_state.renderPassActive) {
        return;
    }

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    if (!cmd) {
        return;
    }

    s1 = 0.5f / (float)cols;
    t1 = 0.5f / (float)rows;
    s2 = ((float)cols - 0.5f) / (float)cols;
    t2 = ((float)rows - 0.5f) / (float)rows;

    VK_SetPicViewport(cmd);
    VK_Set2DMVP(mvp, vk_state.swapExtent.width, vk_state.swapExtent.height);

    vboSize = 4 * (int)sizeof(drawVert_t);
    iboSize = 6 * (int)sizeof(int);
    vboOff = VK_ReserveDynamicVBO(vboSize + iboSize);
    if (vboOff < 0) {
        return;
    }
    iboOff = vboOff + vboSize;

    verts = (drawVert_t *)((uint8_t *)vk_dyn.mapped + vboOff);
    idx = (int *)((uint8_t *)vk_dyn.mapped + iboOff);

    verts[0].xyz[0] = (float)x;         verts[0].xyz[1] = (float)y;         verts[0].xyz[2] = 0;
    verts[0].st[0] = s1;                verts[0].st[1] = t1;
    verts[1].xyz[0] = (float)(x + w);   verts[1].xyz[1] = (float)y;         verts[1].xyz[2] = 0;
    verts[1].st[0] = s2;                verts[1].st[1] = t1;
    verts[2].xyz[0] = (float)(x + w);   verts[2].xyz[1] = (float)(y + h);   verts[2].xyz[2] = 0;
    verts[2].st[0] = s2;                verts[2].st[1] = t2;
    verts[3].xyz[0] = (float)x;         verts[3].xyz[1] = (float)(y + h);   verts[3].xyz[2] = 0;
    verts[3].st[0] = s1;                verts[3].st[1] = t2;

    colorByte = (byte)(tr.identityLight * 255.0f);
    for (i = 0; i < 4; i++) {
        verts[i].color[0] = colorByte;
        verts[i].color[1] = colorByte;
        verts[i].color[2] = colorByte;
        verts[i].color[3] = 255;
        verts[i].lightmap[0] = 0;
        verts[i].lightmap[1] = 0;
        VectorClear(verts[i].normal);
    }

    idx[0] = 0; idx[1] = 1; idx[2] = 2;
    idx[3] = 0; idx[4] = 2; idx[5] = 3;

    offsets[0] = (VkDeviceSize)vboOff;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[VK_PIPELINE_2D]);
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_dyn.buffer, offsets);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(cmd, vk_state.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp), mvp);

    descSet = VK_GetDescriptorSetForImage(image);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}

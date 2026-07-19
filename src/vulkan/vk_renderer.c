#include "vk_local.h"
#include "vk_material.h"
#include <string.h>

extern void (*vk_surfaceTable[SF_NUM_SURFACE_TYPES])(void *);

#define VK_2D_BATCH_MAX 256

static drawVert_t vk_2dBatchVerts[VK_2D_BATCH_MAX * 4];
static int vk_2dBatchCount;
static int vk_2dBatchPipe = -1;
static VkDescriptorSet vk_2dBatchDesc = VK_NULL_HANDLE;
static uint32_t vk_2dBatchMvp;

static void VK_CmdImageBarrier(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                               VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

void VK_SetHoldRestore(qboolean enable) {
    vk_state.colorPreserve = enable;
}

/* Offscreen starts COLOR_ATTACHMENT_OPTIMAL. Clear every frame unless
 * colorPreserve (map load) — LOAD then keeps the last scene and avoids flash.
 * Clearing in the menu/gameplay prevents notify/console trails. */
static void VK_PrepareOffscreenColor(VkCommandBuffer cmd) {
    VkImage color = vk_state.colorImage[vk_state.frameIndex];
    VkImageSubresourceRange range;
    VkClearColorValue clearColor;
    qboolean valid;

    if (!color) {
        return;
    }

    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    valid = vk_state.colorValid[vk_state.frameIndex];
    if (vk_state.colorPreserve && valid) {
        return;
    }

    clearColor.float32[0] = 0.0f;
    clearColor.float32[1] = 0.0f;
    clearColor.float32[2] = 0.0f;
    clearColor.float32[3] = 1.0f;

    if (valid) {
        VK_CmdImageBarrier(cmd, color,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                           VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    } else {
        VK_CmdImageBarrier(cmd, color,
                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           0, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    vkCmdClearColorImage(cmd, color, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
    VK_CmdImageBarrier(cmd, color,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}

/* Blit finished offscreen color into the acquired swapchain image for present. */
static void VK_BlitOffscreenToSwapchain(VkCommandBuffer cmd) {
    VkImage color = vk_state.colorImage[vk_state.frameIndex];
    VkImage swap = vk_state.swapImages[vk_state.currentImageIndex];
    VkImageCopy copy;

    if (!color || !swap) {
        return;
    }

    VK_CmdImageBarrier(cmd, color,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VK_CmdImageBarrier(cmd, swap,
                       VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       0, VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    memset(&copy, 0, sizeof(copy));
    copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.srcSubresource.layerCount = 1;
    copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.dstSubresource.layerCount = 1;
    copy.extent.width = vk_state.swapExtent.width;
    copy.extent.height = vk_state.swapExtent.height;
    copy.extent.depth = 1;
    vkCmdCopyImage(cmd, color, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swap, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VK_CmdImageBarrier(cmd, swap,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                       VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    /* Return offscreen to COLOR_ATTACHMENT for the next LOAD on this slot. */
    VK_CmdImageBarrier(cmd, color,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_ACCESS_TRANSFER_READ_BIT,
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    vk_state.colorValid[vk_state.frameIndex] = qtrue;
}

void VK_BeginFrame(stereoFrame_t stereoFrame) {
    (void)stereoFrame;

    vk_state.renderPassActive = qfalse;

    /* Do not wait forever: if a previous frame failed to signal the fence,
     * skip this frame instead of deadlocking the renderer. */
    VkResult res = vkWaitForFences(vk_state.dev, 1, &vk_state.inFlight[vk_state.frameIndex],
                                    VK_TRUE, 100000000); /* 100 ms */
    if (res == VK_TIMEOUT) {
        ri.Printf(PRINT_WARNING, "VK_BeginFrame: in-flight fence timed out, skipping frame\n");
        return;
    }
    if (res != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "VK_BeginFrame: vkWaitForFences failed (%d), skipping frame\n", res);
        return;
    }

    /* The frame that last used this slot has completed: safe to recycle its
     * upload staging buffers and per-view MVP slots. */
    VK_ReapUploadSlot(vk_state.frameIndex);
    VK_ViewFrameBegin();

    uint32_t imageIndex;
    res = vkAcquireNextImageKHR(vk_state.dev, vk_state.swapchain,
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
    if (res != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "VK_BeginFrame: vkAcquireNextImageKHR failed (%d), skipping frame\n", res);
        return;
    }
    vk_state.currentImageIndex = imageIndex;

    /* The command buffer for this swapchain image may still be in flight from
     * a submission older than the one guarded by inFlight[frameIndex] (with
     * swapCount > VK_MAX_FRAMES_IN_FLIGHT the two indices are not the same).
     * Wait for the specific submission that last used this image. */
    if (vk_state.imagesInFlight && vk_state.imagesInFlight[imageIndex] != VK_NULL_HANDLE &&
        vk_state.imagesInFlight[imageIndex] != vk_state.inFlight[vk_state.frameIndex]) {
        vkWaitForFences(vk_state.dev, 1, &vk_state.imagesInFlight[imageIndex],
                        VK_TRUE, UINT64_MAX);
    }
    if (vk_state.imagesInFlight) {
        vk_state.imagesInFlight[imageIndex] = vk_state.inFlight[vk_state.frameIndex];
    }

    VkCommandBuffer cmd = vk_state.cmdBuffers[imageIndex];
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

    if (!cmd || !vk_state.framebuffers[vk_state.frameIndex]) {
        ri.Printf(PRINT_WARNING, "VK_BeginFrame: missing cmd buffer or offscreen framebuffer\n");
        return;
    }

    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkResetCommandBuffer(cmd, 0) != VK_SUCCESS ||
        vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "VK_BeginFrame: failed to begin command buffer\n");
        return;
    }

    VkClearValue clears[2];

    clears[0].color.float32[0] = 0.0f;
    clears[0].color.float32[1] = 0.0f;
    clears[0].color.float32[2] = 0.0f;
    clears[0].color.float32[3] = 1.0f;
    clears[1].depthStencil.depth = 1.0f;
    clears[1].depthStencil.stencil = 0;

    VK_PrepareOffscreenColor(cmd);

    VkRenderPassBeginInfo rpbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rpbi.renderPass = vk_state.renderPass;
    rpbi.framebuffer = vk_state.framebuffers[vk_state.frameIndex];
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

    /* Bind the per-view MVP descriptor set (set 1) for this frame slot. */
    VK_ViewBindSet(cmd);

    VK_ResetDynamicVBO();
    vk_2dBatchCount = 0;
    vk_2dBatchPipe = -1;
    vk_2dBatchDesc = VK_NULL_HANDLE;
}

void VK_EndFrame(int *frontEndMsec, int *backEndMsec) {
    VkCommandBuffer cmd;
    VkResult res;

    if (!vk_state.renderPassActive) {
        return;
    }

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    if (!cmd) {
        ri.Printf(PRINT_WARNING, "VK_EndFrame: missing command buffer for image %u\n", vk_state.currentImageIndex);
        vk_state.renderPassActive = qfalse;
        return;
    }

    VK_Flush2DBatch();

    vkCmdEndRenderPass(cmd);

    /* Copy offscreen → swapchain for present. Offscreen keeps contents (LOAD). */
    VK_BlitOffscreenToSwapchain(cmd);

    res = vkEndCommandBuffer(cmd);
    if (res != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "VK_EndFrame: vkEndCommandBuffer failed (%d), skipping frame\n", res);
        vk_state.renderPassActive = qfalse;
        return;
    }

    /* Swap is written in TRANSFER after acquire — wait there, not color output. */
    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_TRANSFER_BIT;

    /* The fence must be reset immediately before it is submitted. Doing it
     * earlier (in VK_BeginFrame) leaves the fence unsignaled forever if any
     * later step fails, which deadlocks the next frame. */
    vkResetFences(vk_state.dev, 1, &vk_state.inFlight[vk_state.frameIndex]);

    /* Include the batched texture-upload command buffer first in the same
     * submission. Its layout transitions complete before any rendering
     * command executes, and no queue drain is needed. */
    VkCommandBuffer cmds[2];
    uint32_t cmdCount = 0;
    VkCommandBuffer uploadCmd;
    if (VK_UploadGetPending(&uploadCmd)) {
        cmds[cmdCount++] = uploadCmd;
    }
    cmds[cmdCount++] = cmd;

    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &vk_state.imageAvailable[vk_state.frameIndex];
    si.pWaitDstStageMask = &waitStages;
    si.commandBufferCount = cmdCount;
    si.pCommandBuffers = cmds;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &vk_state.renderFinished[vk_state.frameIndex];
    res = vkQueueSubmit(vk_state.gfxQueue, 1, &si, vk_state.inFlight[vk_state.frameIndex]);
    if (res != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "VK_EndFrame: vkQueueSubmit failed (%d)\n", res);
        /* Try to recover by signaling the fence with an empty submission so
         * the next VK_BeginFrame does not deadlock. */
        VkSubmitInfo emptySi = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        if (vkQueueSubmit(vk_state.gfxQueue, 1, &emptySi, vk_state.inFlight[vk_state.frameIndex]) != VK_SUCCESS) {
            ri.Printf(PRINT_WARNING, "VK_EndFrame: could not recover in-flight fence\n");
        }
        vkQueueWaitIdle(vk_state.gfxQueue);
        vk_state.renderPassActive = qfalse;
        return;
    }

    VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &vk_state.renderFinished[vk_state.frameIndex];
    pi.swapchainCount = 1;
    pi.pSwapchains = &vk_state.swapchain;
    pi.pImageIndices = &vk_state.currentImageIndex;
    res = vkQueuePresentKHR(vk_state.presentQueue, &pi);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        ri.Printf(PRINT_WARNING, "VK_EndFrame: present reported %d, swapchain will be updated next frame\n", res);
    } else if (res != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "VK_EndFrame: vkQueuePresentKHR failed (%d)\n", res);
    }

    vk_state.renderPassActive = qfalse;
    vk_state.frameIndex = (vk_state.frameIndex + 1) % VK_MAX_FRAMES_IN_FLIGHT;

    if (frontEndMsec) *frontEndMsec = 0;
    if (backEndMsec) *backEndMsec = 0;
}

/* ---------------------------------------------------------------------------
 * Per-view MVP uniform buffer.
 *
 * The view projection is converted once per view and the MVP is computed once
 * per (view, entity) into a UBO slot; draws reference the slot through a
 * 4-byte push-constant index instead of a full 64-byte matrix plus the
 * per-draw matrix multiply on the CPU.
 * ------------------------------------------------------------------------- */

uint32_t vk_currentMvpSlot;
uint32_t vk_worldMvpSlot;

static float vk_viewProj[16];
static int vk_2dMvpSlot = -1;

static void VK_Set2DMVP(float *mvp, int width, int height);

void VK_ViewFrameBegin(void) {
    vk_viewUbo.slotCount = 0;
    vk_viewUbo.drawSlotCount = 0;
    vk_2dMvpSlot = -1;
    VK_BoneFrameBegin();
}

void VK_BoneFrameBegin(void) {
    vk_boneUbo.setCount = 0;
    vk_currentBoneSet = 0;
}

uint32_t VK_BoneAllocSet(const mdsBoneFrame_t *bones, int numBones) {
    uint32_t slot;
    float *dst;
    int i;
    int n;

    if (!vk_boneUbo.mapped || !bones || numBones <= 0) {
        return 0;
    }

    if (vk_boneUbo.setCount >= VK_BONE_MAX_SETS) {
        static int overflowLogged = 0;
        if (!overflowLogged) {
            overflowLogged = 1;
            ri.Printf(PRINT_WARNING, "VK_BoneAllocSet: overflow, reusing last set\n");
        }
        return vk_boneUbo.setCount > 0 ? vk_boneUbo.setCount - 1 : 0;
    }

    slot = vk_boneUbo.setCount++;
    dst = (float *)(vk_boneUbo.mapped +
                    (size_t)vk_state.frameIndex * VK_BONE_UBO_REGION_SIZE +
                    (size_t)slot * VK_BONE_SET_BYTES);

    n = numBones;
    if (n > VK_BONE_COUNT) {
        n = VK_BONE_COUNT;
    }
    memset(dst, 0, VK_BONE_SET_BYTES);
    for (i = 0; i < n; i++) {
        float *row = dst + i * 12;
        /* mat3x4 rows: (m[0], tx), (m[1], ty), (m[2], tz) */
        row[0] = bones[i].matrix[0][0];
        row[1] = bones[i].matrix[0][1];
        row[2] = bones[i].matrix[0][2];
        row[3] = bones[i].translation[0];
        row[4] = bones[i].matrix[1][0];
        row[5] = bones[i].matrix[1][1];
        row[6] = bones[i].matrix[1][2];
        row[7] = bones[i].translation[1];
        row[8] = bones[i].matrix[2][0];
        row[9] = bones[i].matrix[2][1];
        row[10] = bones[i].matrix[2][2];
        row[11] = bones[i].translation[2];
    }
    return slot;
}

uint32_t VK_ViewAllocMvp(const float mvp[16]) {
    uint32_t slot;
    uint8_t *dst;

    if (vk_viewUbo.slotCount >= VK_VIEW_MAX_MVPS) {
        static int overflowLogged = 0;
        if (!overflowLogged) {
            overflowLogged = 1;
            ri.Printf(PRINT_WARNING, "VK_ViewAllocMvp: slot overflow, reusing slot 0\n");
        }
        return 0;
    }

    slot = vk_viewUbo.slotCount++;
    dst = vk_viewUbo.mapped +
          (size_t)vk_state.frameIndex * VK_VIEW_UBO_REGION_SIZE +
          (size_t)slot * 64;
    memcpy(dst, mvp, 64);
    return slot;
}

uint32_t VK_ViewAllocDrawParams(const float *drawParams) {
    uint32_t slot;
    uint8_t *dst;
    uint8_t *regionBase;

    if (!vk_viewUbo.mapped || !drawParams) {
        return 0;
    }

    regionBase = vk_viewUbo.mapped +
                 (size_t)vk_state.frameIndex * VK_VIEW_UBO_REGION_SIZE +
                 VK_VIEW_UBO_DRAW_OFF;

    /* Reuse the previous slot when fog/fade params did not change. Consecutive
     * stages/batches almost always share identical distance-fog state. */
    if (vk_viewUbo.drawSlotCount > 0) {
        uint32_t prev = vk_viewUbo.drawSlotCount - 1;
        const uint8_t *prevSrc = regionBase + (size_t)prev * VK_VIEW_DRAW_SLOT_BYTES;
        if (memcmp(prevSrc, drawParams, VK_VIEW_DRAW_SLOT_BYTES) == 0) {
            return prev;
        }
    }

    if (vk_viewUbo.drawSlotCount >= VK_VIEW_MAX_DRAW_PARAMS) {
        static int overflowLogged = 0;
        if (!overflowLogged) {
            overflowLogged = 1;
            ri.Printf(PRINT_WARNING,
                      "VK_ViewAllocDrawParams: slot overflow (%u), reusing last slot\n",
                      VK_VIEW_MAX_DRAW_PARAMS);
        }
        /* Never reuse slot 0: that stomps the first draws of the frame and
         * makes fog flicker on a handful of surfaces. Overwrite only the last
         * slot (newest fog state). */
        slot = VK_VIEW_MAX_DRAW_PARAMS - 1;
    } else {
        slot = vk_viewUbo.drawSlotCount++;
    }

    dst = regionBase + (size_t)slot * VK_VIEW_DRAW_SLOT_BYTES;
    memcpy(dst, drawParams, VK_VIEW_DRAW_SLOT_BYTES);
    return slot;
}

void VK_ViewBegin(void) {
    float mvp[16];

    /* Runs on the BACK END at the start of VK_DrawSurfList, where
     * backEnd.viewParms already holds the CURRENT scene's data (set by
     * RB_DrawSurfs). This matches the old per-draw computation and is correct
     * even with multiple scenes per frame. */
    memcpy(vk_viewProj, backEnd.viewParms.projectionMatrix, sizeof(vk_viewProj));
    VK_ConvertProjectionMatrixToVulkan(vk_viewProj);
    VK_MatrixMulQ3Clip(mvp, vk_viewProj, backEnd.viewParms.world.modelMatrix);

    vk_worldMvpSlot = VK_ViewAllocMvp(mvp);
    vk_currentMvpSlot = vk_worldMvpSlot;
    VK_UploadDlights();
}

void VK_ViewSetEntityMvp(void) {
    float mvp[16];

    if (!backEnd.currentEntity || backEnd.currentEntity == &tr.worldEntity) {
        vk_currentMvpSlot = vk_worldMvpSlot;
        return;
    }

    VK_MatrixMulQ3Clip(mvp, vk_viewProj, backEnd.or.modelMatrix);
    vk_currentMvpSlot = VK_ViewAllocMvp(mvp);
}

uint32_t VK_ViewGet2DMvpSlot(void) {
    if (vk_2dMvpSlot < 0) {
        float mvp[16];

        VK_Set2DMVP(mvp, vk_state.swapExtent.width, vk_state.swapExtent.height);
        vk_2dMvpSlot = (int)VK_ViewAllocMvp(mvp);
    }
    return (uint32_t)vk_2dMvpSlot;
}

void VK_ViewBindSet(VkCommandBuffer cmd) {
    VK_ViewBindBones(cmd, 0);
}

void VK_ViewBindBones(VkCommandBuffer cmd, uint32_t boneSet) {
    uint32_t dynamicOffsets[3];

    if (boneSet >= VK_BONE_MAX_SETS) {
        boneSet = 0;
    }

    dynamicOffsets[0] = vk_state.frameIndex * VK_VIEW_UBO_REGION_SIZE;
    dynamicOffsets[1] = vk_state.frameIndex * VK_BONE_UBO_REGION_SIZE +
                        boneSet * VK_BONE_SET_BYTES;
    dynamicOffsets[2] = vk_state.frameIndex * VK_DLIGHT_UBO_BYTES;

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vk_state.pipelineLayout, 1, 1, &vk_viewUbo.set,
                            3, dynamicOffsets);
}

void VK_UploadDlights(void) {
    uint8_t *base;
    uint32_t *header;
    float *posRadius;
    float *colors;
    int i;
    int n;

    if (!vk_dlightUbo.mapped) {
        return;
    }

    base = vk_dlightUbo.mapped + (size_t)vk_state.frameIndex * VK_DLIGHT_UBO_BYTES;
    header = (uint32_t *)base;
    posRadius = (float *)(base + 16);
    colors = posRadius + VK_DLIGHT_MAX * 4;

    n = backEnd.refdef.num_dlights;
    if (n > VK_DLIGHT_MAX) {
        n = VK_DLIGHT_MAX;
    }
    header[0] = (uint32_t)n;
    header[1] = 0;
    header[2] = 0;
    header[3] = 0;

    memset(posRadius, 0, VK_DLIGHT_MAX * 32);
    for (i = 0; i < n; i++) {
        const dlight_t *dl = &backEnd.refdef.dlights[i];
        float *pr = posRadius + i * 4;
        float *col = colors + i * 4;

        pr[0] = dl->transformed[0];
        pr[1] = dl->transformed[1];
        pr[2] = dl->transformed[2];
        pr[3] = dl->radius;
        col[0] = dl->color[0];
        col[1] = dl->color[1];
        col[2] = dl->color[2];
        col[3] = 1.0f;
    }
}

void VK_CmdPushMaterial(VkCommandBuffer cmd, const vk_push_constants_t *pcIn) {
    vk_push_constants_t pc;

    if (!pcIn) {
        return;
    }

    pc = *pcIn;
    /* One immutable slot per push: GPU reads it later when the CB runs, so a
     * single shared drawParams cell would make fog flicker (last write wins). */
    pc.drawParamIndex = VK_ViewAllocDrawParams(&pc.params[15][0]);
    /* pad[0] = MDS bone set (bind offset); pad[1] = MD3 shortMode — keep from caller. */
    vkCmdPushConstants(cmd, vk_state.pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, VK_PUSH_CONSTANTS_GPU_SIZE, &pc);
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

    /* The view-projection UBO slots are allocated on the back end in
     * VK_DrawSurfList (VK_ViewBegin), where backEnd.viewParms holds the
     * current scene's data. */

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

    /* tcMod runs in world.vert via push constants — keep 2D path texcoord-free. */
    if (stage->bundle[0].numTexMods > 0) {
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

void VK_Flush2DBatch(void) {
    VkCommandBuffer cmd;
    int vboSize, iboSize, vboOff, iboOff;
    drawVert_t *dst;
    int *idx;
    VkDeviceSize offsets[1];
    int q;

    if (vk_2dBatchCount <= 0) {
        return;
    }

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    if (!cmd || !vk_state.renderPassActive) {
        vk_2dBatchCount = 0;
        vk_2dBatchPipe = -1;
        vk_2dBatchDesc = VK_NULL_HANDLE;
        return;
    }

    vboSize = vk_2dBatchCount * 4 * (int)sizeof(drawVert_t);
    iboSize = vk_2dBatchCount * 6 * (int)sizeof(int);
    vboOff = VK_ReserveDynamicVBO(vboSize + iboSize);
    if (vboOff < 0) {
        vk_2dBatchCount = 0;
        return;
    }
    iboOff = vboOff + vboSize;

    dst = (drawVert_t *)((uint8_t *)vk_dyn.mapped + vboOff);
    idx = (int *)((uint8_t *)vk_dyn.mapped + iboOff);
    Com_Memcpy(dst, vk_2dBatchVerts, (size_t)vboSize);

    for (q = 0; q < vk_2dBatchCount; q++) {
        int base = q * 4;
        int *ii = idx + q * 6;
        ii[0] = base + 0; ii[1] = base + 1; ii[2] = base + 2;
        ii[3] = base + 0; ii[4] = base + 2; ii[5] = base + 3;
    }

    VK_SetPicViewport(cmd);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      vk_state.pipelines[vk_2dBatchPipe]);
    offsets[0] = (VkDeviceSize)vboOff;
    VK_BindMeshVertexBuffers(cmd, vk_dyn.buffer, offsets[0], VK_NULL_HANDLE, 0);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(cmd, vk_state.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(vk_2dBatchMvp), &vk_2dBatchMvp);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vk_state.pipelineLayout, 0, 1, &vk_2dBatchDesc, 0, NULL);
    vkCmdDrawIndexed(cmd, (uint32_t)(vk_2dBatchCount * 6), 1, 0, 0, 0);

    vk_2dBatchCount = 0;
}

static void VK_Append2DQuad(int pipeIdx, VkDescriptorSet descSet, uint32_t mvpSlot,
                            const drawVert_t verts[4]) {
    if (vk_2dBatchCount > 0 &&
        (pipeIdx != vk_2dBatchPipe || descSet != vk_2dBatchDesc || mvpSlot != vk_2dBatchMvp)) {
        VK_Flush2DBatch();
    }
    if (vk_2dBatchCount >= VK_2D_BATCH_MAX) {
        VK_Flush2DBatch();
    }

    vk_2dBatchPipe = pipeIdx;
    vk_2dBatchDesc = descSet;
    vk_2dBatchMvp = mvpSlot;
    Com_Memcpy(vk_2dBatchVerts + vk_2dBatchCount * 4, verts, 4 * sizeof(drawVert_t));
    vk_2dBatchCount++;
}

static void VK_DrawPicQuad(float x, float y, float w, float h,
                           float s1, float t1, float s2, float t2,
                           shader_t *shader,
                           const byte color0[4], const byte color1[4],
                           const byte color2[4], const byte color3[4]) {
    VkCommandBuffer cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    shader_t *state;
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
    qboolean reservedSharedIbo = qfalse;

    if (!cmd || !vk_state.renderPassActive || !shader) {
        return;
    }

    state = shader->remappedShader ? shader->remappedShader : shader;
    if (!state->stages[0]) {
        return;
    }

    VK_SetPicViewport(cmd);

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

        use2D = VK_UIStageUses2DPipeline(state, stage);
        if (use2D) {
            VK_ApplyPicTexMods(state, stage, stageVerts);
        }

        VK_FillPicStageColors(state, stage, color0, color1, color2, color3, stageColors);

        for (corner = 0; corner < 4; corner++) {
            stageVerts[corner].color[0] = stageColors[corner][0];
            stageVerts[corner].color[1] = stageColors[corner][1];
            stageVerts[corner].color[2] = stageColors[corner][2];
            stageVerts[corner].color[3] = stageColors[corner][3];
        }

        if (use2D) {
            image_t *img = VK_BundleImage(&stage->bundle[0], state);

            pipeIdx = VK_PipelineFor2DPic(stage);
            descSet = VK_GetDescriptorSetForImage(img ? img : tr.whiteImage);
            VK_Append2DQuad(pipeIdx, descSet, VK_ViewGet2DMvpSlot(), stageVerts);
            continue;
        }

        /* World-pipeline UI: flush lean 2D batch first. */
        VK_Flush2DBatch();

        if (!reservedSharedIbo) {
            iboOff = VK_ReserveDynamicVBO(iboSize);
            if (iboOff < 0) {
                continue;
            }
            idx = (int *)((uint8_t *)vk_dyn.mapped + iboOff);
            idx[0] = 0; idx[1] = 1; idx[2] = 2;
            idx[3] = 0; idx[4] = 2; idx[5] = 3;
            vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
            reservedSharedIbo = qtrue;
        }

        stageVboOff = VK_ReserveDynamicVBO(vboSize);
        if (stageVboOff < 0) {
            continue;
        }
        Com_Memcpy((uint8_t *)vk_dyn.mapped + stageVboOff, stageVerts, vboSize);
        offsets[0] = (VkDeviceSize)stageVboOff;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vk_dyn.buffer, offsets);

        VK_SetUIStageStateFromShader(state, stage);
        VK_FillPushConstants(VK_ViewGet2DMvpSlot(), state, &pc);
        pc.params[14][3] = 0.2f;

        pipeIdx = VK_PipelineForUIStage(stage);
        if (pipeIdx != currentPipe) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipeIdx]);
            currentPipe = pipeIdx;
        }

        vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);
        VK_CmdPushMaterial(cmd, &pc);

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

    VK_Flush2DBatch();

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    if (!cmd) {
        return;
    }

    s1 = 0.5f / (float)cols;
    t1 = 0.5f / (float)rows;
    s2 = ((float)cols - 0.5f) / (float)cols;
    t2 = ((float)rows - 0.5f) / (float)rows;

    VK_SetPicViewport(cmd);

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
    {
        uint32_t slot = VK_ViewGet2DMvpSlot();
        vkCmdPushConstants(cmd, vk_state.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(slot), &slot);
    }

    descSet = VK_GetDescriptorSetForImage(image);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}

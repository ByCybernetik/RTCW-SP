#include "vk_local.h"
#include "vk_material.h"
#include "vk_sky.h"
#include <math.h>
#include <string.h>

#define SKY_SUBDIVISIONS        8
#define HALF_SKY_SUBDIVISIONS   (SKY_SUBDIVISIONS / 2)
#define SKY_FACE_VERTS          ((SKY_SUBDIVISIONS + 1) * (SKY_SUBDIVISIONS + 1))
#define SKY_FACE_QUADS          (SKY_SUBDIVISIONS * SKY_SUBDIVISIONS)
#define SKY_FACE_IDX            (SKY_FACE_QUADS * 6)
#define SKY_TOTAL_VERTS         (SKY_FACE_VERTS * 6)
#define SKY_TOTAL_IDX           (SKY_FACE_IDX * 6)

extern void RB_ClipSkyPolygons(shaderCommands_t *input);
extern void RB_SurfaceFace(srfSurfaceFace_t *surf);
extern void RB_SurfaceGrid(srfGridMesh_t *cv);
extern void RB_SurfaceTriangles(srfTriangles_t *srf);

extern float sky_mins[2][6];
extern float sky_maxs[2][6];

typedef struct {
    VkBuffer vbo;
    VkDeviceMemory vboMemory;
    VkBuffer ibo;
    VkDeviceMemory iboMemory;
    uint32_t faceFirstIndex[6];
    uint32_t faceIndexCount;
    qboolean ready;
} vk_sky_static_t;

static vk_sky_static_t vk_skyStatic;

static void VK_FillSkyPushConstants(const shader_t *shader, vk_push_constants_t *pc) {
    /* The sky is drawn with the world entity active; reuse its per-view MVP
     * slot instead of recomputing the matrix. */
    VK_FillPushConstants(vk_currentMvpSlot, shader, pc);
}

/* Unit-cube sky direction for axis (boxSize == 1). ST in [0,1]. */
static void VK_MakeUnitSkyVec(float s, float t, int axis, float outSt[2], vec3_t outXYZ) {
    static int st_to_vec[6][3] = {
        { 3,-1, 2},
        {-3, 1, 2},
        { 1, 3, 2},
        {-1,-3, 2},
        {-2,-1, 3},
        { 2,-1,-3}
    };
    vec3_t b;
    int j, k;

    b[0] = s;
    b[1] = t;
    b[2] = 1.0f;

    for (j = 0; j < 3; j++) {
        k = st_to_vec[axis][j];
        if (k < 0) {
            outXYZ[j] = -b[-k - 1];
        } else {
            outXYZ[j] = b[k - 1];
        }
    }

    s = (s + 1.0f) * 0.5f;
    t = (t + 1.0f) * 0.5f;
    t = 1.0f - t;

    if (outSt) {
        outSt[0] = s;
        outSt[1] = t;
    }
}

static qboolean VK_CreateSkyHostBuffer(VkBufferUsageFlags usage, const void *data,
                                       VkDeviceSize size, VkBuffer *outBuf,
                                       VkDeviceMemory *outMem) {
    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkMemoryRequirements req;
    void *mapped;

    *outBuf = VK_NULL_HANDLE;
    *outMem = VK_NULL_HANDLE;

    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vk_state.dev, &bci, NULL, outBuf) != VK_SUCCESS) {
        return qfalse;
    }
    vkGetBufferMemoryRequirements(vk_state.dev, *outBuf, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = VK_FindMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(vk_state.dev, &ai, NULL, outMem) != VK_SUCCESS) {
        vkDestroyBuffer(vk_state.dev, *outBuf, NULL);
        *outBuf = VK_NULL_HANDLE;
        return qfalse;
    }
    vkBindBufferMemory(vk_state.dev, *outBuf, *outMem, 0);
    if (vkMapMemory(vk_state.dev, *outMem, 0, size, 0, &mapped) != VK_SUCCESS) {
        vkDestroyBuffer(vk_state.dev, *outBuf, NULL);
        vkFreeMemory(vk_state.dev, *outMem, NULL);
        *outBuf = VK_NULL_HANDLE;
        *outMem = VK_NULL_HANDLE;
        return qfalse;
    }
    memcpy(mapped, data, (size_t)size);
    vkUnmapMemory(vk_state.dev, *outMem);
    return qtrue;
}

qboolean VK_InitSkyStatic(void) {
    drawVert_t *verts;
    uint32_t *indices;
    int axis, s, t;
    int vertBase;
    int idxBase;
    VkDeviceSize vboSize;
    VkDeviceSize iboSize;

    memset(&vk_skyStatic, 0, sizeof(vk_skyStatic));

    verts = (drawVert_t *)ri.Hunk_AllocateTempMemory(SKY_TOTAL_VERTS * sizeof(drawVert_t));
    indices = (uint32_t *)ri.Hunk_AllocateTempMemory(SKY_TOTAL_IDX * sizeof(uint32_t));
    if (!verts || !indices) {
        if (verts) {
            ri.Hunk_FreeTempMemory(verts);
        }
        if (indices) {
            ri.Hunk_FreeTempMemory(indices);
        }
        return qfalse;
    }

    memset(verts, 0, SKY_TOTAL_VERTS * sizeof(drawVert_t));
    idxBase = 0;
    for (axis = 0; axis < 6; axis++) {
        vertBase = axis * SKY_FACE_VERTS;
        vk_skyStatic.faceFirstIndex[axis] = (uint32_t)idxBase;

        for (t = 0; t <= SKY_SUBDIVISIONS; t++) {
            for (s = 0; s <= SKY_SUBDIVISIONS; s++) {
                float s_norm = (s / (float)HALF_SKY_SUBDIVISIONS) - 1.0f;
                float t_norm = (t / (float)HALF_SKY_SUBDIVISIONS) - 1.0f;
                int vi = vertBase + t * (SKY_SUBDIVISIONS + 1) + s;
                float st[2];
                vec3_t xyz;

                VK_MakeUnitSkyVec(s_norm, t_norm, axis, st, xyz);
                VectorCopy(xyz, verts[vi].xyz);
                verts[vi].st[0] = st[0];
                verts[vi].st[1] = st[1];
                VectorSet(verts[vi].normal, 0.0f, 0.0f, 1.0f);
                verts[vi].color[0] = 255;
                verts[vi].color[1] = 255;
                verts[vi].color[2] = 255;
                verts[vi].color[3] = 255;
            }
        }

        for (t = 0; t < SKY_SUBDIVISIONS; t++) {
            for (s = 0; s < SKY_SUBDIVISIONS; s++) {
                int row = SKY_SUBDIVISIONS + 1;
                int v0 = vertBase + s + t * row;
                int v1 = vertBase + s + (t + 1) * row;
                int v2 = vertBase + s + 1 + t * row;
                int v3 = vertBase + s + 1 + (t + 1) * row;

                indices[idxBase++] = (uint32_t)v0;
                indices[idxBase++] = (uint32_t)v1;
                indices[idxBase++] = (uint32_t)v2;
                indices[idxBase++] = (uint32_t)v1;
                indices[idxBase++] = (uint32_t)v3;
                indices[idxBase++] = (uint32_t)v2;
            }
        }
    }
    vk_skyStatic.faceIndexCount = SKY_FACE_IDX;

    vboSize = (VkDeviceSize)(SKY_TOTAL_VERTS * sizeof(drawVert_t));
    iboSize = (VkDeviceSize)(SKY_TOTAL_IDX * sizeof(uint32_t));

    if (!VK_CreateSkyHostBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verts, vboSize,
                                &vk_skyStatic.vbo, &vk_skyStatic.vboMemory) ||
        !VK_CreateSkyHostBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices, iboSize,
                                &vk_skyStatic.ibo, &vk_skyStatic.iboMemory)) {
        ri.Hunk_FreeTempMemory(indices);
        ri.Hunk_FreeTempMemory(verts);
        VK_DestroySkyStatic();
        return qfalse;
    }

    ri.Hunk_FreeTempMemory(indices);
    ri.Hunk_FreeTempMemory(verts);
    vk_skyStatic.ready = qtrue;
    ri.Printf(PRINT_ALL, "VK_InitSkyStatic: %d verts, %d indices\n",
              SKY_TOTAL_VERTS, SKY_TOTAL_IDX);
    return qtrue;
}

void VK_DestroySkyStatic(void) {
    if (vk_skyStatic.vbo) {
        vkDestroyBuffer(vk_state.dev, vk_skyStatic.vbo, NULL);
        vk_skyStatic.vbo = VK_NULL_HANDLE;
    }
    if (vk_skyStatic.vboMemory) {
        vkFreeMemory(vk_state.dev, vk_skyStatic.vboMemory, NULL);
        vk_skyStatic.vboMemory = VK_NULL_HANDLE;
    }
    if (vk_skyStatic.ibo) {
        vkDestroyBuffer(vk_state.dev, vk_skyStatic.ibo, NULL);
        vk_skyStatic.ibo = VK_NULL_HANDLE;
    }
    if (vk_skyStatic.iboMemory) {
        vkFreeMemory(vk_state.dev, vk_skyStatic.iboMemory, NULL);
        vk_skyStatic.iboMemory = VK_NULL_HANDLE;
    }
    vk_skyStatic.ready = qfalse;
}

static void VK_DrawSkyBoxSide(image_t *image, int axis, VkCommandBuffer cmd,
                              vk_push_constants_t *pc, int pipelineIdx) {
    VkDescriptorSet descSet;
    float mins0, mins1, maxs0, maxs1;

    if (!image || image == tr.defaultImage) {
        return;
    }

    mins0 = floorf(sky_mins[0][axis] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
    mins1 = floorf(sky_mins[1][axis] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
    maxs0 = ceilf(sky_maxs[0][axis] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
    maxs1 = ceilf(sky_maxs[1][axis] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;

    /* Skip faces with no visible sky projection (portal / partial sky). */
    if (mins0 >= maxs0 || mins1 >= maxs1) {
        return;
    }

    descSet = VK_GetDescriptorSetForImage(image);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipelineIdx]);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);

    if (!vk_skyStatic.ready) {
        return;
    }

    VK_BindMeshVertexBuffers(cmd, vk_skyStatic.vbo, 0, VK_NULL_HANDLE, 0);
    vkCmdBindIndexBuffer(cmd, vk_skyStatic.ibo, 0, VK_INDEX_TYPE_UINT32);
    VK_CmdPushMaterial(cmd, pc);
    vkCmdDrawIndexed(cmd, vk_skyStatic.faceIndexCount, 1,
                     vk_skyStatic.faceFirstIndex[axis], 0, 0);
}

static void VK_DrawSkyBoxFaces(const shader_t *shader, image_t *images[6],
                               VkCommandBuffer cmd, vk_push_constants_t *pc,
                               int pipelineIdx) {
    static const int sky_texorder[6] = {0, 2, 1, 3, 4, 5};
    int i;

    (void)shader;

    for (i = 0; i < 6; i++) {
        int imgIdx = sky_texorder[i];

        if (!images[imgIdx] || images[imgIdx] == tr.defaultImage) {
            continue;
        }

        VK_DrawSkyBoxSide(images[imgIdx], i, cmd, pc, pipelineIdx);
    }
}

static void VK_DrawCloudLayers(shader_t *shader, VkCommandBuffer cmd,
                               vk_push_constants_t *pc) {
    int i, axis;

    if (!shader->sky.cloudHeight) {
        return;
    }

    if (!vk_skyStatic.ready) {
        return;
    }

    for (i = 0; i < MAX_SHADER_STAGES && shader->stages[i]; i++) {
        shaderStage_t *stage = shader->stages[i];
        VkDescriptorSet descSet;
        int pipeIdx;

        if (!stage->active) {
            continue;
        }

        VK_SetStageStateFromShader(shader, stage);
        VK_FillSkyPushConstants(shader, pc);
        VK_SetSkyPushConstants(shader, stage, pc, qtrue);

        pipeIdx = VK_StageIsBlended(stage) ? VK_PipelineForStage(stage) : VK_PIPELINE_SKY;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipeIdx]);

        descSet = VK_StageDescriptorSet(shader, stage);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);

        VK_BindMeshVertexBuffers(cmd, vk_skyStatic.vbo, 0, VK_NULL_HANDLE, 0);
        vkCmdBindIndexBuffer(cmd, vk_skyStatic.ibo, 0, VK_INDEX_TYPE_UINT32);
        VK_CmdPushMaterial(cmd, pc);

        /* Faces 0..4 only — match FillCloudBox (skip bottom). */
        for (axis = 0; axis < 5; axis++) {
            float mins0, mins1, maxs0, maxs1;

            mins0 = floorf(sky_mins[0][axis] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
            mins1 = floorf(sky_mins[1][axis] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
            maxs0 = ceilf(sky_maxs[0][axis] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
            maxs1 = ceilf(sky_maxs[1][axis] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
            if (mins0 >= maxs0 || mins1 >= maxs1) {
                continue;
            }

            vkCmdDrawIndexed(cmd, vk_skyStatic.faceIndexCount, 1,
                             vk_skyStatic.faceFirstIndex[axis], 0, 0);
        }
    }
}

void VK_DrawSky(shader_t *shader, surfaceType_t *surf, VkCommandBuffer cmd) {
    shaderCommands_t savedTess;
    vk_push_constants_t pc;
    VkViewport vp;
    VkRect2D sc;
    int vpY;

    if (!shader || !shader->isSky) {
        return;
    }

    if (r_fastsky->integer) {
        return;
    }

    if (backEnd.viewParms.glFog.registered) {
        if (!backEnd.viewParms.glFog.drawsky) {
            return;
        }
    } else if (glfogNum > FOG_NONE) {
        if (!glfogsettings[FOG_CURRENT].drawsky) {
            return;
        }
    }

    savedTess = tess;
    Com_Memset(&tess, 0, sizeof(tess));

    /* Force full depth range and no polygon offset for sky. */
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
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
    vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);

    RB_BeginSurface(shader, savedTess.fogNum);

    switch (*surf) {
    case SF_FACE:
        RB_SurfaceFace((srfSurfaceFace_t *)surf);
        break;
    case SF_GRID:
        RB_SurfaceGrid((srfGridMesh_t *)surf);
        break;
    case SF_TRIANGLES:
        RB_SurfaceTriangles((srfTriangles_t *)surf);
        break;
    default:
        break;
    }

    /* Determine which sides of the sky box are visible from this surface. */
    RB_ClipSkyPolygons(&tess);

    backEnd.refdef.rdflags |= RDF_DRAWINGSKY;

    /* Outer skybox. */
    if (shader->sky.outerbox[0] && shader->sky.outerbox[0] != tr.defaultImage) {
        VK_SetStageStateFromShader(shader, NULL);
        VK_FillSkyPushConstants(shader, &pc);
        VK_SetSkyPushConstants(shader, NULL, &pc, qfalse);

        VK_DrawSkyBoxFaces(shader, shader->sky.outerbox, cmd, &pc, VK_PIPELINE_SKY);
    }

    /* Cloud layers. */
    VK_DrawCloudLayers(shader, cmd, &pc);

    /* Inner skybox (drawn with alpha blending like GL). */
    if (shader->sky.innerbox[0] && shader->sky.innerbox[0] != tr.defaultImage) {
        tess.numIndexes = 0;
        tess.numVertexes = 0;

        VK_SetStageStateFromShader(shader, NULL);
        VK_FillSkyPushConstants(shader, &pc);
        VK_SetSkyPushConstants(shader, NULL, &pc, qfalse);

        VK_DrawSkyBoxFaces(shader, shader->sky.innerbox, cmd, &pc, VK_PIPELINE_ALPHA);
    }

    tess = savedTess;

    backEnd.refdef.rdflags &= ~RDF_DRAWINGSKY;
    backEnd.skyRenderedThisView = qtrue;
}

void VK_DrawSun(VkCommandBuffer cmd) {
    shader_t *shader;
    shaderCommands_t savedTess;
    float dist;
    float size;
    vec3_t origin, left, up, normal;
    byte color[4] = { 255, 255, 255, 255 };
    int baseVert, baseIdx;
    int i;
    VkViewport vp;
    VkRect2D sc;
    int vpY;

    if (!cmd || !vk_state.renderPassActive) {
        return;
    }

    if (!tr.sunShader) {
        return;
    }
    if (!backEnd.skyRenderedThisView) {
        return;
    }
    if (!r_drawSun || !r_drawSun->integer) {
        return;
    }

    shader = tr.sunShader->remappedShader ? tr.sunShader->remappedShader : tr.sunShader;
    if (!shader->stages[0]) {
        return;
    }

    dist = backEnd.viewParms.zFar / 1.75f;
    size = dist * 0.2f;

    /*
     * The world model matrix includes the -viewOrigin translation, so add
     * viewOrigin here to cancel it out. This matches GL's
     * qglTranslatef(viewOrigin) before drawing the sun quad.
     */
    VectorScale(tr.sunDirection, dist, origin);
    VectorAdd(origin, backEnd.viewParms.or.origin, origin);
    PerpendicularVector(left, tr.sunDirection);
    CrossProduct(tr.sunDirection, left, up);
    VectorScale(left, size, left);
    VectorScale(up, size, up);

    VectorSubtract(vec3_origin, backEnd.viewParms.or.axis[0], normal);

    savedTess = tess;
    Com_Memset(&tess, 0, sizeof(tess));

    /* Build the sun quad in tess, mirroring RB_AddQuadStamp. */
    baseVert = tess.numVertexes;
    baseIdx = tess.numIndexes;

    tess.indexes[tess.numIndexes++] = baseVert + 0;
    tess.indexes[tess.numIndexes++] = baseVert + 1;
    tess.indexes[tess.numIndexes++] = baseVert + 3;
    tess.indexes[tess.numIndexes++] = baseVert + 3;
    tess.indexes[tess.numIndexes++] = baseVert + 1;
    tess.indexes[tess.numIndexes++] = baseVert + 2;

    VectorAdd(origin, left, tess.xyz[baseVert + 0]);
    VectorAdd(tess.xyz[baseVert + 0], up, tess.xyz[baseVert + 0]);

    VectorSubtract(origin, left, tess.xyz[baseVert + 1]);
    VectorAdd(tess.xyz[baseVert + 1], up, tess.xyz[baseVert + 1]);

    VectorSubtract(origin, left, tess.xyz[baseVert + 2]);
    VectorSubtract(tess.xyz[baseVert + 2], up, tess.xyz[baseVert + 2]);

    VectorAdd(origin, left, tess.xyz[baseVert + 3]);
    VectorSubtract(tess.xyz[baseVert + 3], up, tess.xyz[baseVert + 3]);

    for (i = 0; i < 4; i++) {
        VectorCopy(normal, tess.normal[baseVert + i]);
        tess.texCoords[baseVert + i][0][0] = (i == 0 || i == 3) ? 0.0f : 1.0f;
        tess.texCoords[baseVert + i][0][1] = (i == 0 || i == 1) ? 0.0f : 1.0f;
        tess.texCoords[baseVert + i][1][0] = 0.0f;
        tess.texCoords[baseVert + i][1][1] = 0.0f;
        tess.vertexColors[baseVert + i][0] = color[0];
        tess.vertexColors[baseVert + i][1] = color[1];
        tess.vertexColors[baseVert + i][2] = color[2];
        tess.vertexColors[baseVert + i][3] = color[3];
    }
    tess.numVertexes = baseVert + 4;

    /* Push the sun to the far depth plane so it only shows over the sky. */
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
    vp.minDepth = 1.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
    vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);

    /* Draw each active stage of the sun shader. */
    for (i = 0; i < MAX_SHADER_STAGES && shader->stages[i]; i++) {
        shaderStage_t *stage = shader->stages[i];
        vk_push_constants_t pc;
        int pipeIdx;
        VkDescriptorSet descSet;

        if (!stage->active) {
            continue;
        }
        if (stage->bundle[0].isLightmap) {
            continue;
        }

        VK_SetStageStateFromShader(shader, stage);

        /* The sun is drawn after the surf list with the world orientation
         * restored: reuse the world-entity MVP slot for this view. */
        VK_FillPushConstants(vk_worldMvpSlot, shader, &pc);

        pipeIdx = VK_PipelineForStage(stage);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipeIdx]);

        VK_CmdPushMaterial(cmd, &pc);

        descSet = VK_StageDescriptorSet(shader, stage);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);

        VK_DrawTessRange(baseVert, baseIdx);
    }

    tess = savedTess;

    /* Restore the normal viewport for any following draws. */
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
    vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);
}

#include "vk_local.h"
#include "vk_material.h"
#include "vk_sky.h"
#include <math.h>

#define SKY_SUBDIVISIONS        8
#define HALF_SKY_SUBDIVISIONS   (SKY_SUBDIVISIONS / 2)

extern void RB_ClipSkyPolygons(shaderCommands_t *input);
extern void R_BuildCloudData(shaderCommands_t *input);
extern void RB_SurfaceFace(srfSurfaceFace_t *surf);
extern void RB_SurfaceGrid(srfGridMesh_t *cv);
extern void RB_SurfaceTriangles(srfTriangles_t *srf);

extern float sky_mins[2][6];
extern float sky_maxs[2][6];

static float vk_sky_min;
static float vk_sky_max;

static void VK_FillSkyPushConstants(const shader_t *shader, vk_push_constants_t *pc) {
    float proj[16];
    float mvp[16];

    memcpy(proj, tr.viewParms.projectionMatrix, sizeof(proj));
    VK_ConvertProjectionMatrixToVulkan(proj);
    VK_MatrixMulQ3Clip(mvp, proj, backEnd.or.modelMatrix);
    VK_FillPushConstants(mvp, shader, pc);
}

static void VK_MakeSkyVec(float s, float t, int axis, float outSt[2], vec3_t outXYZ) {
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
    float boxSize;

    if (glfogsettings[FOG_SKY].registered) {
        boxSize = glfogsettings[FOG_SKY].end;
    } else {
        boxSize = backEnd.viewParms.zFar / 1.75f;
    }
    if (boxSize < r_znear->value * 2.0f) {
        boxSize = r_znear->value * 2.0f;
    }

    b[0] = s * boxSize;
    b[1] = t * boxSize;
    b[2] = boxSize;

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
    if (s < vk_sky_min) {
        s = vk_sky_min;
    } else if (s > vk_sky_max) {
        s = vk_sky_max;
    }
    if (t < vk_sky_min) {
        t = vk_sky_min;
    } else if (t > vk_sky_max) {
        t = vk_sky_max;
    }
    t = 1.0f - t;

    if (outSt) {
        outSt[0] = s;
        outSt[1] = t;
    }
}

static void VK_DrawSkyBoxSide(image_t *image, int axis, VkCommandBuffer cmd,
                              vk_push_constants_t *pc, int pipelineIdx) {
    int sky_mins_subd[2], sky_maxs_subd[2];
    int s, t;
    int baseVert, baseIdx;
    int tHeight, sWidth;
    VkDescriptorSet descSet;

    sky_mins[0][axis] = floorf(sky_mins[0][axis] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
    sky_mins[1][axis] = floorf(sky_mins[1][axis] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
    sky_maxs[0][axis] = ceilf(sky_maxs[0][axis] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;
    sky_maxs[1][axis] = ceilf(sky_maxs[1][axis] * HALF_SKY_SUBDIVISIONS) / HALF_SKY_SUBDIVISIONS;

    if (sky_mins[0][axis] >= sky_maxs[0][axis] ||
        sky_mins[1][axis] >= sky_maxs[1][axis]) {
        return;
    }

    sky_mins_subd[0] = (int)(sky_mins[0][axis] * HALF_SKY_SUBDIVISIONS);
    sky_mins_subd[1] = (int)(sky_mins[1][axis] * HALF_SKY_SUBDIVISIONS);
    sky_maxs_subd[0] = (int)(sky_maxs[0][axis] * HALF_SKY_SUBDIVISIONS);
    sky_maxs_subd[1] = (int)(sky_maxs[1][axis] * HALF_SKY_SUBDIVISIONS);

    if (sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS) sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
    else if (sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS) sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
    if (sky_mins_subd[1] < -HALF_SKY_SUBDIVISIONS) sky_mins_subd[1] = -HALF_SKY_SUBDIVISIONS;
    else if (sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS) sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;

    if (sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS) sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
    else if (sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS) sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
    if (sky_maxs_subd[1] < -HALF_SKY_SUBDIVISIONS) sky_maxs_subd[1] = -HALF_SKY_SUBDIVISIONS;
    else if (sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS) sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;

    descSet = VK_GetDescriptorSetForImage(image);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipelineIdx]);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);

    baseVert = tess.numVertexes;
    baseIdx = tess.numIndexes;

    tHeight = sky_maxs_subd[1] - sky_mins_subd[1] + 1;
    sWidth = sky_maxs_subd[0] - sky_mins_subd[0] + 1;

    for (t = 0; t < tHeight; t++) {
        for (s = 0; s < sWidth; s++) {
            float s_norm = (sky_mins_subd[0] + s) / (float)HALF_SKY_SUBDIVISIONS;
            float t_norm = (sky_mins_subd[1] + t) / (float)HALF_SKY_SUBDIVISIONS;
            vec3_t skyVec;
            float st[2];

            VK_MakeSkyVec(s_norm, t_norm, axis, st, skyVec);
            VectorAdd(skyVec, backEnd.viewParms.or.origin, tess.xyz[tess.numVertexes]);
            tess.texCoords[tess.numVertexes][0][0] = st[0];
            tess.texCoords[tess.numVertexes][0][1] = st[1];
            tess.texCoords[tess.numVertexes][1][0] = 0.0f;
            tess.texCoords[tess.numVertexes][1][1] = 0.0f;
            VectorSet(tess.normal[tess.numVertexes], 0.0f, 0.0f, 1.0f);
            tess.vertexColors[tess.numVertexes][0] = 255;
            tess.vertexColors[tess.numVertexes][1] = 255;
            tess.vertexColors[tess.numVertexes][2] = 255;
            tess.vertexColors[tess.numVertexes][3] = 255;
            tess.numVertexes++;
        }
    }

    for (t = 0; t < tHeight - 1; t++) {
        for (s = 0; s < sWidth - 1; s++) {
            tess.indexes[tess.numIndexes++] = baseVert + s + t * sWidth;
            tess.indexes[tess.numIndexes++] = baseVert + s + (t + 1) * sWidth;
            tess.indexes[tess.numIndexes++] = baseVert + s + 1 + t * sWidth;

            tess.indexes[tess.numIndexes++] = baseVert + s + (t + 1) * sWidth;
            tess.indexes[tess.numIndexes++] = baseVert + s + 1 + (t + 1) * sWidth;
            tess.indexes[tess.numIndexes++] = baseVert + s + 1 + t * sWidth;
        }
    }

    VK_DrawTessRange(baseVert, baseIdx);

    tess.numVertexes = baseVert;
    tess.numIndexes = baseIdx;
}

static void VK_DrawSkyBoxFaces(const shader_t *shader, image_t *images[6],
                               VkCommandBuffer cmd, vk_push_constants_t *pc,
                               int pipelineIdx) {
    static const int sky_texorder[6] = {0, 2, 1, 3, 4, 5};
    int i;

    vk_sky_min = 0.0f;
    vk_sky_max = 1.0f;

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
    int i, v;
    int baseVert, baseIdx;

    if (!shader->sky.cloudHeight) {
        return;
    }

    tess.numIndexes = 0;
    tess.numVertexes = 0;
    R_BuildCloudData(&tess);

    if (tess.numIndexes <= 0 || tess.numVertexes <= 0) {
        return;
    }

    for (v = 0; v < tess.numVertexes; v++) {
        VectorSet(tess.normal[v], 0.0f, 0.0f, 1.0f);
        tess.texCoords[v][1][0] = 0.0f;
        tess.texCoords[v][1][1] = 0.0f;
        tess.vertexColors[v][0] = 255;
        tess.vertexColors[v][1] = 255;
        tess.vertexColors[v][2] = 255;
        tess.vertexColors[v][3] = 255;
    }

    baseVert = 0;
    baseIdx = 0;

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

        vkCmdPushConstants(cmd, vk_state.pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(*pc), pc);

        pipeIdx = VK_StageIsBlended(stage) ? VK_PipelineForStage(stage) : VK_PIPELINE_SKY;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipeIdx]);

        descSet = VK_StageDescriptorSet(shader, stage);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vk_state.pipelineLayout, 0, 1, &descSet, 0, NULL);

        VK_DrawTessRange(baseVert, baseIdx);
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

        vkCmdPushConstants(cmd, vk_state.pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(pc), &pc);

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

        vkCmdPushConstants(cmd, vk_state.pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(pc), &pc);

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
        float proj[16];
        float mvp[16];
        int pipeIdx;
        VkDescriptorSet descSet;

        if (!stage->active) {
            continue;
        }
        if (stage->bundle[0].isLightmap) {
            continue;
        }

        VK_SetStageStateFromShader(shader, stage);

        memcpy(proj, tr.viewParms.projectionMatrix, sizeof(proj));
        VK_ConvertProjectionMatrixToVulkan(proj);
        VK_MatrixMulQ3Clip(mvp, proj, backEnd.or.modelMatrix);
        VK_FillPushConstants(mvp, shader, &pc);

        pipeIdx = VK_PipelineForStage(stage);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state.pipelines[pipeIdx]);

        vkCmdPushConstants(cmd, vk_state.pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(pc), &pc);

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

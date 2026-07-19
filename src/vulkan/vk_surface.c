#include "vk_local.h"
#include "../cgame/tr_types.h"
#include <string.h>

#define VK_TEMP_VBO_SIZE (32 * 1024 * 1024)

vk_dynamic_vbo_t vk_dyn;
int vk_worldDrawCount;
int vk_worldVboFailCount;

void VK_DestroyDynamicVBO(void) {
    if (vk_dyn.buffer) vkDestroyBuffer(vk_state.dev, vk_dyn.buffer, NULL);
    if (vk_dyn.memory) vkFreeMemory(vk_state.dev, vk_dyn.memory, NULL);
    memset(&vk_dyn, 0, sizeof(vk_dyn));
}

qboolean VK_InitDynamicVBO(void) {
    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkMemoryRequirements req;

    bci.size = VK_TEMP_VBO_SIZE;
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(vk_state.dev, &bci, NULL, &vk_dyn.buffer);

    vkGetBufferMemoryRequirements(vk_state.dev, vk_dyn.buffer, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = VK_FindMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(vk_state.dev, &ai, NULL, &vk_dyn.memory) != VK_SUCCESS)
        return qfalse;

    vkBindBufferMemory(vk_state.dev, vk_dyn.buffer, vk_dyn.memory, 0);
    vkMapMemory(vk_state.dev, vk_dyn.memory, 0, VK_TEMP_VBO_SIZE, 0, &vk_dyn.mapped);
    vk_dyn.offset = 0;
    return qtrue;
}

static int VK_DynamicVBOFrameBase(void) {
    return (int)vk_state.frameIndex * (VK_TEMP_VBO_SIZE / VK_MAX_FRAMES_IN_FLIGHT);
}

static int VK_DynamicVBOFrameLimit(void) {
    return VK_DynamicVBOFrameBase() + (VK_TEMP_VBO_SIZE / VK_MAX_FRAMES_IN_FLIGHT);
}

static void VK_NoteVboFail(int size, int frameLimit) {
    (void)size;
    (void)frameLimit;
    vk_worldVboFailCount++;
}

void VK_ResetDynamicVBO(void) {
    int frameBase;

    frameBase = VK_DynamicVBOFrameBase();
    vk_dyn.offset = frameBase;
}

int VK_ReserveDynamicVBO(int size) {
    int frameLimit;

    frameLimit = VK_DynamicVBOFrameLimit();
    if (size <= 0 || vk_dyn.offset + size > frameLimit) {
        VK_NoteVboFail(size, frameLimit);
        return -1;
    }
    int offset = vk_dyn.offset;
    vk_dyn.offset += size;
    return offset;
}

/* ---------------------------------------------------------------------------
 * Static world VBO/IBO
 *
 * All SF_FACE / SF_GRID / SF_TRIANGLES world surfaces are immutable after map
 * load, so they are uploaded once into a device-local vertex/index buffer
 * instead of being re-streamed through the dynamic VBO every draw call.
 * ------------------------------------------------------------------------- */

vk_world_static_t vk_world;

static uint32_t VK_WorldHashKey(const void *p) {
    uintptr_t v = (uintptr_t)p;
    return (uint32_t)((v >> 4) ^ (v >> 20));
}

static void VK_WorldHashInsert(void *surfData, uint32_t firstIndex,
                               int32_t vertexOffset, uint32_t indexCount) {
    uint32_t idx;

    if (!vk_world.hash || vk_world.hashSize == 0) {
        return;
    }

    idx = VK_WorldHashKey(surfData) & (vk_world.hashSize - 1);
    while (vk_world.hash[idx].surfData) {
        idx = (idx + 1) & (vk_world.hashSize - 1);
    }
    vk_world.hash[idx].surfData = surfData;
    vk_world.hash[idx].firstIndex = firstIndex;
    vk_world.hash[idx].vertexOffset = vertexOffset;
    vk_world.hash[idx].indexCount = indexCount;
    vk_world.surfCount++;
}

static vk_world_surf_t *VK_WorldFindSurf(void *surfData) {
    uint32_t idx;

    if (!vk_world.built || !vk_world.hash || vk_world.hashSize == 0) {
        return NULL;
    }

    idx = VK_WorldHashKey(surfData) & (vk_world.hashSize - 1);
    while (vk_world.hash[idx].surfData) {
        if (vk_world.hash[idx].surfData == surfData) {
            return &vk_world.hash[idx];
        }
        idx = (idx + 1) & (vk_world.hashSize - 1);
    }
    return NULL;
}

void VK_DestroyWorldStaticBuffers(void) {
    /* In-flight frames may still be drawing from these buffers (map loading
     * happens while the loading screen keeps rendering), so wait for the GPU
     * before destroying them. */
    if ((vk_world.vbo || vk_world.ibo) && vk_state.dev) {
        vkDeviceWaitIdle(vk_state.dev);
    }
    if (vk_world.vbo) {
        vkDestroyBuffer(vk_state.dev, vk_world.vbo, NULL);
    }
    if (vk_world.vboMemory) {
        vkFreeMemory(vk_state.dev, vk_world.vboMemory, NULL);
    }
    if (vk_world.ibo) {
        vkDestroyBuffer(vk_state.dev, vk_world.ibo, NULL);
    }
    if (vk_world.iboMemory) {
        vkFreeMemory(vk_state.dev, vk_world.iboMemory, NULL);
    }
    free(vk_world.cpuIndices);
    free(vk_world.hash);
    memset(&vk_world, 0, sizeof(vk_world));
}

const vk_world_surf_t *VK_WorldGetStaticSurf(void *surfData) {
    return VK_WorldFindSurf(surfData);
}

const uint32_t *VK_WorldGetCpuIndices(void) {
    return vk_world.cpuIndices;
}

static qboolean VK_CreateDeviceLocalBuffer(const void *data, VkDeviceSize size,
                                           VkBufferUsageFlags usage,
                                           VkBuffer *outBuf,
                                           VkDeviceMemory *outMem) {
    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkMemoryRequirements req;
    VkBuffer stagingBuf = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    void *mapped;
    VkCommandBuffer cmd;
    VkCommandBufferBeginInfo cbbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkBufferCopy copy = { 0 };

    *outBuf = VK_NULL_HANDLE;
    *outMem = VK_NULL_HANDLE;

    bci.size = size;
    bci.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vk_state.dev, &bci, NULL, outBuf) != VK_SUCCESS) {
        return qfalse;
    }

    vkGetBufferMemoryRequirements(vk_state.dev, *outBuf, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = VK_FindMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(vk_state.dev, &ai, NULL, outMem) != VK_SUCCESS) {
        vkDestroyBuffer(vk_state.dev, *outBuf, NULL);
        *outBuf = VK_NULL_HANDLE;
        return qfalse;
    }
    vkBindBufferMemory(vk_state.dev, *outBuf, *outMem, 0);

    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (vkCreateBuffer(vk_state.dev, &bci, NULL, &stagingBuf) != VK_SUCCESS) {
        goto fail;
    }
    vkGetBufferMemoryRequirements(vk_state.dev, stagingBuf, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = VK_FindMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(vk_state.dev, &ai, NULL, &stagingMem) != VK_SUCCESS) {
        goto fail;
    }
    vkBindBufferMemory(vk_state.dev, stagingBuf, stagingMem, 0);
    vkMapMemory(vk_state.dev, stagingMem, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(vk_state.dev, stagingMem);

    cmd = vk_state.uploadCmd;
    vkResetCommandBuffer(cmd, 0);
    vkBeginCommandBuffer(cmd, &cbbi);
    copy.size = size;
    vkCmdCopyBuffer(cmd, stagingBuf, *outBuf, 1, &copy);
    vkEndCommandBuffer(cmd);

    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(vk_state.gfxQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(vk_state.gfxQueue);

    vkDestroyBuffer(vk_state.dev, stagingBuf, NULL);
    vkFreeMemory(vk_state.dev, stagingMem, NULL);
    return qtrue;

fail:
    vkDestroyBuffer(vk_state.dev, stagingBuf, NULL);
    vkFreeMemory(vk_state.dev, stagingMem, NULL);
    if (*outBuf) {
        vkDestroyBuffer(vk_state.dev, *outBuf, NULL);
        *outBuf = VK_NULL_HANDLE;
    }
    if (*outMem) {
        vkFreeMemory(vk_state.dev, *outMem, NULL);
        *outMem = VK_NULL_HANDLE;
    }
    return qfalse;
}

static void VK_AppendFaceToStaticBuffers(const srfSurfaceFace_t *face,
                                         drawVert_t *outVerts,
                                         uint32_t *outIndices) {
    int i;
    int numVerts = face->numPoints;
    int numIdx = face->numIndices;
    const int *idxSrc = (const int *)((const uint8_t *)face + face->ofsIndices);

    for (i = 0; i < numVerts; i++) {
        const float *v = face->points[i];
        drawVert_t *dv = &outVerts[i];
        VectorCopy(v, dv->xyz);
        dv->st[0] = v[3];
        dv->st[1] = v[4];
        dv->lightmap[0] = v[5];
        dv->lightmap[1] = v[6];
        VectorCopy(face->plane.normal, dv->normal);
        *(int *)dv->color = *(const int *)&v[7];
        if (*(int *)dv->color == 0) {
            dv->color[0] = 255;
            dv->color[1] = 255;
            dv->color[2] = 255;
            dv->color[3] = 255;
        }
    }

    for (i = 0; i < numIdx; i++) {
        outIndices[i] = (uint32_t)idxSrc[i];
    }
}

static void VK_AppendGridToStaticBuffers(const srfGridMesh_t *grid,
                                         drawVert_t *outVerts,
                                         uint32_t *outIndices) {
    int x, y;
    int idx = 0;
    int w = grid->width;
    int h = grid->height;
    int numVerts = w * h;

    memcpy(outVerts, grid->verts, numVerts * sizeof(drawVert_t));

    for (y = 0; y < h - 1; y++) {
        for (x = 0; x < w - 1; x++) {
            int a = y * w + x;
            int b = y * w + x + 1;
            int c = (y + 1) * w + x;
            int d = (y + 1) * w + x + 1;
            outIndices[idx++] = (uint32_t)a;
            outIndices[idx++] = (uint32_t)c;
            outIndices[idx++] = (uint32_t)b;
            outIndices[idx++] = (uint32_t)b;
            outIndices[idx++] = (uint32_t)c;
            outIndices[idx++] = (uint32_t)d;
        }
    }
}

void VK_BuildWorldStaticBuffers(void) {
    int i;
    int totalVerts = 0;
    int totalIdx = 0;
    int worldSurfs = 0;
    int vertOff = 0;
    int idxOff = 0;
    drawVert_t *verts = NULL;
    uint32_t *indices = NULL;

    if (!vk_active || !tr.world) {
        return;
    }

    VK_DestroyWorldStaticBuffers();

    /* First pass: count geometry and hash size. */
    for (i = 0; i < tr.world->numsurfaces; i++) {
        const msurface_t *surf = &tr.world->surfaces[i];
        const void *data = surf->data;

        if (!data) {
            continue;
        }

        switch (*(const surfaceType_t *)data) {
        case SF_FACE: {
            const srfSurfaceFace_t *face = (const srfSurfaceFace_t *)data;
            totalVerts += face->numPoints;
            totalIdx += face->numIndices;
            worldSurfs++;
            break;
        }
        case SF_GRID: {
            const srfGridMesh_t *grid = (const srfGridMesh_t *)data;
            totalVerts += grid->width * grid->height;
            totalIdx += (grid->width - 1) * (grid->height - 1) * 6;
            worldSurfs++;
            break;
        }
        case SF_TRIANGLES: {
            const srfTriangles_t *tri = (const srfTriangles_t *)data;
            totalVerts += tri->numVerts;
            totalIdx += tri->numIndexes;
            worldSurfs++;
            break;
        }
        default:
            break;
        }
    }

    if (totalVerts == 0 || totalIdx == 0 || worldSurfs == 0) {
        return;
    }

    vk_world.hashSize = 1;
    while (vk_world.hashSize < (uint32_t)(worldSurfs * 2)) {
        vk_world.hashSize <<= 1;
    }
    vk_world.hash = calloc(vk_world.hashSize, sizeof(*vk_world.hash));
    if (!vk_world.hash) {
        return;
    }

    verts = malloc(totalVerts * sizeof(*verts));
    indices = malloc(totalIdx * sizeof(*indices));
    if (!verts || !indices) {
        free(verts);
        free(indices);
        VK_DestroyWorldStaticBuffers();
        return;
    }

    /* Second pass: fill CPU-side arrays and record per-surface offsets. */
    for (i = 0; i < tr.world->numsurfaces; i++) {
        const msurface_t *surf = &tr.world->surfaces[i];
        void *data = surf->data;
        int numVerts = 0;
        int numIdx = 0;

        if (!data) {
            continue;
        }

        switch (*(const surfaceType_t *)data) {
        case SF_FACE: {
            const srfSurfaceFace_t *face = (const srfSurfaceFace_t *)data;
            numVerts = face->numPoints;
            numIdx = face->numIndices;
            VK_AppendFaceToStaticBuffers(face, verts + vertOff, indices + idxOff);
            break;
        }
        case SF_GRID: {
            const srfGridMesh_t *grid = (const srfGridMesh_t *)data;
            numVerts = grid->width * grid->height;
            numIdx = (grid->width - 1) * (grid->height - 1) * 6;
            VK_AppendGridToStaticBuffers(grid, verts + vertOff, indices + idxOff);
            break;
        }
        case SF_TRIANGLES: {
            const srfTriangles_t *tri = (const srfTriangles_t *)data;
            numVerts = tri->numVerts;
            numIdx = tri->numIndexes;
            memcpy(verts + vertOff, tri->verts, numVerts * sizeof(drawVert_t));
            for (int j = 0; j < numIdx; j++) {
                indices[idxOff + j] = (uint32_t)tri->indexes[j];
            }
            break;
        }
        default:
            break;
        }

        if (numVerts > 0 && numIdx > 0) {
            VK_WorldHashInsert(data, (uint32_t)idxOff, vertOff, (uint32_t)numIdx);
            vertOff += numVerts;
            idxOff += numIdx;
        }
    }

    vk_world.vboSize = (VkDeviceSize)totalVerts * sizeof(*verts);
    vk_world.iboSize = (VkDeviceSize)totalIdx * sizeof(*indices);

    if (!VK_CreateDeviceLocalBuffer(verts, vk_world.vboSize,
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    &vk_world.vbo, &vk_world.vboMemory) ||
        !VK_CreateDeviceLocalBuffer(indices, vk_world.iboSize,
                                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                    &vk_world.ibo, &vk_world.iboMemory)) {
        free(verts);
        free(indices);
        VK_DestroyWorldStaticBuffers();
        return;
    }

    free(verts);
    /* Keep a CPU-side copy of the world indices: the draw-call batcher
     * re-bases them into the dynamic IBO so whole batches of world surfaces
     * can be drawn with a single vkCmdDrawIndexed. */
    vk_world.cpuIndices = indices;
    vk_world.cpuIndexCount = (uint32_t)totalIdx;
    vk_world.totalVerts = (uint32_t)totalVerts;
    vk_world.built = qtrue;

    ri.Printf(PRINT_ALL,
              "VK_BuildWorldStaticBuffers: %u world surfaces, %d verts, %d idx (VBO %.1f MB, IBO %.1f MB)\n",
              vk_world.surfCount, totalVerts, totalIdx,
              (double)vk_world.vboSize / (1024.0 * 1024.0),
              (double)vk_world.iboSize / (1024.0 * 1024.0));
}

static void VK_DecodeLatLongNormal(vec3_t outNormal, short latLong) {
    unsigned lat, lng;

    lat = (latLong >> 8) & 0xff;
    lng = latLong & 0xff;
    lat *= (FUNCTABLE_SIZE / 256);
    lng *= (FUNCTABLE_SIZE / 256);

    outNormal[0] = tr.sinTable[(lat + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK] * tr.sinTable[lng];
    outNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
    outNormal[2] = tr.sinTable[(lng + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK];
}

static void VK_FillMeshVertexColor(byte *color, const vec3_t normal) {
    trRefEntity_t *ent;
    float incoming;
    int ambientLightInt;
    vec3_t ambientLight;
    vec3_t directedLight;
    vec3_t lightDir;
    int r, g, b;

    if (!backEnd.currentEntity) {
        color[0] = color[1] = color[2] = 255;
        color[3] = 255;
        return;
    }

    ent = backEnd.currentEntity;
    ambientLightInt = ent->ambientLightInt;
    VectorCopy(ent->ambientLight, ambientLight);
    VectorCopy(ent->directedLight, directedLight);
    VectorCopy(ent->lightDir, lightDir);

    incoming = DotProduct(normal, lightDir);
    if (incoming <= 0.0f) {
        *(int *)color = ambientLightInt;
        return;
    }

    r = myftol(ambientLight[0] + incoming * directedLight[0]);
    g = myftol(ambientLight[1] + incoming * directedLight[1]);
    b = myftol(ambientLight[2] + incoming * directedLight[2]);
    if (r > 255) {
        r = 255;
    }
    if (g > 255) {
        g = 255;
    }
    if (b > 255) {
        b = 255;
    }

    color[0] = (byte)r;
    color[1] = (byte)g;
    color[2] = (byte)b;
    color[3] = 255;
}

static void VK_LerpMD3Mesh(md3Surface_t *surf, float backlerp, drawVert_t *outVerts) {
    short *oldXyz, *newXyz, *oldNormals, *newNormals;
    float oldXyzScale, newXyzScale;
    float oldNormalScale, newNormalScale;
    int vertNum;
    int numVerts;

    numVerts = surf->numVerts;
    newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
        + (backEnd.currentEntity->e.frame * surf->numVerts * 4);
    newNormals = newXyz + 3;

    newXyzScale = MD3_XYZ_SCALE * (1.0f - backlerp);
    newNormalScale = 1.0f - backlerp;

    if (backlerp == 0.0f) {
        for (vertNum = 0; vertNum < numVerts; vertNum++,
            newXyz += 4, newNormals += 4) {
            outVerts[vertNum].xyz[0] = newXyz[0] * newXyzScale;
            outVerts[vertNum].xyz[1] = newXyz[1] * newXyzScale;
            outVerts[vertNum].xyz[2] = newXyz[2] * newXyzScale;
            VK_DecodeLatLongNormal(outVerts[vertNum].normal, newNormals[0]);
        }
    } else {
        oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
            + (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
        oldNormals = oldXyz + 3;
        oldXyzScale = MD3_XYZ_SCALE * backlerp;
        oldNormalScale = backlerp;

        for (vertNum = 0; vertNum < numVerts; vertNum++,
            oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4) {
            vec3_t oldNormal, newNormal;

            outVerts[vertNum].xyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
            outVerts[vertNum].xyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
            outVerts[vertNum].xyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

            VK_DecodeLatLongNormal(newNormal, newNormals[0]);
            VK_DecodeLatLongNormal(oldNormal, oldNormals[0]);
            outVerts[vertNum].normal[0] = oldNormal[0] * oldNormalScale + newNormal[0] * newNormalScale;
            outVerts[vertNum].normal[1] = oldNormal[1] * oldNormalScale + newNormal[1] * newNormalScale;
            outVerts[vertNum].normal[2] = oldNormal[2] * oldNormalScale + newNormal[2] * newNormalScale;
            VectorNormalize(outVerts[vertNum].normal);
        }
    }
}

static void VK_LerpMDCMesh(mdcSurface_t *surf, float backlerp, drawVert_t *outVerts) {
    short *oldXyz, *newXyz, *oldNormals, *newNormals;
    float oldXyzScale, newXyzScale;
    float oldNormalScale, newNormalScale;
    int vertNum;
    int numVerts;
    int oldBase, newBase;
    short *oldComp = NULL, *newComp = NULL;
    mdcXyzCompressed_t *oldXyzComp = NULL, *newXyzComp = NULL;
    vec3_t oldOfsVec, newOfsVec;
    qboolean hasComp;

    numVerts = surf->numVerts;
    newBase = (int)*((short *)((byte *)surf + surf->ofsFrameBaseFrames)
        + backEnd.currentEntity->e.frame);
    newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
        + (newBase * surf->numVerts * 4);
    newNormals = newXyz + 3;

    hasComp = (surf->numCompFrames > 0);
    if (hasComp) {
        newComp = ((short *)((byte *)surf + surf->ofsFrameCompFrames)
            + backEnd.currentEntity->e.frame);
        if (*newComp >= 0) {
            newXyzComp = (mdcXyzCompressed_t *)((byte *)surf + surf->ofsXyzCompressed)
                + (*newComp * surf->numVerts);
        }
    }

    newXyzScale = MD3_XYZ_SCALE * (1.0f - backlerp);
    newNormalScale = 1.0f - backlerp;

    if (backlerp == 0.0f) {
        for (vertNum = 0; vertNum < numVerts; vertNum++,
            newXyz += 4, newNormals += 4) {
            outVerts[vertNum].xyz[0] = newXyz[0] * newXyzScale;
            outVerts[vertNum].xyz[1] = newXyz[1] * newXyzScale;
            outVerts[vertNum].xyz[2] = newXyz[2] * newXyzScale;

            if (hasComp && newComp && *newComp >= 0) {
                R_MDC_DecodeXyzCompressed(newXyzComp->ofsVec, newOfsVec, outVerts[vertNum].normal);
                newXyzComp++;
                VectorAdd(outVerts[vertNum].xyz, newOfsVec, outVerts[vertNum].xyz);
            } else {
                VK_DecodeLatLongNormal(outVerts[vertNum].normal, newNormals[0]);
            }
        }
    } else {
        oldBase = (int)*((short *)((byte *)surf + surf->ofsFrameBaseFrames)
            + backEnd.currentEntity->e.oldframe);
        oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
            + (oldBase * surf->numVerts * 4);
        oldNormals = oldXyz + 3;

        if (hasComp) {
            oldComp = ((short *)((byte *)surf + surf->ofsFrameCompFrames)
                + backEnd.currentEntity->e.oldframe);
            if (*oldComp >= 0) {
                oldXyzComp = (mdcXyzCompressed_t *)((byte *)surf + surf->ofsXyzCompressed)
                    + (*oldComp * surf->numVerts);
            }
        }

        oldXyzScale = MD3_XYZ_SCALE * backlerp;
        oldNormalScale = backlerp;

        for (vertNum = 0; vertNum < numVerts; vertNum++,
            oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4) {
            vec3_t oldNormal, newNormal;

            outVerts[vertNum].xyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
            outVerts[vertNum].xyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
            outVerts[vertNum].xyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

            if (hasComp && newComp && *newComp >= 0) {
                R_MDC_DecodeXyzCompressed(newXyzComp->ofsVec, newOfsVec, newNormal);
                newXyzComp++;
                VectorMA(outVerts[vertNum].xyz, 1.0f - backlerp, newOfsVec, outVerts[vertNum].xyz);
            } else {
                VK_DecodeLatLongNormal(newNormal, newNormals[0]);
            }

            if (hasComp && oldComp && *oldComp >= 0) {
                R_MDC_DecodeXyzCompressed(oldXyzComp->ofsVec, oldOfsVec, oldNormal);
                oldXyzComp++;
                VectorMA(outVerts[vertNum].xyz, backlerp, oldOfsVec, outVerts[vertNum].xyz);
            } else {
                VK_DecodeLatLongNormal(oldNormal, oldNormals[0]);
            }

            outVerts[vertNum].normal[0] = oldNormal[0] * oldNormalScale + newNormal[0] * newNormalScale;
            outVerts[vertNum].normal[1] = oldNormal[1] * oldNormalScale + newNormal[1] * newNormalScale;
            outVerts[vertNum].normal[2] = oldNormal[2] * oldNormalScale + newNormal[2] * newNormalScale;
            VectorNormalize(outVerts[vertNum].normal);
        }
    }
}

void VK_SurfaceFace(void *surfData) {
    srfSurfaceFace_t *face = (srfSurfaceFace_t *)surfData;
    VkCommandBuffer cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    vk_world_surf_t *ws = VK_WorldFindSurf(surfData);
    int numVerts = face->numPoints;
    int numIdx = face->numIndices;
    int vboOff, iboOff;

    if (ws) {
        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, &vk_world.vbo, offsets);
        vkCmdBindIndexBuffer(cmd, vk_world.ibo, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, ws->indexCount, 1, ws->firstIndex, ws->vertexOffset, 0);
        vk_worldDrawCount++;
        return;
    }

    /* Fallback: surface was not present when the static buffers were built. */
    vboOff = VK_ReserveDynamicVBO(numVerts * sizeof(drawVert_t));
    iboOff = VK_ReserveDynamicVBO(numIdx * sizeof(int));
    if (vboOff < 0 || iboOff < 0) {
        return;
    }

    uint8_t *verts = (uint8_t *)vk_dyn.mapped + vboOff;
    int *idxDst = (int *)((uint8_t *)vk_dyn.mapped + iboOff);

    for (int i = 0; i < numVerts; i++) {
        float *v = face->points[i];
        drawVert_t *dv = (drawVert_t *)(verts + i * sizeof(drawVert_t));
        VectorCopy(v, dv->xyz);
        dv->st[0] = v[3];
        dv->st[1] = v[4];
        dv->lightmap[0] = v[5];
        dv->lightmap[1] = v[6];
        VectorCopy(face->plane.normal, dv->normal);
        *(int *)dv->color = *(int *)&v[7];
        if (*(int *)dv->color == 0) {
            dv->color[0] = 255;
            dv->color[1] = 255;
            dv->color[2] = 255;
            dv->color[3] = 255;
        }
    }

    int *idxSrc = (int *)((uint8_t *)face + face->ofsIndices);
    memcpy(idxDst, idxSrc, numIdx * sizeof(int));

    VkDeviceSize offsets[1] = { (VkDeviceSize)vboOff };
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_dyn.buffer, offsets);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, numIdx, 1, 0, 0, 0);
    vk_worldDrawCount++;
}

void VK_SurfaceGrid(void *surfData) {
    srfGridMesh_t *grid = (srfGridMesh_t *)surfData;
    VkCommandBuffer cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    vk_world_surf_t *ws = VK_WorldFindSurf(surfData);
    int w = grid->width, h = grid->height;
    int numVerts = w * h;
    int numIdx = (w - 1) * (h - 1) * 6;
    int vboOff, iboOff;

    if (ws) {
        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, &vk_world.vbo, offsets);
        vkCmdBindIndexBuffer(cmd, vk_world.ibo, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, ws->indexCount, 1, ws->firstIndex, ws->vertexOffset, 0);
        vk_worldDrawCount++;
        return;
    }

    /* Fallback: surface was not present when the static buffers were built. */
    vboOff = VK_ReserveDynamicVBO(numVerts * sizeof(drawVert_t));
    iboOff = VK_ReserveDynamicVBO(numIdx * sizeof(int));
    if (vboOff < 0 || iboOff < 0) {
        return;
    }

    uint8_t *verts = (uint8_t *)vk_dyn.mapped + vboOff;
    int *idxDst = (int *)((uint8_t *)vk_dyn.mapped + iboOff);

    memcpy(verts, grid->verts, numVerts * sizeof(drawVert_t));

    int idx = 0;
    for (int y = 0; y < h - 1; y++) {
        for (int x = 0; x < w - 1; x++) {
            int a = y * w + x;
            int b = y * w + x + 1;
            int c = (y + 1) * w + x;
            int d = (y + 1) * w + x + 1;
            idxDst[idx++] = a; idxDst[idx++] = c; idxDst[idx++] = b;
            idxDst[idx++] = b; idxDst[idx++] = c; idxDst[idx++] = d;
        }
    }

    VkDeviceSize offsets[1] = { (VkDeviceSize)vboOff };
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_dyn.buffer, offsets);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, numIdx, 1, 0, 0, 0);
    vk_worldDrawCount++;
}

void VK_SurfaceTriangles(void *surfData) {
    srfTriangles_t *tri = (srfTriangles_t *)surfData;
    VkCommandBuffer cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    vk_world_surf_t *ws = VK_WorldFindSurf(surfData);
    int vboOff, iboOff;

    if (ws) {
        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, &vk_world.vbo, offsets);
        vkCmdBindIndexBuffer(cmd, vk_world.ibo, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, ws->indexCount, 1, ws->firstIndex, ws->vertexOffset, 0);
        vk_worldDrawCount++;
        return;
    }

    /* Fallback: surface was not present when the static buffers were built. */
    vboOff = VK_ReserveDynamicVBO(tri->numVerts * sizeof(drawVert_t));
    iboOff = VK_ReserveDynamicVBO(tri->numIndexes * sizeof(int));
    if (vboOff < 0 || iboOff < 0) {
        return;
    }

    uint8_t *verts = (uint8_t *)vk_dyn.mapped + vboOff;
    memcpy(verts, tri->verts, tri->numVerts * sizeof(drawVert_t));

    int *idxDst = (int *)((uint8_t *)vk_dyn.mapped + iboOff);
    memcpy(idxDst, tri->indexes, tri->numIndexes * sizeof(int));

    VkDeviceSize offsets[1] = { (VkDeviceSize)vboOff };
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_dyn.buffer, offsets);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, tri->numIndexes, 1, 0, 0, 0);
    vk_worldDrawCount++;
}

void VK_SurfacePoly(void *surfData) {
    srfPoly_t *poly = (srfPoly_t *)surfData;
    VkCommandBuffer cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    int numIndexes;
    int idx;
    int i;

    if (poly->numVerts < 3) {
        return;
    }

    numIndexes = 3 * (poly->numVerts - 2);

    int vboOff = VK_ReserveDynamicVBO(poly->numVerts * sizeof(drawVert_t));
    int iboOff = VK_ReserveDynamicVBO(numIndexes * sizeof(int));
    if (vboOff < 0 || iboOff < 0) {
        return;
    }

    uint8_t *verts = (uint8_t *)vk_dyn.mapped + vboOff;
    int *idxDst = (int *)((uint8_t *)vk_dyn.mapped + iboOff);

    for (i = 0; i < poly->numVerts; i++) {
        drawVert_t *dv = (drawVert_t *)(verts + i * sizeof(drawVert_t));
        VectorCopy(poly->verts[i].xyz, dv->xyz);
        dv->st[0] = poly->verts[i].st[0]; dv->st[1] = poly->verts[i].st[1];
        dv->lightmap[0] = 0; dv->lightmap[1] = 0;
        VectorClear(dv->normal);
        dv->color[0] = poly->verts[i].modulate[0];
        dv->color[1] = poly->verts[i].modulate[1];
        dv->color[2] = poly->verts[i].modulate[2];
        dv->color[3] = poly->verts[i].modulate[3];
    }

    idx = 0;
    for (i = 0; i < poly->numVerts - 2; i++) {
        idxDst[idx++] = 0;
        idxDst[idx++] = i + 1;
        idxDst[idx++] = i + 2;
    }

    VkDeviceSize offsets[1] = { (VkDeviceSize)vboOff };
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_dyn.buffer, offsets);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, numIndexes, 1, 0, 0, 0);
    vk_worldDrawCount++;
}

void VK_DrawTessRange(int baseVert, int baseIdx) {
    int numVerts;
    int numIdx;
    int vboOff;
    int iboOff;
    drawVert_t *verts;
    int *idxDst;
    VkCommandBuffer cmd;
    VkDeviceSize offsets[1];
    int i;
    int j;

    numVerts = tess.numVertexes - baseVert;
    numIdx = tess.numIndexes - baseIdx;
    if (numVerts <= 0 || numIdx <= 0) {
        return;
    }

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    if (!cmd) {
        return;
    }

    vboOff = VK_ReserveDynamicVBO(numVerts * (int)sizeof(drawVert_t));
    iboOff = VK_ReserveDynamicVBO(numIdx * (int)sizeof(int));
    if (vboOff < 0 || iboOff < 0) {
        return;
    }
    verts = (drawVert_t *)((uint8_t *)vk_dyn.mapped + vboOff);
    idxDst = (int *)((uint8_t *)vk_dyn.mapped + iboOff);

    for (i = 0; i < numVerts; i++) {
        j = baseVert + i;
        VectorCopy(tess.xyz[j], verts[i].xyz);
        VectorCopy(tess.normal[j], verts[i].normal);
        if (VectorLengthSquared(verts[i].normal) < 0.01f) {
            verts[i].normal[0] = 0.0f;
            verts[i].normal[1] = 0.0f;
            verts[i].normal[2] = 1.0f;
        }
        verts[i].st[0] = tess.texCoords[j][0][0];
        verts[i].st[1] = tess.texCoords[j][0][1];
        verts[i].lightmap[0] = tess.texCoords[j][1][0];
        verts[i].lightmap[1] = tess.texCoords[j][1][1];
        *(int *)verts[i].color = *(int *)&tess.vertexColors[j];
        if (*(int *)verts[i].color == 0) {
            VK_FillMeshVertexColor(verts[i].color, verts[i].normal);
        }
    }

    for (i = 0; i < numIdx; i++) {
        idxDst[i] = tess.indexes[baseIdx + i] - baseVert;
    }

    offsets[0] = (VkDeviceSize)vboOff;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_dyn.buffer, offsets);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, (uint32_t)numIdx, 1, 0, 0, 0);
    vk_worldDrawCount++;
}

static void VK_SurfaceBeamTess(void) {
#define NUM_BEAM_SEGS 6
    refEntity_t *e;
    int i;
    vec3_t perpvec;
    vec3_t direction, normalized_direction;
    vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
    vec3_t oldorigin, origin;
    int vbase;

    e = &backEnd.currentEntity->e;

    VectorCopy(e->oldorigin, oldorigin);
    VectorCopy(e->origin, origin);

    normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
    normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
    normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

    if (VectorNormalize(normalized_direction) == 0.0f) {
        return;
    }

    PerpendicularVector(perpvec, normalized_direction);
    VectorScale(perpvec, 4.0f, perpvec);

    for (i = 0; i < NUM_BEAM_SEGS; i++) {
        RotatePointAroundVector(start_points[i], normalized_direction, perpvec,
            (360.0f / NUM_BEAM_SEGS) * i);
        VectorAdd(start_points[i], direction, end_points[i]);
    }

    vbase = tess.numVertexes;
    for (i = 0; i <= NUM_BEAM_SEGS; i++) {
        int idx = i % NUM_BEAM_SEGS;

        VectorCopy(start_points[idx], tess.xyz[tess.numVertexes]);
        VectorClear(tess.normal[tess.numVertexes]);
        tess.texCoords[tess.numVertexes][0][0] = 0.0f;
        tess.texCoords[tess.numVertexes][0][1] = 0.0f;
        tess.vertexColors[tess.numVertexes][0] = 255;
        tess.vertexColors[tess.numVertexes][1] = 0;
        tess.vertexColors[tess.numVertexes][2] = 0;
        tess.vertexColors[tess.numVertexes][3] = 255;
        tess.numVertexes++;

        VectorCopy(end_points[idx], tess.xyz[tess.numVertexes]);
        VectorClear(tess.normal[tess.numVertexes]);
        tess.texCoords[tess.numVertexes][0][0] = 1.0f;
        tess.texCoords[tess.numVertexes][0][1] = 1.0f;
        tess.vertexColors[tess.numVertexes][0] = 255;
        tess.vertexColors[tess.numVertexes][1] = 0;
        tess.vertexColors[tess.numVertexes][2] = 0;
        tess.vertexColors[tess.numVertexes][3] = 255;
        tess.numVertexes++;
    }

    for (i = 0; i < NUM_BEAM_SEGS * 2; i++) {
        tess.indexes[tess.numIndexes++] = vbase + i;
        tess.indexes[tess.numIndexes++] = vbase + i + 1;
        tess.indexes[tess.numIndexes++] = vbase + i + 2;
    }
#undef NUM_BEAM_SEGS
}

void VK_SurfaceMD3(void *surfData) {
    md3Surface_t *md3Surface = (md3Surface_t *)surfData;
    mdcSurface_t *mdcSurface = (mdcSurface_t *)surfData;
    surfaceType_t type = *(surfaceType_t *)surfData;
    float backlerp;
    int numVerts;
    int numIdx;
    int vboOff;
    int iboOff;
    drawVert_t *verts;
    int *idxDst;
    int *triangles;
    float *texCoords;
    VkCommandBuffer cmd;
    VkDeviceSize offsets[1];
    int j;

    if (!backEnd.currentEntity) {
        return;
    }

    if (backEnd.currentEntity->e.reFlags & REFLAG_ONLYHAND) {
        if (type == SF_MDC) {
            if (!strstr(mdcSurface->name, "hand")) {
                return;
            }
        } else {
            if (!strstr(md3Surface->name, "hand")) {
                return;
            }
        }
    }

    if (backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame) {
        backlerp = 0.0f;
    } else {
        backlerp = backEnd.currentEntity->e.backlerp;
    }

    if (type == SF_MDC) {
        numVerts = mdcSurface->numVerts;
        numIdx = mdcSurface->numTriangles * 3;
    } else {
        numVerts = md3Surface->numVerts;
        numIdx = md3Surface->numTriangles * 3;
    }
    if (numVerts <= 0 || numIdx <= 0) {
        return;
    }

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    if (!cmd) {
        return;
    }

    vboOff = VK_ReserveDynamicVBO(numVerts * (int)sizeof(drawVert_t));
    iboOff = VK_ReserveDynamicVBO(numIdx * (int)sizeof(int));
    if (vboOff < 0 || iboOff < 0) {
        return;
    }
    verts = (drawVert_t *)((uint8_t *)vk_dyn.mapped + vboOff);
    idxDst = (int *)((uint8_t *)vk_dyn.mapped + iboOff);

    if (type == SF_MDC) {
        VK_LerpMDCMesh(mdcSurface, backlerp, verts);
        texCoords = (float *)((byte *)mdcSurface + mdcSurface->ofsSt);
        triangles = (int *)((byte *)mdcSurface + mdcSurface->ofsTriangles);
    } else {
        VK_LerpMD3Mesh(md3Surface, backlerp, verts);
        texCoords = (float *)((byte *)md3Surface + md3Surface->ofsSt);
        triangles = (int *)((byte *)md3Surface + md3Surface->ofsTriangles);
    }

    for (j = 0; j < numVerts; j++) {
        verts[j].st[0] = texCoords[j * 2 + 0];
        verts[j].st[1] = texCoords[j * 2 + 1];
        verts[j].lightmap[0] = 0.0f;
        verts[j].lightmap[1] = 0.0f;
        VK_FillMeshVertexColor(verts[j].color, verts[j].normal);
    }

    for (j = 0; j < numIdx; j++) {
        idxDst[j] = triangles[j];
    }

    offsets[0] = (VkDeviceSize)vboOff;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_dyn.buffer, offsets);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, (uint32_t)numIdx, 1, 0, 0, 0);
    vk_worldDrawCount++;
}

void VK_SurfaceFlare(void *surfData) {
}

void VK_SurfaceAnim(void *surfData) {
    mdsSurface_t *surface;
    int baseVert;
    int baseIdx;

    if (!backEnd.currentEntity) {
        return;
    }

    surface = (mdsSurface_t *)surfData;
    if (backEnd.currentEntity->e.reFlags & REFLAG_ONLYHAND) {
        if (!strstr(surface->name, "hand")) {
            return;
        }
    }

    baseVert = tess.numVertexes;
    baseIdx = tess.numIndexes;

    RB_SurfaceAnim(surface);

    if (tess.numVertexes > baseVert && tess.numIndexes > baseIdx) {
        VK_DrawTessRange(baseVert, baseIdx);
    }

    tess.numVertexes = baseVert;
    tess.numIndexes = baseIdx;
}

void VK_SurfaceEntity(void *surfData) {
    refEntity_t *e;
    int baseVert;
    int baseIdx;

    if (!backEnd.currentEntity) {
        return;
    }

    e = &backEnd.currentEntity->e;
    baseVert = tess.numVertexes;
    baseIdx = tess.numIndexes;

    switch (e->reType) {
    case RT_BEAM:
        VK_SurfaceBeamTess();
        break;
    case RT_SPLASH:
    case RT_SPRITE:
    case RT_RAIL_CORE:
    case RT_RAIL_RINGS:
    case RT_LIGHTNING:
        RB_SurfaceEntity((surfaceType_t *)surfData);
        break;
    default:
        return;
    }

    if (tess.numVertexes > baseVert && tess.numIndexes > baseIdx) {
        VK_DrawTessRange(baseVert, baseIdx);
    }

    tess.numVertexes = baseVert;
    tess.numIndexes = baseIdx;
}

void VK_Skip(void *surfData) {
}

void (*vk_surfaceTable[SF_NUM_SURFACE_TYPES])(void *) = {
    VK_Skip,
    VK_Skip,
    VK_SurfaceFace,
    VK_SurfaceGrid,
    VK_SurfaceTriangles,
    VK_SurfacePoly,
    VK_SurfaceMD3,
    VK_SurfaceMD3,
    VK_SurfaceAnim,
    VK_SurfaceFlare,
    VK_SurfaceEntity,
    VK_Skip
};

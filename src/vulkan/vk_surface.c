#include "vk_local.h"
#include "vk_material.h"
#include "../cgame/tr_types.h"
#include <math.h>
#include <string.h>

#define VK_TEMP_VBO_SIZE (32 * 1024 * 1024)

vk_dynamic_vbo_t vk_dyn;
vk_buffer_t vk_meshDummyFrame;
vk_buffer_t vk_meshDummyShort;
int vk_worldDrawCount;
int vk_worldVboFailCount;

void VK_DestroyMeshDummyFrame(void) {
    VK_DestroyBuffer(&vk_meshDummyFrame);
    VK_DestroyBuffer(&vk_meshDummyShort);
}

static qboolean VK_InitHostVertexBuffer(vk_buffer_t *out, const void *data, VkDeviceSize size) {
    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkMemoryRequirements req;
    void *mapped;

    memset(out, 0, sizeof(*out));
    bci.size = size;
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vk_state.dev, &bci, NULL, &out->buffer) != VK_SUCCESS) {
        return qfalse;
    }
    vkGetBufferMemoryRequirements(vk_state.dev, out->buffer, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = VK_FindMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(vk_state.dev, &ai, NULL, &out->memory) != VK_SUCCESS) {
        vkDestroyBuffer(vk_state.dev, out->buffer, NULL);
        out->buffer = VK_NULL_HANDLE;
        return qfalse;
    }
    vkBindBufferMemory(vk_state.dev, out->buffer, out->memory, 0);
    if (vkMapMemory(vk_state.dev, out->memory, 0, size, 0, &mapped) != VK_SUCCESS) {
        VK_DestroyBuffer(out);
        return qfalse;
    }
    memcpy(mapped, data, (size_t)size);
    vkUnmapMemory(vk_state.dev, out->memory);
    out->size = size;
    return qtrue;
}

qboolean VK_InitMeshDummyFrame(void) {
    vk_meshAttrib1_t zeroAttr;
    md3XyzNormal_t zeroShort;

    memset(&zeroAttr, 0, sizeof(zeroAttr));
    memset(&zeroShort, 0, sizeof(zeroShort));
    if (!VK_InitHostVertexBuffer(&vk_meshDummyFrame, &zeroAttr, sizeof(zeroAttr))) {
        return qfalse;
    }
    if (!VK_InitHostVertexBuffer(&vk_meshDummyShort, &zeroShort, sizeof(zeroShort))) {
        VK_DestroyMeshDummyFrame();
        return qfalse;
    }
    return qtrue;
}

void VK_BindMeshVertexBuffers4(VkCommandBuffer cmd,
                               VkBuffer b0, VkDeviceSize o0,
                               VkBuffer b1, VkDeviceSize o1,
                               VkBuffer b2, VkDeviceSize o2,
                               VkBuffer b3, VkDeviceSize o3) {
    VkBuffer bufs[4];
    VkDeviceSize offs[4];

    bufs[0] = b0;
    bufs[1] = b1 ? b1 : vk_meshDummyFrame.buffer;
    bufs[2] = b2 ? b2 : vk_meshDummyShort.buffer;
    bufs[3] = b3 ? b3 : vk_meshDummyShort.buffer;
    offs[0] = o0;
    offs[1] = b1 ? o1 : 0;
    offs[2] = b2 ? o2 : 0;
    offs[3] = b3 ? o3 : 0;
    vkCmdBindVertexBuffers(cmd, 0, 4, bufs, offs);
}

void VK_BindMeshVertexBuffers(VkCommandBuffer cmd, VkBuffer primary, VkDeviceSize primaryOff,
                              VkBuffer oldFrame, VkDeviceSize oldFrameOff) {
    VK_BindMeshVertexBuffers4(cmd, primary, primaryOff, oldFrame, oldFrameOff,
                              VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 0);
}

void VK_DestroyDynamicVBO(void) {
    VK_DestroyMeshDummyFrame();
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

    if (!VK_InitMeshDummyFrame()) {
        VK_DestroyDynamicVBO();
        return qfalse;
    }
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

/* Decode one MD3 frame into drawVert xyz+normal (GPU lerps + lights). */
static void VK_LoadMD3Frame(md3Surface_t *surf, int frame, drawVert_t *outVerts) {
    short *xyz;
    short *normals;
    int vertNum;
    int numVerts;

    numVerts = surf->numVerts;
    xyz = (short *)((byte *)surf + surf->ofsXyzNormals)
        + (frame * surf->numVerts * 4);
    normals = xyz + 3;

    for (vertNum = 0; vertNum < numVerts; vertNum++, xyz += 4, normals += 4) {
        outVerts[vertNum].xyz[0] = xyz[0] * MD3_XYZ_SCALE;
        outVerts[vertNum].xyz[1] = xyz[1] * MD3_XYZ_SCALE;
        outVerts[vertNum].xyz[2] = xyz[2] * MD3_XYZ_SCALE;
        VK_DecodeLatLongNormal(outVerts[vertNum].normal, normals[0]);
    }
}

static void VK_LoadMD3FrameOld(md3Surface_t *surf, int frame, vk_meshAttrib1_t *out) {
    short *xyz;
    short *normals;
    int vertNum;
    int numVerts;
    vec3_t nrm;

    numVerts = surf->numVerts;
    xyz = (short *)((byte *)surf + surf->ofsXyzNormals)
        + (frame * surf->numVerts * 4);
    normals = xyz + 3;

    for (vertNum = 0; vertNum < numVerts; vertNum++, xyz += 4, normals += 4) {
        memset(&out[vertNum], 0, sizeof(out[vertNum]));
        out[vertNum].a[0] = xyz[0] * MD3_XYZ_SCALE;
        out[vertNum].a[1] = xyz[1] * MD3_XYZ_SCALE;
        out[vertNum].a[2] = xyz[2] * MD3_XYZ_SCALE;
        VK_DecodeLatLongNormal(nrm, normals[0]);
        out[vertNum].b[0] = nrm[0];
        out[vertNum].b[1] = nrm[1];
        out[vertNum].b[2] = nrm[2];
    }
}

/* MDC: base frame (+ optional compressed offset). GPU lerps the results. */
static void VK_LoadMDCFrame(mdcSurface_t *surf, int animFrame, drawVert_t *outVerts) {
    short *xyz, *normals;
    int vertNum;
    int numVerts;
    int base;
    short *comp = NULL;
    mdcXyzCompressed_t *xyzComp = NULL;
    qboolean hasComp;
    vec3_t ofsVec;

    numVerts = surf->numVerts;
    base = (int)*((short *)((byte *)surf + surf->ofsFrameBaseFrames) + animFrame);
    xyz = (short *)((byte *)surf + surf->ofsXyzNormals) + (base * surf->numVerts * 4);
    normals = xyz + 3;

    hasComp = (surf->numCompFrames > 0);
    if (hasComp) {
        comp = ((short *)((byte *)surf + surf->ofsFrameCompFrames) + animFrame);
        if (*comp >= 0) {
            xyzComp = (mdcXyzCompressed_t *)((byte *)surf + surf->ofsXyzCompressed)
                + (*comp * surf->numVerts);
        }
    }

    for (vertNum = 0; vertNum < numVerts; vertNum++, xyz += 4, normals += 4) {
        outVerts[vertNum].xyz[0] = xyz[0] * MD3_XYZ_SCALE;
        outVerts[vertNum].xyz[1] = xyz[1] * MD3_XYZ_SCALE;
        outVerts[vertNum].xyz[2] = xyz[2] * MD3_XYZ_SCALE;

        if (hasComp && comp && *comp >= 0) {
            R_MDC_DecodeXyzCompressed(xyzComp->ofsVec, ofsVec, outVerts[vertNum].normal);
            xyzComp++;
            VectorAdd(outVerts[vertNum].xyz, ofsVec, outVerts[vertNum].xyz);
        } else {
            VK_DecodeLatLongNormal(outVerts[vertNum].normal, normals[0]);
        }
    }
}

static void VK_LoadMDCFrameOld(mdcSurface_t *surf, int animFrame, vk_meshAttrib1_t *out) {
    short *xyz, *normals;
    int vertNum;
    int numVerts;
    int base;
    short *comp = NULL;
    mdcXyzCompressed_t *xyzComp = NULL;
    qboolean hasComp;
    vec3_t ofsVec;
    vec3_t nrm;

    numVerts = surf->numVerts;
    base = (int)*((short *)((byte *)surf + surf->ofsFrameBaseFrames) + animFrame);
    xyz = (short *)((byte *)surf + surf->ofsXyzNormals) + (base * surf->numVerts * 4);
    normals = xyz + 3;

    hasComp = (surf->numCompFrames > 0);
    if (hasComp) {
        comp = ((short *)((byte *)surf + surf->ofsFrameCompFrames) + animFrame);
        if (*comp >= 0) {
            xyzComp = (mdcXyzCompressed_t *)((byte *)surf + surf->ofsXyzCompressed)
                + (*comp * surf->numVerts);
        }
    }

    for (vertNum = 0; vertNum < numVerts; vertNum++, xyz += 4, normals += 4) {
        memset(&out[vertNum], 0, sizeof(out[vertNum]));
        out[vertNum].a[0] = xyz[0] * MD3_XYZ_SCALE;
        out[vertNum].a[1] = xyz[1] * MD3_XYZ_SCALE;
        out[vertNum].a[2] = xyz[2] * MD3_XYZ_SCALE;

        if (hasComp && comp && *comp >= 0) {
            R_MDC_DecodeXyzCompressed(xyzComp->ofsVec, ofsVec, nrm);
            xyzComp++;
            out[vertNum].a[0] += ofsVec[0];
            out[vertNum].a[1] += ofsVec[1];
            out[vertNum].a[2] += ofsVec[2];
        } else {
            VK_DecodeLatLongNormal(nrm, normals[0]);
        }
        out[vertNum].b[0] = nrm[0];
        out[vertNum].b[1] = nrm[1];
        out[vertNum].b[2] = nrm[2];
    }
}

extern void RB_SurfaceFace(srfSurfaceFace_t *surf);
extern void RB_SurfaceGrid(srfGridMesh_t *cv);
extern void RB_SurfaceTriangles(srfTriangles_t *srf);
extern void RB_DeformTessGeometry(void);

void VK_SurfaceFace(void *surfData) {
    srfSurfaceFace_t *face = (srfSurfaceFace_t *)surfData;
    VkCommandBuffer cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    vk_world_surf_t *ws = VK_WorldFindSurf(surfData);
    int numVerts = face->numPoints;
    int numIdx = face->numIndices;
    int vboOff, iboOff;
    int baseVert, baseIdx;

    if (ws && !VK_ShaderNeedsCpuDeform(tess.shader)) {
        VkDeviceSize offsets[1] = { 0 };
        VK_BindMeshVertexBuffers(cmd, vk_world.vbo, offsets[0], VK_NULL_HANDLE, 0);
        vkCmdBindIndexBuffer(cmd, vk_world.ibo, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, ws->indexCount, 1, ws->firstIndex, ws->vertexOffset, 0);
        vk_worldDrawCount++;
        return;
    }

    if (VK_ShaderNeedsCpuDeform(tess.shader)) {
        baseVert = tess.numVertexes;
        baseIdx = tess.numIndexes;
        RB_SurfaceFace(face);
        RB_DeformTessGeometry();
        if (tess.numVertexes > baseVert && tess.numIndexes > baseIdx) {
            VK_DrawTessRange(baseVert, baseIdx);
        }
        tess.numVertexes = baseVert;
        tess.numIndexes = baseIdx;
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
    VK_BindMeshVertexBuffers(cmd, vk_dyn.buffer, offsets[0], VK_NULL_HANDLE, 0);
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
    int baseVert, baseIdx;

    if (ws && !VK_ShaderNeedsCpuDeform(tess.shader)) {
        VkDeviceSize offsets[1] = { 0 };
        VK_BindMeshVertexBuffers(cmd, vk_world.vbo, offsets[0], VK_NULL_HANDLE, 0);
        vkCmdBindIndexBuffer(cmd, vk_world.ibo, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, ws->indexCount, 1, ws->firstIndex, ws->vertexOffset, 0);
        vk_worldDrawCount++;
        return;
    }

    if (VK_ShaderNeedsCpuDeform(tess.shader)) {
        baseVert = tess.numVertexes;
        baseIdx = tess.numIndexes;
        RB_SurfaceGrid(grid);
        RB_DeformTessGeometry();
        if (tess.numVertexes > baseVert && tess.numIndexes > baseIdx) {
            VK_DrawTessRange(baseVert, baseIdx);
        }
        tess.numVertexes = baseVert;
        tess.numIndexes = baseIdx;
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
    VK_BindMeshVertexBuffers(cmd, vk_dyn.buffer, offsets[0], VK_NULL_HANDLE, 0);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, numIdx, 1, 0, 0, 0);
    vk_worldDrawCount++;
}

void VK_SurfaceTriangles(void *surfData) {
    srfTriangles_t *tri = (srfTriangles_t *)surfData;
    VkCommandBuffer cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    vk_world_surf_t *ws = VK_WorldFindSurf(surfData);
    int vboOff, iboOff;
    int baseVert, baseIdx;

    if (ws && !VK_ShaderNeedsCpuDeform(tess.shader)) {
        VkDeviceSize offsets[1] = { 0 };
        VK_BindMeshVertexBuffers(cmd, vk_world.vbo, offsets[0], VK_NULL_HANDLE, 0);
        vkCmdBindIndexBuffer(cmd, vk_world.ibo, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, ws->indexCount, 1, ws->firstIndex, ws->vertexOffset, 0);
        vk_worldDrawCount++;
        return;
    }

    if (VK_ShaderNeedsCpuDeform(tess.shader)) {
        baseVert = tess.numVertexes;
        baseIdx = tess.numIndexes;
        RB_SurfaceTriangles(tri);
        RB_DeformTessGeometry();
        if (tess.numVertexes > baseVert && tess.numIndexes > baseIdx) {
            VK_DrawTessRange(baseVert, baseIdx);
        }
        tess.numVertexes = baseVert;
        tess.numIndexes = baseIdx;
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
    VK_BindMeshVertexBuffers(cmd, vk_dyn.buffer, offsets[0], VK_NULL_HANDLE, 0);
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
    VK_BindMeshVertexBuffers(cmd, vk_dyn.buffer, offsets[0], VK_NULL_HANDLE, 0);
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
            /* GPU lighting fills color for CGEN_LIGHTING_DIFFUSE; white base. */
            verts[i].color[0] = verts[i].color[1] = verts[i].color[2] = verts[i].color[3] = 255;
        }
    }

    for (i = 0; i < numIdx; i++) {
        idxDst[i] = tess.indexes[baseIdx + i] - baseVert;
    }

    offsets[0] = (VkDeviceSize)vboOff;
    VK_BindMeshVertexBuffers(cmd, vk_dyn.buffer, offsets[0], VK_NULL_HANDLE, 0);
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

/* ---------------------------------------------------------------------------
 * MD3/MDC device-local mesh cache
 *
 * MD3: raw xyz+latlong shorts for all frames; ST+IBO static; decode/lerp in VS.
 * MDC: expand compressed frames once (ofsVec decode at build); float lerp in VS.
 * ------------------------------------------------------------------------- */

#define VK_MESH_CACHE_HASH 512

typedef struct vk_mesh_cache_s {
    void *key;
    int numVerts;
    int numFrames;
    int numIdx;
    qboolean isShort; /* MD3 shorts path */
    VkBuffer stVbo;
    VkDeviceMemory stMem;
    VkBuffer frameVbo;
    VkDeviceMemory frameMem;
    VkBuffer oldVbo;
    VkDeviceMemory oldMem;
    VkBuffer ibo;
    VkDeviceMemory iboMem;
    struct vk_mesh_cache_s *next;
} vk_mesh_cache_t;

static vk_mesh_cache_t *vk_meshCacheHash[VK_MESH_CACHE_HASH];

static uint32_t VK_MeshCacheHashKey(const void *p) {
    uintptr_t v = (uintptr_t)p;
    return (uint32_t)((v >> 4) ^ (v >> 20)) & (VK_MESH_CACHE_HASH - 1);
}

void VK_DestroyMeshCaches(void) {
    int i;

    if (vk_state.dev) {
        vkDeviceWaitIdle(vk_state.dev);
    }
    for (i = 0; i < VK_MESH_CACHE_HASH; i++) {
        vk_mesh_cache_t *c = vk_meshCacheHash[i];
        while (c) {
            vk_mesh_cache_t *next = c->next;
            if (c->stVbo) {
                vkDestroyBuffer(vk_state.dev, c->stVbo, NULL);
            }
            if (c->stMem) {
                vkFreeMemory(vk_state.dev, c->stMem, NULL);
            }
            if (c->frameVbo) {
                vkDestroyBuffer(vk_state.dev, c->frameVbo, NULL);
            }
            if (c->frameMem) {
                vkFreeMemory(vk_state.dev, c->frameMem, NULL);
            }
            if (c->oldVbo) {
                vkDestroyBuffer(vk_state.dev, c->oldVbo, NULL);
            }
            if (c->oldMem) {
                vkFreeMemory(vk_state.dev, c->oldMem, NULL);
            }
            if (c->ibo) {
                vkDestroyBuffer(vk_state.dev, c->ibo, NULL);
            }
            if (c->iboMem) {
                vkFreeMemory(vk_state.dev, c->iboMem, NULL);
            }
            free(c);
            c = next;
        }
        vk_meshCacheHash[i] = NULL;
    }
}

static vk_mesh_cache_t *VK_MeshCacheFind(void *key) {
    uint32_t h = VK_MeshCacheHashKey(key);
    vk_mesh_cache_t *c;

    for (c = vk_meshCacheHash[h]; c; c = c->next) {
        if (c->key == key) {
            return c;
        }
    }
    return NULL;
}

static int VK_MdcNumFrames(mdcSurface_t *surf) {
    return (surf->ofsFrameCompFrames - surf->ofsFrameBaseFrames) / (int)sizeof(short);
}

static vk_mesh_cache_t *VK_BuildMd3ShortCache(md3Surface_t *surf) {
    vk_mesh_cache_t *c;
    drawVert_t *stVerts;
    md3XyzNormal_t *frames;
    int *indices;
    float *texCoords;
    int *triangles;
    int j, f;
    uint32_t h;
    VkDeviceSize stSize, frameSize, iboSize;

    c = (vk_mesh_cache_t *)calloc(1, sizeof(*c));
    if (!c) {
        return NULL;
    }
    c->key = surf;
    c->numVerts = surf->numVerts;
    c->numFrames = surf->numFrames;
    c->numIdx = surf->numTriangles * 3;
    c->isShort = qtrue;

    if (c->numVerts <= 0 || c->numFrames <= 0 || c->numIdx <= 0) {
        free(c);
        return NULL;
    }

    stSize = (VkDeviceSize)c->numVerts * sizeof(drawVert_t);
    frameSize = (VkDeviceSize)c->numVerts * (VkDeviceSize)c->numFrames * sizeof(md3XyzNormal_t);
    iboSize = (VkDeviceSize)c->numIdx * sizeof(int);

    stVerts = (drawVert_t *)malloc((size_t)stSize);
    frames = (md3XyzNormal_t *)malloc((size_t)frameSize);
    indices = (int *)malloc((size_t)iboSize);
    if (!stVerts || !frames || !indices) {
        free(stVerts);
        free(frames);
        free(indices);
        free(c);
        return NULL;
    }

    texCoords = (float *)((byte *)surf + surf->ofsSt);
    triangles = (int *)((byte *)surf + surf->ofsTriangles);
    memset(stVerts, 0, (size_t)stSize);
    for (j = 0; j < c->numVerts; j++) {
        stVerts[j].st[0] = texCoords[j * 2 + 0];
        stVerts[j].st[1] = texCoords[j * 2 + 1];
        stVerts[j].color[0] = stVerts[j].color[1] = stVerts[j].color[2] = stVerts[j].color[3] = 255;
        stVerts[j].normal[2] = 1.0f;
    }
    memcpy(frames, (byte *)surf + surf->ofsXyzNormals, (size_t)frameSize);
    memcpy(indices, triangles, (size_t)iboSize);

    if (!VK_CreateDeviceLocalBuffer(stVerts, stSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    &c->stVbo, &c->stMem) ||
        !VK_CreateDeviceLocalBuffer(frames, frameSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    &c->frameVbo, &c->frameMem) ||
        !VK_CreateDeviceLocalBuffer(indices, iboSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                    &c->ibo, &c->iboMem)) {
        free(stVerts);
        free(frames);
        free(indices);
        if (c->stVbo) {
            vkDestroyBuffer(vk_state.dev, c->stVbo, NULL);
        }
        if (c->stMem) {
            vkFreeMemory(vk_state.dev, c->stMem, NULL);
        }
        if (c->frameVbo) {
            vkDestroyBuffer(vk_state.dev, c->frameVbo, NULL);
        }
        if (c->frameMem) {
            vkFreeMemory(vk_state.dev, c->frameMem, NULL);
        }
        free(c);
        return NULL;
    }

    free(stVerts);
    free(frames);
    free(indices);

    h = VK_MeshCacheHashKey(surf);
    c->next = vk_meshCacheHash[h];
    vk_meshCacheHash[h] = c;
    return c;
}

static vk_mesh_cache_t *VK_BuildMdcFloatCache(mdcSurface_t *surf) {
    vk_mesh_cache_t *c;
    drawVert_t *frames;
    vk_meshAttrib1_t *oldFrames;
    int *indices;
    float *texCoords;
    int *triangles;
    int j, f;
    int numFrames;
    uint32_t h;
    VkDeviceSize frameSize, oldSize, iboSize;

    numFrames = VK_MdcNumFrames(surf);
    c = (vk_mesh_cache_t *)calloc(1, sizeof(*c));
    if (!c) {
        return NULL;
    }
    c->key = surf;
    c->numVerts = surf->numVerts;
    c->numFrames = numFrames;
    c->numIdx = surf->numTriangles * 3;
    c->isShort = qfalse;

    if (c->numVerts <= 0 || c->numFrames <= 0 || c->numIdx <= 0) {
        free(c);
        return NULL;
    }

    frameSize = (VkDeviceSize)c->numVerts * (VkDeviceSize)c->numFrames * sizeof(drawVert_t);
    oldSize = (VkDeviceSize)c->numVerts * (VkDeviceSize)c->numFrames * sizeof(vk_meshAttrib1_t);
    iboSize = (VkDeviceSize)c->numIdx * sizeof(int);

    frames = (drawVert_t *)malloc((size_t)frameSize);
    oldFrames = (vk_meshAttrib1_t *)malloc((size_t)oldSize);
    indices = (int *)malloc((size_t)iboSize);
    if (!frames || !oldFrames || !indices) {
        free(frames);
        free(oldFrames);
        free(indices);
        free(c);
        return NULL;
    }

    texCoords = (float *)((byte *)surf + surf->ofsSt);
    triangles = (int *)((byte *)surf + surf->ofsTriangles);
    memset(frames, 0, (size_t)frameSize);
    memset(oldFrames, 0, (size_t)oldSize);

    for (f = 0; f < numFrames; f++) {
        drawVert_t *dst = frames + f * c->numVerts;
        vk_meshAttrib1_t *odst = oldFrames + f * c->numVerts;

        VK_LoadMDCFrame(surf, f, dst);
        VK_LoadMDCFrameOld(surf, f, odst);
        for (j = 0; j < c->numVerts; j++) {
            dst[j].st[0] = texCoords[j * 2 + 0];
            dst[j].st[1] = texCoords[j * 2 + 1];
            dst[j].color[0] = dst[j].color[1] = dst[j].color[2] = dst[j].color[3] = 255;
        }
    }
    memcpy(indices, triangles, (size_t)iboSize);

    if (!VK_CreateDeviceLocalBuffer(frames, frameSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    &c->frameVbo, &c->frameMem) ||
        !VK_CreateDeviceLocalBuffer(oldFrames, oldSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    &c->oldVbo, &c->oldMem) ||
        !VK_CreateDeviceLocalBuffer(indices, iboSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                    &c->ibo, &c->iboMem)) {
        free(frames);
        free(oldFrames);
        free(indices);
        if (c->frameVbo) {
            vkDestroyBuffer(vk_state.dev, c->frameVbo, NULL);
        }
        if (c->frameMem) {
            vkFreeMemory(vk_state.dev, c->frameMem, NULL);
        }
        if (c->oldVbo) {
            vkDestroyBuffer(vk_state.dev, c->oldVbo, NULL);
        }
        if (c->oldMem) {
            vkFreeMemory(vk_state.dev, c->oldMem, NULL);
        }
        free(c);
        return NULL;
    }

    free(frames);
    free(oldFrames);
    free(indices);

    h = VK_MeshCacheHashKey(surf);
    c->next = vk_meshCacheHash[h];
    vk_meshCacheHash[h] = c;
    return c;
}

static vk_mesh_cache_t *VK_GetMeshCache(void *surfData, surfaceType_t type) {
    vk_mesh_cache_t *c = VK_MeshCacheFind(surfData);

    if (c) {
        return c;
    }
    if (type == SF_MDC) {
        return VK_BuildMdcFloatCache((mdcSurface_t *)surfData);
    }
    return VK_BuildMd3ShortCache((md3Surface_t *)surfData);
}

void VK_SurfaceMD3(void *surfData) {
    md3Surface_t *md3Surface = (md3Surface_t *)surfData;
    mdcSurface_t *mdcSurface = (mdcSurface_t *)surfData;
    surfaceType_t type = *(surfaceType_t *)surfData;
    float backlerp;
    int frame, oldframe;
    vk_mesh_cache_t *cache;
    VkCommandBuffer cmd;
    vk_push_constants_t pc;
    VkDeviceSize newOff, oldOff;

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

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    if (!cmd) {
        return;
    }

    cache = VK_GetMeshCache(surfData, type);
    if (!cache || cache->numIdx <= 0) {
        return;
    }

    frame = backEnd.currentEntity->e.frame;
    oldframe = backEnd.currentEntity->e.oldframe;
    if (frame < 0) {
        frame = 0;
    }
    if (oldframe < 0) {
        oldframe = 0;
    }
    if (frame >= cache->numFrames) {
        frame = cache->numFrames - 1;
    }
    if (oldframe >= cache->numFrames) {
        oldframe = cache->numFrames - 1;
    }

    if (vk_volumetricFogPass) {
        /* Preserve volumetric fog mode/vectors from VK_FogPass; only attach
         * mesh decode state. A full FillPushConstants would wipe mode 4 and
         * draw the gun/models as an opaque wash under the fog blend. */
        pc = vk_fogPassPush;
        pc.mvpIndex = vk_currentMvpSlot;
        pc.pad[1] = cache->isShort ? 1u : 0u;
        pc.params[VK_MESH_LIGHTDIR_PARAM][3] = backlerp;
        pc.params[VK_MESH_LIGHT_PARAM][3] = 0.0f;
        pc.params[VK_MESH_DIRECT_PARAM][3] = 0.0f;
    } else {
        VK_FillPushConstants(vk_currentMvpSlot, tess.shader, &pc);
        pc.pad[1] = cache->isShort ? 1u : 0u;
        /* Ensure backlerp is current even if FillPushConstants raced. */
        pc.params[VK_MESH_LIGHTDIR_PARAM][3] = backlerp;
    }
    VK_CmdPushMaterial(cmd, &pc);

    if (cache->isShort) {
        newOff = (VkDeviceSize)frame * (VkDeviceSize)cache->numVerts * sizeof(md3XyzNormal_t);
        oldOff = (VkDeviceSize)oldframe * (VkDeviceSize)cache->numVerts * sizeof(md3XyzNormal_t);
        VK_BindMeshVertexBuffers4(cmd,
                                  cache->stVbo, 0,
                                  VK_NULL_HANDLE, 0,
                                  cache->frameVbo, newOff,
                                  (backlerp > 0.0f) ? cache->frameVbo : VK_NULL_HANDLE,
                                  (backlerp > 0.0f) ? oldOff : 0);
    } else {
        newOff = (VkDeviceSize)frame * (VkDeviceSize)cache->numVerts * sizeof(drawVert_t);
        oldOff = (VkDeviceSize)oldframe * (VkDeviceSize)cache->numVerts * sizeof(vk_meshAttrib1_t);
        VK_BindMeshVertexBuffers4(cmd,
                                  cache->frameVbo, newOff,
                                  (backlerp > 0.0f) ? cache->oldVbo : VK_NULL_HANDLE,
                                  (backlerp > 0.0f) ? oldOff : 0,
                                  VK_NULL_HANDLE, 0,
                                  VK_NULL_HANDLE, 0);
    }

    vkCmdBindIndexBuffer(cmd, cache->ibo, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, (uint32_t)cache->numIdx, 1, 0, 0, 0);
    vk_worldDrawCount++;
}

void VK_SurfaceFlare(void *surfData) {
}

void VK_SurfaceAnim(void *surfData) {
    mdsSurface_t *surface;
    mdsHeader_t *header;
    refEntity_t *refent;
    int *boneList;
    int *triangles;
    int *collapse_map;
    int collapse[MDS_MAX_VERTS];
    int render_count;
    int indexes;
    int numIdx;
    int vboOff;
    int iboOff;
    int weightOff;
    int j, k;
    float lodScale;
    float lodRadius;
    vec3_t lodOrigin;
    mdsFrame_t *frame;
    int frameSize;
    drawVert_t *verts;
    vk_meshAttrib1_t *weights;
    int *idxDst;
    mdsVertex_t *v;
    const mdsBoneFrame_t *bones;
    uint32_t boneSet;
    VkCommandBuffer cmd;
    vk_push_constants_t pc;
    mdsHeader_t *hdr;

    if (!backEnd.currentEntity) {
        return;
    }

    surface = (mdsSurface_t *)surfData;
    if (backEnd.currentEntity->e.reFlags & REFLAG_ONLYHAND) {
        if (!strstr(surface->name, "hand")) {
            return;
        }
    }

    cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    if (!cmd) {
        return;
    }

    refent = &backEnd.currentEntity->e;
    boneList = (int *)((byte *)surface + surface->ofsBoneReferences);
    header = (mdsHeader_t *)((byte *)surface + surface->ofsHeader);
    hdr = header;

    R_CalcBones(header, (const refEntity_t *)refent, boneList, surface->numBoneReferences);
    bones = RB_GetAnimBones();
    boneSet = VK_BoneAllocSet(bones, header->numBones);

    frameSize = (int)(sizeof(mdsFrame_t) + (header->numBones - 1) * sizeof(mdsBoneFrameCompressed_t));
    frame = (mdsFrame_t *)((byte *)header + header->ofsFrames + refent->frame * frameSize);
    VectorAdd(refent->origin, frame->localOrigin, lodOrigin);
    lodRadius = frame->radius;
    lodScale = R_CalcMDSLod(refent, lodOrigin, lodRadius, header->lodBias, header->lodScale);

    if (refent->reFlags & REFLAG_DEAD_LOD) {
        if (lodScale < 0.35f) {
            lodScale = 0.35f;
        }
        render_count = (int)((float)surface->numVerts * lodScale);
    } else {
        render_count = (int)((float)surface->numVerts * lodScale);
        if (render_count < surface->minLod) {
            render_count = surface->minLod;
        }
    }
    if (render_count > surface->numVerts) {
        render_count = surface->numVerts;
    }
    if (render_count <= 0) {
        return;
    }

    collapse_map = (int *)((byte *)surface + surface->ofsCollapseMap);
    triangles = (int *)((byte *)surface + surface->ofsTriangles);
    indexes = surface->numTriangles * 3;

    /* Build collapsed index list into a temp, then upload. */
    {
        static int tmpIdx[MDS_MAX_TRIANGLES * 3];
        int *pIndexes = tmpIdx;
        numIdx = 0;

        if (render_count == surface->numVerts) {
            memcpy(tmpIdx, triangles, sizeof(int) * indexes);
            numIdx = indexes;
        } else {
            int *pCollapse;
            int *pCollapseMap;
            int *collapseEnd;
            int p0, p1, p2;
            int *tri = triangles;

            pCollapse = collapse;
            for (j = 0; j < render_count; pCollapse++, j++) {
                *pCollapse = j;
            }
            pCollapseMap = &collapse_map[render_count];
            for (collapseEnd = collapse + surface->numVerts; pCollapse < collapseEnd;
                 pCollapse++, pCollapseMap++) {
                *pCollapse = collapse[*pCollapseMap];
            }

            for (j = 0; j < indexes; j += 3) {
                p0 = collapse[*(tri++)];
                p1 = collapse[*(tri++)];
                p2 = collapse[*(tri++)];
                if (p0 == p1 || p1 == p2 || p2 == p0) {
                    continue;
                }
                *pIndexes++ = p0;
                *pIndexes++ = p1;
                *pIndexes++ = p2;
                numIdx += 3;
            }
        }

        if (numIdx <= 0) {
            return;
        }

        vboOff = VK_ReserveDynamicVBO(render_count * (int)sizeof(drawVert_t));
        weightOff = VK_ReserveDynamicVBO(render_count * (int)sizeof(vk_meshAttrib1_t));
        iboOff = VK_ReserveDynamicVBO(numIdx * (int)sizeof(int));
        if (vboOff < 0 || weightOff < 0 || iboOff < 0) {
            return;
        }

        verts = (drawVert_t *)((uint8_t *)vk_dyn.mapped + vboOff);
        weights = (vk_meshAttrib1_t *)((uint8_t *)vk_dyn.mapped + weightOff);
        idxDst = (int *)((uint8_t *)vk_dyn.mapped + iboOff);
        memcpy(idxDst, tmpIdx, numIdx * sizeof(int));
    }

    v = (mdsVertex_t *)((byte *)surface + surface->ofsVerts);
    for (j = 0; j < render_count; j++) {
        mdsWeight_t *w;
        int nw;
        int boneIdx[4];
        float *slots[4];

        memset(&verts[j], 0, sizeof(verts[j]));
        memset(&weights[j], 0, sizeof(weights[j]));
        VectorCopy(v->normal, verts[j].normal);
        verts[j].st[0] = v->texCoords[0];
        verts[j].st[1] = v->texCoords[1];
        verts[j].color[2] = 255;
        verts[j].color[3] = 255;

        slots[0] = weights[j].a;
        slots[1] = weights[j].b;
        slots[2] = weights[j].c;
        slots[3] = weights[j].d;
        boneIdx[0] = boneIdx[1] = boneIdx[2] = boneIdx[3] = 0;

        nw = v->numWeights;
        if (nw > 4) {
            nw = 4;
        }
        w = v->weights;
        for (k = 0; k < nw; k++, w++) {
            slots[k][0] = w->offset[0];
            slots[k][1] = w->offset[1];
            slots[k][2] = w->offset[2];
            slots[k][3] = w->boneWeight;
            boneIdx[k] = w->boneIndex;
            if (boneIdx[k] < 0) {
                boneIdx[k] = 0;
            }
            if (boneIdx[k] > 255) {
                boneIdx[k] = 255;
            }
        }

        verts[j].lightmap[0] = (float)boneIdx[0];
        verts[j].lightmap[1] = (float)boneIdx[1];
        verts[j].color[0] = (byte)boneIdx[2];
        verts[j].color[1] = (byte)boneIdx[3];

        v = (mdsVertex_t *)&v->weights[v->numWeights];
    }

    /* Re-push with skinning enabled; stage state already set by the draw loop. */
    if (vk_volumetricFogPass) {
        pc = vk_fogPassPush;
        pc.mvpIndex = vk_currentMvpSlot;
        pc.params[VK_MESH_DIRECT_PARAM][3] = 1.0f;
        pc.pad[0] = boneSet;
    } else {
        VK_FillPushConstants(vk_currentMvpSlot, tess.shader, &pc);
        pc.params[VK_MESH_DIRECT_PARAM][3] = 1.0f;
        pc.pad[0] = boneSet;
    }
    VK_CmdPushMaterial(cmd, &pc);
    VK_ViewBindBones(cmd, boneSet);

    VK_BindMeshVertexBuffers(cmd, vk_dyn.buffer, (VkDeviceSize)vboOff,
                             vk_dyn.buffer, (VkDeviceSize)weightOff);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, (uint32_t)numIdx, 1, 0, 0, 0);
    vk_worldDrawCount++;

    /* Restore bone binding 0 so later world draws see a valid (zero) set. */
    VK_ViewBindBones(cmd, 0);
    (void)hdr;
}

static void VK_DrawBillboardQuad(const vec3_t origin, const vec3_t left, const vec3_t up,
                                 const byte color[4]) {
    VkCommandBuffer cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    int vboOff, iboOff;
    drawVert_t *verts;
    int *idx;
    VkDeviceSize offsets[1];
    int i;
    vec3_t normal;

    if (!cmd) {
        return;
    }

    vboOff = VK_ReserveDynamicVBO(4 * (int)sizeof(drawVert_t));
    iboOff = VK_ReserveDynamicVBO(6 * (int)sizeof(int));
    if (vboOff < 0 || iboOff < 0) {
        return;
    }

    verts = (drawVert_t *)((uint8_t *)vk_dyn.mapped + vboOff);
    idx = (int *)((uint8_t *)vk_dyn.mapped + iboOff);

    /* Match RB_AddQuadStampExt corner / ST / index winding. */
    VectorAdd(origin, left, verts[0].xyz);
    VectorAdd(verts[0].xyz, up, verts[0].xyz);
    VectorSubtract(origin, left, verts[1].xyz);
    VectorAdd(verts[1].xyz, up, verts[1].xyz);
    VectorSubtract(origin, left, verts[2].xyz);
    VectorSubtract(verts[2].xyz, up, verts[2].xyz);
    VectorAdd(origin, left, verts[3].xyz);
    VectorSubtract(verts[3].xyz, up, verts[3].xyz);

    VectorSubtract(vec3_origin, backEnd.viewParms.or.axis[0], normal);

    verts[0].st[0] = 0.0f; verts[0].st[1] = 0.0f;
    verts[1].st[0] = 1.0f; verts[1].st[1] = 0.0f;
    verts[2].st[0] = 1.0f; verts[2].st[1] = 1.0f;
    verts[3].st[0] = 0.0f; verts[3].st[1] = 1.0f;

    for (i = 0; i < 4; i++) {
        VectorCopy(normal, verts[i].normal);
        verts[i].lightmap[0] = 0.0f;
        verts[i].lightmap[1] = 0.0f;
        verts[i].color[0] = color[0];
        verts[i].color[1] = color[1];
        verts[i].color[2] = color[2];
        verts[i].color[3] = color[3];
    }

    idx[0] = 0; idx[1] = 1; idx[2] = 3;
    idx[3] = 3; idx[4] = 1; idx[5] = 2;

    offsets[0] = (VkDeviceSize)vboOff;
    VK_BindMeshVertexBuffers(cmd, vk_dyn.buffer, offsets[0], VK_NULL_HANDLE, 0);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
    vk_worldDrawCount++;
}

static void VK_SurfaceSpriteDirect(qboolean splash) {
    refEntity_t *e;
    vec3_t left, up;
    float radius;

    e = &backEnd.currentEntity->e;
    radius = e->radius;

    if (splash) {
        VectorSet(left, -radius, 0.0f, 0.0f);
        VectorSet(up, 0.0f, radius, 0.0f);
    } else if (e->rotation == 0) {
        VectorScale(backEnd.viewParms.or.axis[1], radius, left);
        VectorScale(backEnd.viewParms.or.axis[2], radius, up);
    } else {
        float s, c;
        float ang = (float)M_PI * e->rotation / 180.0f;

        s = sinf(ang);
        c = cosf(ang);
        VectorScale(backEnd.viewParms.or.axis[1], c * radius, left);
        VectorMA(left, -s * radius, backEnd.viewParms.or.axis[2], left);
        VectorScale(backEnd.viewParms.or.axis[2], c * radius, up);
        VectorMA(up, s * radius, backEnd.viewParms.or.axis[1], up);
    }

    if (backEnd.viewParms.isMirror) {
        VectorSubtract(vec3_origin, left, left);
    }

    VK_DrawBillboardQuad(e->origin, left, up, e->shaderRGBA);
}

static void VK_EmitRailCoreQuad(const vec3_t start, const vec3_t end, const vec3_t up,
                                float len, float spanWidth) {
    VkCommandBuffer cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    int vboOff, iboOff;
    drawVert_t *verts;
    int *idx;
    VkDeviceSize offsets[1];
    float spanWidth2 = -spanWidth;
    float t = len / 256.0f;
    const byte *rgba;
    byte dim[4];
    int i;

    if (!cmd || !backEnd.currentEntity) {
        return;
    }

    rgba = backEnd.currentEntity->e.shaderRGBA;
    dim[0] = (byte)(rgba[0] * 0.25f);
    dim[1] = (byte)(rgba[1] * 0.25f);
    dim[2] = (byte)(rgba[2] * 0.25f);
    dim[3] = 255;

    vboOff = VK_ReserveDynamicVBO(4 * (int)sizeof(drawVert_t));
    iboOff = VK_ReserveDynamicVBO(6 * (int)sizeof(int));
    if (vboOff < 0 || iboOff < 0) {
        return;
    }

    verts = (drawVert_t *)((uint8_t *)vk_dyn.mapped + vboOff);
    idx = (int *)((uint8_t *)vk_dyn.mapped + iboOff);

    VectorMA(start, spanWidth, up, verts[0].xyz);
    verts[0].st[0] = 0.0f; verts[0].st[1] = 0.0f;
    VectorMA(start, spanWidth2, up, verts[1].xyz);
    verts[1].st[0] = 0.0f; verts[1].st[1] = 1.0f;
    VectorMA(end, spanWidth, up, verts[2].xyz);
    verts[2].st[0] = t; verts[2].st[1] = 0.0f;
    VectorMA(end, spanWidth2, up, verts[3].xyz);
    verts[3].st[0] = t; verts[3].st[1] = 1.0f;

    for (i = 0; i < 4; i++) {
        VectorClear(verts[i].normal);
        verts[i].lightmap[0] = 0.0f;
        verts[i].lightmap[1] = 0.0f;
        if (i == 0) {
            verts[i].color[0] = dim[0];
            verts[i].color[1] = dim[1];
            verts[i].color[2] = dim[2];
            verts[i].color[3] = dim[3];
        } else {
            verts[i].color[0] = rgba[0];
            verts[i].color[1] = rgba[1];
            verts[i].color[2] = rgba[2];
            verts[i].color[3] = 255;
        }
    }

    idx[0] = 0; idx[1] = 1; idx[2] = 2;
    idx[3] = 2; idx[4] = 1; idx[5] = 3;

    offsets[0] = (VkDeviceSize)vboOff;
    VK_BindMeshVertexBuffers(cmd, vk_dyn.buffer, offsets[0], VK_NULL_HANDLE, 0);
    vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
    vk_worldDrawCount++;
}

static void VK_SurfaceRailCoreDirect(void) {
    refEntity_t *e;
    float len;
    vec3_t right, vec, start, end, v1, v2;

    e = &backEnd.currentEntity->e;
    VectorCopy(e->oldorigin, start);
    VectorCopy(e->origin, end);
    VectorSubtract(end, start, vec);
    len = VectorNormalize(vec);

    VectorSubtract(start, backEnd.viewParms.or.origin, v1);
    VectorNormalize(v1);
    VectorSubtract(end, backEnd.viewParms.or.origin, v2);
    VectorNormalize(v2);
    CrossProduct(v1, v2, right);
    VectorNormalize(right);

    VK_EmitRailCoreQuad(start, end, right, len, (float)r_railCoreWidth->integer);
}

static void VK_SurfaceLightningDirect(void) {
    refEntity_t *e;
    float len;
    vec3_t right, vec, start, end, v1, v2, temp;
    int i;

    e = &backEnd.currentEntity->e;
    VectorCopy(e->oldorigin, end);
    VectorCopy(e->origin, start);
    VectorSubtract(end, start, vec);
    len = VectorNormalize(vec);

    VectorSubtract(start, backEnd.viewParms.or.origin, v1);
    VectorNormalize(v1);
    VectorSubtract(end, backEnd.viewParms.or.origin, v2);
    VectorNormalize(v2);
    CrossProduct(v1, v2, right);
    VectorNormalize(right);

    for (i = 0; i < 4; i++) {
        VK_EmitRailCoreQuad(start, end, right, len, 8.0f);
        RotatePointAroundVector(temp, vec, right, 45.0f);
        VectorCopy(temp, right);
    }
}

static void VK_SurfaceRailRingsDirect(void) {
    refEntity_t *e;
    int numSegs;
    int len;
    int i, j;
    vec3_t vec, right, up, start, end;
    vec3_t pos[4], v;
    float c, s;
    float scale = 0.25f;
    int spanWidth;
    VkCommandBuffer cmd = vk_state.cmdBuffers[vk_state.currentImageIndex];
    const byte *rgba;

    if (!cmd || !backEnd.currentEntity) {
        return;
    }

    e = &backEnd.currentEntity->e;
    rgba = e->shaderRGBA;
    spanWidth = r_railWidth->integer;

    VectorCopy(e->oldorigin, start);
    VectorCopy(e->origin, end);
    VectorSubtract(end, start, vec);
    len = VectorNormalize(vec);
    MakeNormalVectors(vec, right, up);
    numSegs = (int)(len / r_railSegmentLength->value);
    if (numSegs <= 0) {
        numSegs = 1;
    }
    VectorScale(vec, r_railSegmentLength->value, vec);

    if (numSegs > 1) {
        numSegs--;
    }
    if (!numSegs) {
        return;
    }

    for (i = 0; i < 4; i++) {
        c = cosf(DEG2RAD(45 + i * 90));
        s = sinf(DEG2RAD(45 + i * 90));
        v[0] = (right[0] * c + up[0] * s) * scale * spanWidth;
        v[1] = (right[1] * c + up[1] * s) * scale * spanWidth;
        v[2] = (right[2] * c + up[2] * s) * scale * spanWidth;
        VectorAdd(start, v, pos[i]);
        if (numSegs > 1) {
            VectorAdd(pos[i], vec, pos[i]);
        }
    }

    for (i = 0; i < numSegs; i++) {
        int vboOff, iboOff;
        drawVert_t *verts;
        int *idx;
        VkDeviceSize offsets[1];

        vboOff = VK_ReserveDynamicVBO(4 * (int)sizeof(drawVert_t));
        iboOff = VK_ReserveDynamicVBO(6 * (int)sizeof(int));
        if (vboOff < 0 || iboOff < 0) {
            return;
        }

        verts = (drawVert_t *)((uint8_t *)vk_dyn.mapped + vboOff);
        idx = (int *)((uint8_t *)vk_dyn.mapped + iboOff);

        for (j = 0; j < 4; j++) {
            VectorCopy(pos[j], verts[j].xyz);
            verts[j].st[0] = (j < 2) ? 1.0f : 0.0f;
            verts[j].st[1] = (j && j != 3) ? 1.0f : 0.0f;
            VectorClear(verts[j].normal);
            verts[j].lightmap[0] = 0.0f;
            verts[j].lightmap[1] = 0.0f;
            verts[j].color[0] = rgba[0];
            verts[j].color[1] = rgba[1];
            verts[j].color[2] = rgba[2];
            verts[j].color[3] = 255;
            VectorAdd(pos[j], vec, pos[j]);
        }

        idx[0] = 0; idx[1] = 1; idx[2] = 3;
        idx[3] = 3; idx[4] = 1; idx[5] = 2;

        offsets[0] = (VkDeviceSize)vboOff;
        VK_BindMeshVertexBuffers(cmd, vk_dyn.buffer, offsets[0], VK_NULL_HANDLE, 0);
        vkCmdBindIndexBuffer(cmd, vk_dyn.buffer, (VkDeviceSize)iboOff, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
        vk_worldDrawCount++;
    }
}

void VK_SurfaceEntity(void *surfData) {
    refEntity_t *e;
    int baseVert;
    int baseIdx;

    if (!backEnd.currentEntity) {
        return;
    }

    e = &backEnd.currentEntity->e;

    switch (e->reType) {
    case RT_SPLASH:
        VK_SurfaceSpriteDirect(qtrue);
        return;
    case RT_SPRITE:
        VK_SurfaceSpriteDirect(qfalse);
        return;
    case RT_RAIL_CORE:
        VK_SurfaceRailCoreDirect();
        return;
    case RT_RAIL_RINGS:
        VK_SurfaceRailRingsDirect();
        return;
    case RT_LIGHTNING:
        VK_SurfaceLightningDirect();
        return;
    case RT_BEAM:
        baseVert = tess.numVertexes;
        baseIdx = tess.numIndexes;
        VK_SurfaceBeamTess();
        if (tess.numVertexes > baseVert && tess.numIndexes > baseIdx) {
            VK_DrawTessRange(baseVert, baseIdx);
        }
        tess.numVertexes = baseVert;
        tess.numIndexes = baseIdx;
        return;
    default:
        (void)surfData;
        return;
    }
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

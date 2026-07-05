#ifndef BSP_VIEWER_TYPES_H
#define BSP_VIEWER_TYPES_H

#include <stdint.h>

#ifdef RENDERER_VK
#include "../game/q_shared.h"
#include "../qcommon/qfiles.h"
#include <vulkan/vulkan.h>
#else
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>

#define BSP_IDENT (('P' << 24) + ('S' << 16) + ('B' << 8) + 'I')
#define BSP_VERSION 47
#define HEADER_LUMPS 17
#define LUMP_ENTITIES 0
#define LUMP_SHADERS        1
#define LUMP_PLANES         2
#define LUMP_NODES          3
#define LUMP_LEAFS          4
#define LUMP_LEAFSURFACES   5
#define LUMP_LEAFBRUSHES    6
#define LUMP_BRUSHES        8
#define LUMP_BRUSHSIDES     9
#define LUMP_DRAWVERTS 10
#define LUMP_DRAWINDEXES 11
#define LUMP_FOGS           12
#define LUMP_SURFACES 13
#define LUMP_LIGHTMAPS 14
#define LUMP_VISIBILITY     16

#define MAX_QPATH 64

typedef enum {
    MST_BAD,
    MST_PLANAR,
    MST_PATCH,
    MST_TRIANGLE_SOUP,
    MST_FLARE
} mapSurfaceType_t;

typedef struct {
    int32_t fileofs, filelen;
} lump_t;

typedef struct {
    float normal[3];
    float dist;
} dplane_t;

typedef struct {
    int planeNum;
    int children[2];
    int mins[3];
    int maxs[3];
} dnode_t;

typedef struct {
    int cluster;
    int area;
    int mins[3];
    int maxs[3];
    int firstLeafSurface;
    int numLeafSurfaces;
    int firstLeafBrush;
    int numLeafBrushes;
} dleaf_t;
typedef struct {
    int32_t ident, version;
    lump_t lumps[HEADER_LUMPS];
} dheader_t;
typedef struct {
    float xyz[3], st[2], lightmap[2], normal[3];
    uint8_t color[4];
} drawVert_t;
typedef struct {
    int32_t shaderNum, fogNum, surfaceType;
    int32_t firstVert, numVerts, firstIndex, numIndexes;
    int32_t lightmapNum, lightmapX, lightmapY, lightmapWidth, lightmapHeight;
    float lightmapOrigin[3], lightmapVecs[3][3];
    int32_t patchWidth, patchHeight;
} dsurface_t;

typedef struct {
    char shader[MAX_QPATH];
    int32_t surfaceFlags;
    int32_t contentFlags;
} dshader_t;

typedef struct {
    int32_t planeNum;
    int32_t shaderNum;
} dbrushside_t;

typedef struct {
    int32_t firstSide;
    int32_t numSides;
    int32_t shaderNum;
} dbrush_t;

typedef struct {
    char shader[MAX_QPATH];
    int32_t brushNum;
    int32_t visibleSide;
} dfog_t;
#endif /* !RENDERER_VK */

#define BSP_VIEWER_MAX_MAP_AREA_BYTES 32
#define MAX_FRAMES_IN_FLIGHT 2
#define VIEWER_MAX_MATERIAL_STAGES 8
#define VIEWER_SURF_SKY 0x4
#define VIEWER_SURF_NODLIGHT 0x20000
#define BSP_VIEWER_MAX_DLIGHTS 32

enum {
    VIEWER_PIPELINE_OPAQUE = 0,
    VIEWER_PIPELINE_ALPHA,
    VIEWER_PIPELINE_ADDITIVE,
    VIEWER_PIPELINE_FILTER,
    VIEWER_PIPELINE_DSTCOLOR_ONE,
    VIEWER_PIPELINE_ONE_MINUS_DST_ALPHA_ONE,
    VIEWER_PIPELINE_ONE_ONE_MINUS_SRC_ALPHA,
    VIEWER_PIPELINE_DSTCOLOR_ONE_MINUS_DST_ALPHA,
    VIEWER_PIPELINE_SKY,
    VIEWER_PIPELINE_ALPHA_DEPTHWRITE,
    VIEWER_PIPELINE_ADDITIVE_DEPTHWRITE,
    VIEWER_PIPELINE_FILTER_DEPTHWRITE,
    VIEWER_PIPELINE_DSTCOLOR_ONE_DEPTHWRITE,
    VIEWER_PIPELINE_ONE_MINUS_DST_ALPHA_ONE_DEPTHWRITE,
    VIEWER_PIPELINE_ONE_ONE_MINUS_SRC_ALPHA_DEPTHWRITE,
    VIEWER_PIPELINE_DSTCOLOR_ONE_MINUS_DST_ALPHA_DEPTHWRITE,
    VIEWER_PIPELINE_DEPTH_DISABLED_ALPHA,
    VIEWER_PIPELINE_DLIGHT,
    VIEWER_PIPELINE_COUNT
};

typedef struct {
    float bounds[2][3];
    float color[3];
    float tcScale;
    int hasSurface;
    float surface[4];  // plane equation
} viewer_fog_t;

typedef struct {
    float origin[3];
    float radius;
    float color[3];
    int overdraw;
} viewer_dlight_t;

typedef struct {
    float pos[3];
    float st[2];
    float lightmap[2];
    float normal[3];
    float color[4];
} viewer_vertex_t;

typedef struct {
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t shaderNum;
    int32_t lightmapNum;
    int32_t surfaceType;
    int32_t bspSurfIndex;
    int32_t fogIndex;  /* 0 = no fog, >0 = index into mesh->fogs + 1 */
    float mins[3];
    float maxs[3];
    VkDescriptorSet descriptorSets[MAX_FRAMES_IN_FLIGHT];
} draw_batch_t;

#ifndef RENDERER_VK
typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
} vk_buffer_t;

typedef struct {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;
} vk_texture_t;
#endif

typedef struct {
    viewer_vertex_t *vertices;
    uint32_t vertexCount;
    uint32_t *indicesAll;
    uint32_t indexCountAll;
    dshader_t *shaders;
    uint32_t shaderCount;
    uint8_t *lightmapData;
    uint32_t lightmapCount;
    draw_batch_t *batches;
    uint32_t batchCount;
    char gameRoot[1024];
    int hasSkyPortal;
    float skyPortalOrigin[3];
    float skyPortalFov;
    int hasSkyPortalFog;
    float skyPortalFogColor[3];
    int skyPortalFogNear;
    int skyPortalFogFar;
    
    /* Material table for shader-based rendering */
    struct material_table_s *materialTable;  /* Forward declaration */

    /* BSP PVS + areaportal (area mask), same semantics as renderer R_MarkLeaves */
    int pvsCullEnabled;
    uint32_t numBspSurfaces;
    uint32_t numBspNodes;
    uint32_t numBspLeafs;
    dplane_t *bspPlanes;
    uint32_t numBspPlanes;
    dnode_t *bspRawNodes;
    dleaf_t *bspLeafs;
    int32_t *leafSurfaceIndices;
    uint32_t numLeafSurfIndices;
    uint8_t *bspVis;
    uint32_t visNumClusters;
    uint32_t visClusterBytes;
    uint8_t *novis;
    uint32_t novisLen;
    uint8_t *surfVisScratch;
    
    /* Global scene fog (from fogvars) */
    int hasGlobalFog;
    float globalFogColor[3];
    float globalFogDensity;
    float globalFogCurrentDensity;
    uint32_t globalFogTransitionStart;
    uint32_t globalFogTransitionDuration;
    
    /* Brush-based volumetric fogs */
    viewer_fog_t *fogs;
    uint32_t fogCount;
} mesh_t;

#ifndef RENDERER_VK
typedef struct {
    SDL_Window *window;
    int width, height;
    int running;
    int patchOnly;
    int noPatch;
    float camPos[3];
    float yaw;
    float pitch;
    int viewerQuadNoDepthWrite;
    int alphaMode;
    float alphaScale;
    int advancedLiquids;
    int skyPortalEnabled;
    int bspViewerPvs;
    int dynamicLightsEnabled;
    float dynamicLightRadius;
    viewer_dlight_t dlights[BSP_VIEWER_MAX_DLIGHTS];
    uint32_t dlightCount;

    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice phys;
    VkDevice dev;
    uint32_t gfxQueueFamily;
    uint32_t presentQueueFamily;
    VkQueue gfxQueue;
    VkQueue presentQueue;
    VkSwapchainKHR swapchain;
    VkFormat swapFormat;
    VkColorSpaceKHR swapColorSpace;
    VkExtent2D swapExtent;
    VkImage *swapImages;
    uint32_t swapImageCount;
    VkImageView *swapViews;
    VkFramebuffer *framebuffers;
    VkRenderPass renderPass;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkPipeline pipelines[VIEWER_PIPELINE_COUNT];
    VkCommandPool cmdPool;
    VkCommandBuffer *cmdBuffers;

    VkImage depthImage;
    VkDeviceMemory depthMemory;
    VkImageView depthView;
    VkFormat depthFormat;

    vk_buffer_t vbo;
    vk_buffer_t iboAll;
    vk_texture_t whiteTexture;
    vk_texture_t *lightmapTextures;
    vk_texture_t *shaderTextures;
    vk_texture_t *materialStageTextures;
    vk_texture_t *materialStageAnimTextures;
    vk_texture_t skyboxTextures[6];
    vk_texture_t skyboxInnerTextures[6];
    VkDescriptorSet *materialStageDescriptorSets;
    VkDescriptorSet *materialStageAnimDescriptorSets;
    VkDescriptorSet skyboxDescriptorSets[6][MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet skyboxInnerDescriptorSets[6][MAX_FRAMES_IN_FLIGHT];
    vk_texture_t fogImage;
    vk_buffer_t skyboxVbo;
    vk_buffer_t skyboxIbo;
    vk_buffer_t skyboxIboFrames[MAX_FRAMES_IN_FLIGHT];
    uint32_t skyboxIndexCount;
    uint32_t skyboxFaceIndexCount;
    int skyboxShaderNum;
    int skyboxReady;
    int skyboxInnerReady;
    int skyVisible;

    VkSemaphore imageAvailable[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinished[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlight[MAX_FRAMES_IN_FLIGHT];
    uint32_t frameIndex;

    mesh_t mesh;
} app_t;
#endif /* !RENDERER_VK */

#endif

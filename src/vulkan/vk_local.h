#ifndef VK_LOCAL_H
#define VK_LOCAL_H

#include <vulkan/vulkan.h>
#include "../renderer/tr_local.h"
#include "../renderer/qgl.h"
#include "../renderer/ktx_load.h"

#define VK_MAX_FRAMES_IN_FLIGHT 2
#define VK_MAX_PIPELINES 41
#define VK_MAX_DESCRIPTOR_SETS 4096
#define VK_MAX_TEXTURES 2048
/* CPU-side fill array: params[15..23] upload to ViewUBO drawParams (9 vec4s).
 * Only params[0..14] are real push constants (maxPushConstantsSize 256 on AMD).
 * Slot layout in ViewUBO (drawParamIndex * 9 + i):
 *   0..5 = former params15..20 (fade / fog / NV)
 *   6    = ambient.rgb + meshLightEnable (.w)
 *   7    = directed.rgb + mdsSkinEnable (.w)
 *   8    = lightDir.xyz + meshBacklerp (.w) */
#define VK_PUSH_PARAMS 24
#define VK_PUSH_PARAMS_GPU 15
#define VK_PUSH_CONSTANTS_GPU_SIZE (16 + (VK_PUSH_PARAMS_GPU) * 16) /* 256 */
#define VK_DRAW_UBO_PARAMS 9
#define VK_MESH_LIGHT_PARAM   21
#define VK_MESH_DIRECT_PARAM  22
#define VK_MESH_LIGHTDIR_PARAM 23

typedef enum {
    VK_PIPELINE_OPAQUE = 0,
    VK_PIPELINE_ALPHA,
    VK_PIPELINE_ADDITIVE,
    VK_PIPELINE_SRC_ALPHA_ONE,
    VK_PIPELINE_FILTER,
    VK_PIPELINE_DSTCOLOR_ONE,
    VK_PIPELINE_ONE_MINUS_DST_ALPHA_ONE,
    VK_PIPELINE_ONE_ONE_MINUS_SRC_ALPHA,
    VK_PIPELINE_DSTCOLOR_ONE_MINUS_DST_ALPHA,
    VK_PIPELINE_ONE_ONE_MINUS_SRC_COLOR,
    VK_PIPELINE_SRC_ALPHA_ONE_MINUS_SRC_COLOR,
    VK_PIPELINE_ZERO_ONE_MINUS_SRC_ALPHA,
    VK_PIPELINE_ZERO_ONE_MINUS_SRC_COLOR,
    VK_PIPELINE_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR,
    VK_PIPELINE_ALPHA_DEPTHWRITE,
    VK_PIPELINE_ADDITIVE_DEPTHWRITE,
    VK_PIPELINE_SRC_ALPHA_ONE_DEPTHWRITE,
    VK_PIPELINE_FILTER_DEPTHWRITE,
    VK_PIPELINE_DSTCOLOR_ONE_DEPTHWRITE,
    VK_PIPELINE_ONE_MINUS_DST_ALPHA_ONE_DEPTHWRITE,
    VK_PIPELINE_ONE_ONE_MINUS_SRC_ALPHA_DEPTHWRITE,
    VK_PIPELINE_DSTCOLOR_ONE_MINUS_DST_ALPHA_DEPTHWRITE,
    VK_PIPELINE_ONE_ONE_MINUS_SRC_COLOR_DEPTHWRITE,
    VK_PIPELINE_SRC_ALPHA_ONE_MINUS_SRC_COLOR_DEPTHWRITE,
    VK_PIPELINE_ZERO_ONE_MINUS_SRC_ALPHA_DEPTHWRITE,
    VK_PIPELINE_ZERO_ONE_MINUS_SRC_COLOR_DEPTHWRITE,
    VK_PIPELINE_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR_DEPTHWRITE,
    VK_PIPELINE_DEPTH_DISABLED_ALPHA,
    VK_PIPELINE_SKY,
    VK_PIPELINE_DLIGHT,
    VK_PIPELINE_SHADOW_STENCIL,
    VK_PIPELINE_2D,
    VK_PIPELINE_2D_OPAQUE,
    VK_PIPELINE_2D_ADDITIVE,
    VK_PIPELINE_2D_MODULATE,
    VK_PIPELINE_2D_SRC_ALPHA_ONE,
    VK_PIPELINE_2D_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR,
    VK_PIPELINE_FILTER_EQUAL,
    VK_PIPELINE_FOG,
    VK_PIPELINE_FOG_EQUAL,
    VK_PIPELINE_COUNT
} vkPipelineIndex_t;

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
} vk_buffer_t;

typedef struct {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;
    int width, height;
    qboolean loaded;
} vk_texture_t;

typedef struct {
    int width, height;
    VkFormat format;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
} vk_depth_t;

typedef struct {
    VkInstance instance;
    VkPhysicalDevice phys;
    VkDevice dev;
    uint32_t gfxFamily;
    uint32_t presentFamily;
    VkQueue gfxQueue;
    VkQueue presentQueue;

    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat swapFormat;
    VkColorSpaceKHR swapColorSpace;
    VkExtent2D swapExtent;
    VkImage *swapImages;
    uint32_t swapCount;
    VkImageView *swapViews;
    VkFramebuffer *framebuffers;

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipelines[VK_PIPELINE_COUNT];
    VkDescriptorSetLayout descSetLayout;
    VkDescriptorPool descPool;
    VkCommandPool cmdPool;
    VkCommandPool uploadPool;
    VkCommandBuffer uploadCmd;
    VkCommandBuffer *cmdBuffers;

    vk_depth_t depth;

    VkSemaphore imageAvailable[VK_MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinished[VK_MAX_FRAMES_IN_FLIGHT];
    VkFence inFlight[VK_MAX_FRAMES_IN_FLIGHT];
    /* Per swapchain image: fence of the last submission that used it.
     * Sized swapCount, VK_NULL_HANDLE when the image is unused. */
    VkFence *imagesInFlight;
    uint32_t frameIndex;
    uint32_t currentImageIndex;

    vk_buffer_t vbo;
    vk_buffer_t ibo;
    int vboCount;
    int iboCount;
    int vboCapacity;
    int iboCapacity;

    vk_texture_t textures[VK_MAX_TEXTURES];
    int textureCount;
    vk_texture_t whiteTexture;
    vk_texture_t lightmapTextures[256];
    int lightmapCount;
    vk_texture_t fogTexture;

    VkDescriptorSet *descriptorSets;
    VkDescriptorSet whiteDescSet;
    VkDescriptorSet fogDescSet;

    VkSurfaceFormatKHR surfaceFormat;

    VkPhysicalDeviceFeatures physFeatures;
    float maxSamplerAnisotropy;

    qboolean active;
    qboolean renderPassActive;
} vk_state_t;

typedef struct {
    uint32_t mvpIndex;
    uint32_t drawParamIndex; /* index into ViewUBO.drawParams[] slots */
    uint32_t pad[2];
    /* params[0..14] → push constants; params[15..20] → ViewUBO draw-param slot */
    float params[VK_PUSH_PARAMS][4];
} vk_push_constants_t;

/* Per-view MVP + per-draw fog/fade param slots (same lifetime as cmd buffer). */
#define VK_VIEW_MAX_MVPS         256
/* Each material push allocates a fog/fade slot. Batched scenes easily exceed
 * 256 pushes/frame; overflowing used to reuse slot 0 and stomp early draws
 * still referenced by the in-flight command buffer → fog flicker on a few
 * surfaces. 2048 + identical-slot reuse keeps this rare. */
#define VK_VIEW_MAX_DRAW_PARAMS  2048
#define VK_VIEW_UBO_MVP_BYTES    (VK_VIEW_MAX_MVPS * 64)
#define VK_VIEW_DRAW_SLOT_BYTES  (VK_DRAW_UBO_PARAMS * 16) /* 96 */
#define VK_VIEW_UBO_DRAW_OFF     VK_VIEW_UBO_MVP_BYTES
#define VK_VIEW_UBO_DRAW_BYTES   (VK_VIEW_MAX_DRAW_PARAMS * VK_VIEW_DRAW_SLOT_BYTES)
#define VK_VIEW_UBO_REGION_SIZE  (VK_VIEW_UBO_MVP_BYTES + VK_VIEW_UBO_DRAW_BYTES)

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    uint8_t *mapped;
    VkDescriptorSetLayout setLayout;
    VkDescriptorSet set;
    uint32_t slotCount;
    uint32_t drawSlotCount;
} vk_view_ubo_t;

extern vk_view_ubo_t vk_viewUbo;
extern uint32_t vk_currentMvpSlot;
extern uint32_t vk_worldMvpSlot;

void VK_ViewFrameBegin(void);
void VK_ViewBegin(void);
uint32_t VK_ViewAllocMvp(const float mvp[16]);
uint32_t VK_ViewAllocDrawParams(const float *drawParams);
void VK_ViewSetEntityMvp(void);
uint32_t VK_ViewGet2DMvpSlot(void);
void VK_ViewBindSet(VkCommandBuffer cmd);
void VK_ViewBindBones(VkCommandBuffer cmd, uint32_t boneSet);
void VK_CmdPushMaterial(VkCommandBuffer cmd, const vk_push_constants_t *pc);

/* Fog layout in params[16]..params[17] (CPU fill → draw-param slot vec4[1..2]).
 * params[16].xyz = fog color, .w = mode (0=off, 1=linear, 2=exp)
 * params[17].x   = fog start
 * params[17].y   = fog end
 * params[17].z   = fog density
 * params[17].w   = fog registered flag (>0.5 active) */
#define VK_FOG_COLOR_PARAM  16
#define VK_FOG_RANGE_PARAM  17

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void *mapped;
    int offset;
} vk_dynamic_vbo_t;

typedef struct {
    void *surfData;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t indexCount;
} vk_world_surf_t;

/* Secondary vertex stream (binding 1), 64 bytes:
 * MD3/MDC lerp: a.xyz=oldPos, b.xyz=oldNormal
 * MDS skin:     a=off0+w0, b=off1+w1, c=off2+w2, d=off3+w3
 *               bone indices in drawVert lightmap.xy + color.rg */
typedef struct {
    float a[4];
    float b[4];
    float c[4];
    float d[4];
} vk_meshAttrib1_t;

#define VK_MESH_ATTRIB1_SIZE ((int)sizeof(vk_meshAttrib1_t))

extern vk_buffer_t vk_meshDummyFrame;

void VK_BindMeshVertexBuffers(VkCommandBuffer cmd, VkBuffer primary, VkDeviceSize primaryOff,
                              VkBuffer oldFrame, VkDeviceSize oldFrameOff);
void VK_BindMeshVertexBuffers4(VkCommandBuffer cmd,
                               VkBuffer b0, VkDeviceSize o0,
                               VkBuffer b1, VkDeviceSize o1,
                               VkBuffer b2, VkDeviceSize o2,
                               VkBuffer b3, VkDeviceSize o3);
qboolean VK_InitMeshDummyFrame(void);
void VK_DestroyMeshDummyFrame(void);

extern vk_buffer_t vk_meshDummyShort;

/* Per-draw MDS bone matrices (set 1, binding 1, dynamic offset). */
#define VK_BONE_MAX_SETS     64
#define VK_BONE_COUNT        MDS_MAX_BONES
#define VK_BONE_VEC4S        (VK_BONE_COUNT * 3)
#define VK_BONE_SET_BYTES    (VK_BONE_VEC4S * 16)
#define VK_BONE_UBO_REGION_SIZE (VK_BONE_MAX_SETS * VK_BONE_SET_BYTES)

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    uint8_t *mapped;
    uint32_t setCount;
} vk_bone_ubo_t;

extern vk_bone_ubo_t vk_boneUbo;
extern uint32_t vk_currentBoneSet;

qboolean VK_SetupBoneUbo(void);
void VK_DestroyBoneUbo(void);
void VK_BoneFrameBegin(void);
uint32_t VK_BoneAllocSet(const mdsBoneFrame_t *bones, int numBones);

/* Dlight UBO (set 1, binding 2): all lights for one multi-light FS pass.
 * Region size is aligned to 256 for minUniformBufferOffsetAlignment. */
#define VK_DLIGHT_MAX            32
#define VK_DLIGHT_UBO_RAW_BYTES  (16 + VK_DLIGHT_MAX * 32)
#define VK_DLIGHT_UBO_BYTES      ((VK_DLIGHT_UBO_RAW_BYTES + 255) & ~255)

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    uint8_t *mapped;
} vk_dlight_ubo_t;

extern vk_dlight_ubo_t vk_dlightUbo;

void VK_UploadDlights(void);
qboolean VK_InitSkyStatic(void);
void VK_DestroySkyStatic(void);

typedef struct {
    VkBuffer vbo;
    VkDeviceMemory vboMemory;
    VkDeviceSize vboSize;
    VkBuffer ibo;
    VkDeviceMemory iboMemory;
    VkDeviceSize iboSize;
    uint32_t *cpuIndices;
    uint32_t cpuIndexCount;
    uint32_t totalVerts;
    vk_world_surf_t *hash;
    uint32_t hashSize;
    uint32_t surfCount;
    qboolean built;
} vk_world_static_t;

extern vk_state_t vk_state;
extern qboolean vk_active;
extern vk_dynamic_vbo_t vk_dyn;
extern vk_world_static_t vk_world;

void VK_Init(void);
void VK_Shutdown(void);
void VK_RenderView(viewParms_t *parms);
void VK_Render2D(void);

qboolean VK_CreateInstance(const char **extensions, uint32_t extCount,
                           const char **layers, uint32_t layerCount);
void VK_DestroyInstance(void);
qboolean VK_InitFromPlatform(int width, int height,
                             VkInstance instance, VkSurfaceKHR surface);

void VK_MatrixPerspective(float *dst, float fovY, float aspect, float zNear, float zFar);
void VK_MatrixView(float *dst, const vec3_t origin, const vec3_t axis[3]);
void VK_MatrixMul(float *dst, const float *a, const float *b);
void VK_MatrixMulQ3Clip(float *dst, const float *proj, const float *mv);
void VK_TransposeMatrix(float *dst, const float *src);
void VK_MatrixIdentity(float *dst);
void VK_ConvertProjectionDepthToVulkan(float m[16]);
void VK_ConvertProjectionMatrixToVulkan(float m[16]);

uint32_t VK_FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props);
qboolean VK_CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                         vk_buffer_t *out, const void *data);
void VK_DestroyBuffer(vk_buffer_t *b);
qboolean VK_CreateTextureFromPixels(const uint8_t *pixels, int w, int h,
                                    vk_texture_t *out, int wrapMode, qboolean mipmap);
qboolean VK_CreateTextureFromKTX(const ktx_texture_t *tex, vk_texture_t *out,
                                 int wrapMode, qboolean mipmap);
void VK_DestroyTexture(vk_texture_t *t);

void VK_InitTextures(void);
int VK_FindOrCreateTexture(const char *name, qboolean mipmap,
                           qboolean allowPicmip, int wrapMode);
void VK_SetupPipelines(void);
void VK_Setup2DPipeline(void);
void VK_SetupDescriptorSetLayout(void);
void VK_SetupDescriptorPool(void);
void VK_SetupRenderPass(void);
void VK_SetupFramebuffers(void);
void VK_DestroySwapchain(void);
void VK_UpdateSwapchain(int width, int height);
void VK_CreateSyncObjects(void);
void VK_SetupCommandBuffers(void);
void VK_AllocateDescriptorSets(void);
VkDescriptorSet VK_GetDescriptorSetForImage(image_t *image);
VkDescriptorSet VK_GetDescriptorSetForImages(image_t *base, image_t *light);
void VK_DrawTessRange(int baseVert, int baseIdx);

void VK_BeginFrame(stereoFrame_t stereoFrame);
void VK_EndFrame(int *frontEndMsec, int *backEndMsec);
void VK_SetColor(const float *rgba);
void VK_Flush2DBatch(void);
void VK_StretchPic(float x, float y, float w, float h,
                   float s1, float t1, float s2, float t2, shader_t *shader);
void VK_StretchPicGradient(float x, float y, float w, float h,
                           float s1, float t1, float s2, float t2, shader_t *shader,
                           const byte gradientColor[4]);
void VK_UploadScratchImage(image_t *image, const byte *data, int cols, int rows, qboolean dirty);
void VK_OnTextureUploaded(int vkIdx);

qboolean VK_InitUploadSlots(void);
void VK_DestroyUploadSlots(void);
void VK_ReapUploadSlot(uint32_t frameIndex);
qboolean VK_UploadGetPending(VkCommandBuffer *outCmd);
void VK_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, int client);
void VK_DrawSurfList(drawSurf_t *drawSurfs, int numDrawSurfs, int glfogNum, const glfog_t *glfog);
void VK_RenderFlares(VkCommandBuffer cmd);

int BlendModeToPipeline(shader_t *shader);
qboolean VK_RegisterModel(model_t *model);
void VK_AddWorldSurfaces(void);
void VK_AddMD3Surfaces(trRefEntity_t *e);
void VK_AddAnimSurfaces(trRefEntity_t *e);
void VK_AddPolygonSurfaces(void);

void VK_TransformDlights(int count, dlight_t *dl, orientationr_t *or);
void VK_SetupEntityLighting(const trRefdef_t *refdef, trRefEntity_t *ent);

qboolean VK_InitDynamicVBO(void);
void VK_ResetDynamicVBO(void);
void VK_DestroyDynamicVBO(void);

void VK_BuildWorldStaticBuffers(void);
void VK_DestroyWorldStaticBuffers(void);
const vk_world_surf_t *VK_WorldGetStaticSurf(void *surfData);
const uint32_t *VK_WorldGetCpuIndices(void);

void VK_DestroyMeshCaches(void);

/* Set while VK_FogPass draws surfaces: mesh paths must not wipe fog push state. */
extern int vk_volumetricFogPass;
extern vk_push_constants_t vk_fogPassPush;

extern void (*vk_surfaceTable[SF_NUM_SURFACE_TYPES])(void *);

extern int VK_ReserveDynamicVBO(int size);
extern vk_dynamic_vbo_t vk_dyn;

#endif

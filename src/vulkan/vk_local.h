#ifndef VK_LOCAL_H
#define VK_LOCAL_H

#include <vulkan/vulkan.h>
#include "../renderer/tr_local.h"
#include "../renderer/qgl.h"

#define VK_MAX_FRAMES_IN_FLIGHT 2
#define VK_MAX_PIPELINES 41
#define VK_MAX_DESCRIPTOR_SETS 4096
#define VK_MAX_TEXTURES 2048
#define VK_PUSH_PARAMS 21

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
    float mvp[16];
    float params[VK_PUSH_PARAMS][4];
} vk_push_constants_t;

/* Fog push-constant layout (params[16]..params[17]).
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

extern vk_state_t vk_state;
extern qboolean vk_active;
extern vk_dynamic_vbo_t vk_dyn;

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
void VK_StretchPic(float x, float y, float w, float h,
                   float s1, float t1, float s2, float t2, shader_t *shader);
void VK_StretchPicGradient(float x, float y, float w, float h,
                           float s1, float t1, float s2, float t2, shader_t *shader,
                           const byte gradientColor[4]);
void VK_UploadScratchImage(image_t *image, const byte *data, int cols, int rows, qboolean dirty);
void VK_OnTextureUploaded(int vkIdx);
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

extern void (*vk_surfaceTable[SF_NUM_SURFACE_TYPES])(void *);

extern int VK_ReserveDynamicVBO(int size);
extern vk_dynamic_vbo_t vk_dyn;

#endif

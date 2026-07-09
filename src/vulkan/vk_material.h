#ifndef VK_MATERIAL_H
#define VK_MATERIAL_H

#include "vk_local.h"

int VK_PipelineForStage(const shaderStage_t *stage);
qboolean VK_StageIsBlended(const shaderStage_t *stage);
qboolean VK_RenderPassMatchesStage(int pass, qboolean polygonOffset, qboolean blended);
qboolean VK_StageUsesSourceAlphaBlend(const shaderStage_t *stage);

void VK_SetStageStateFromShader(const shader_t *shader, const shaderStage_t *stage);
void VK_SetUIStageStateFromShader(const shader_t *shader, const shaderStage_t *stage);
void VK_FillPushConstants(const float mvp[16], const shader_t *shader,
                          vk_push_constants_t *pc, float params[VK_PUSH_PARAMS][4]);
void VK_FillFogPushConstants(float params[VK_PUSH_PARAMS][4]);
void VK_FillPicStageColors(const shader_t *shader, const shaderStage_t *stage,
                           const byte color0[4], const byte color1[4],
                           const byte color2[4], const byte color3[4],
                           byte outColors[4][4]);
int VK_PipelineForUIStage(const shaderStage_t *stage);
int VK_PipelineFor2DPic(const shaderStage_t *stage);
void VK_SetSkyPushConstants(const shader_t *shader, const shaderStage_t *stage,
                            float params[VK_PUSH_PARAMS][4], qboolean cloudLayer);

image_t *VK_BundleImage(const textureBundle_t *bundle, const shader_t *shader);
image_t *VK_StageLightmapImage(const shader_t *shader);
VkDescriptorSet VK_StageDescriptorSet(const shader_t *shader, const shaderStage_t *stage);

int VK_MaterialPolygonOffset(void);
float VK_MaterialPolyOffsetFactor(void);
float VK_MaterialPolyOffsetUnits(void);

#endif

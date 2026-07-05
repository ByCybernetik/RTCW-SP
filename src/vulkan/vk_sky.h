#ifndef VK_SKY_H
#define VK_SKY_H

void VK_DrawSky(shader_t *shader, surfaceType_t *surf, VkCommandBuffer cmd);
void VK_DrawSun(VkCommandBuffer cmd);

#endif

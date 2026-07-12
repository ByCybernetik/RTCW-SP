#include "vk_local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

vk_state_t vk_state;
qboolean vk_active = qfalse;

static qboolean VK_CreateWhiteTexture(void);
static qboolean VK_CreateFogTexture(void);
void VK_DestroyTexture(vk_texture_t *t);

static VkShaderModule LoadSPIRV(const char *path) {
    uint8_t *data;
    VkShaderModule module = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    int len;

    len = ri.FS_ReadFile(path, (void **)&data);
    if (len <= 0 || !data) return VK_NULL_HANDLE;

    ci.codeSize = len;
    ci.pCode = (const uint32_t *)data;
    if (vkCreateShaderModule(vk_state.dev, &ci, NULL, &module) != VK_SUCCESS)
        module = VK_NULL_HANDLE;

    ri.FS_FreeFile(data);
    return module;
}

static int PickQueues(void) {
    uint32_t count = 0;
    VkQueueFamilyProperties *props;

    vkGetPhysicalDeviceQueueFamilyProperties(vk_state.phys, &count, NULL);
    if (!count) return 0;
    props = malloc(sizeof(*props) * count);
    vkGetPhysicalDeviceQueueFamilyProperties(vk_state.phys, &count, props);

    vk_state.gfxFamily = UINT32_MAX;
    vk_state.presentFamily = UINT32_MAX;
    for (uint32_t i = 0; i < count; i++) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(vk_state.phys, i, vk_state.surface, &present);
        if ((props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && vk_state.gfxFamily == UINT32_MAX)
            vk_state.gfxFamily = i;
        if (present && vk_state.presentFamily == UINT32_MAX)
            vk_state.presentFamily = i;
    }
    free(props);
    return (vk_state.gfxFamily != UINT32_MAX && vk_state.presentFamily != UINT32_MAX);
}

static void CreateSwapchain(int width, int height) {
    VkSurfaceCapabilitiesKHR caps;
    VkSwapchainCreateInfoKHR ci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    uint32_t formatCount, presentCount;
    VkSurfaceFormatKHR *formats;
    VkPresentModeKHR *modes;
    VkPresentModeKHR chosenMode = VK_PRESENT_MODE_FIFO_KHR;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_state.phys, vk_state.surface, &caps);

    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_state.phys, vk_state.surface, &formatCount, NULL);
    formats = malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_state.phys, vk_state.surface, &formatCount, formats);
    vk_state.swapFormat = formats[0].format;
    vk_state.swapColorSpace = formats[0].colorSpace;
    if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        vk_state.swapFormat = VK_FORMAT_B8G8R8A8_UNORM;
    }
    for (uint32_t i = 0; i < formatCount; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
            vk_state.swapFormat = formats[i].format;
            vk_state.swapColorSpace = formats[i].colorSpace;
            break;
        }
    }
    free(formats);

    vkGetPhysicalDeviceSurfacePresentModesKHR(vk_state.phys, vk_state.surface, &presentCount, NULL);
    modes = malloc(presentCount * sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(vk_state.phys, vk_state.surface, &presentCount, modes);
    for (uint32_t i = 0; i < presentCount; i++) {
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            chosenMode = modes[i];
            break;
        }
        if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR && chosenMode != VK_PRESENT_MODE_MAILBOX_KHR)
            chosenMode = modes[i];
    }
    free(modes);

    vk_state.swapExtent = caps.currentExtent;
    if (vk_state.swapExtent.width == 0xFFFFFFFF) {
        vk_state.swapExtent.width = width;
        vk_state.swapExtent.height = height;
    }

    ci.surface = vk_state.surface;
    ci.minImageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && ci.minImageCount > caps.maxImageCount)
        ci.minImageCount = caps.maxImageCount;
    ci.imageFormat = vk_state.swapFormat;
    ci.imageColorSpace = vk_state.swapColorSpace;
    ci.imageExtent = vk_state.swapExtent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = chosenMode;
    ci.clipped = VK_TRUE;

    if (vk_state.gfxFamily != vk_state.presentFamily) {
        uint32_t families[] = { vk_state.gfxFamily, vk_state.presentFamily };
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = families;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    vkCreateSwapchainKHR(vk_state.dev, &ci, NULL, &vk_state.swapchain);
    vkGetSwapchainImagesKHR(vk_state.dev, vk_state.swapchain, &vk_state.swapCount, NULL);
    vk_state.swapImages = malloc(vk_state.swapCount * sizeof(VkImage));
    vkGetSwapchainImagesKHR(vk_state.dev, vk_state.swapchain, &vk_state.swapCount, vk_state.swapImages);
}

static void CreateDepthImage(void) {
    VkFormat depthFormats[] = {
        VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM
    };
    VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkImageViewCreateInfo ivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    VkMemoryRequirements req;
    VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    vk_state.depth.format = VK_FORMAT_UNDEFINED;
    for (int i = 0; i < 3; i++) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(vk_state.phys, depthFormats[i], &props);
        if ((props.optimalTilingFeatures & features) == features) {
            vk_state.depth.format = depthFormats[i];
            break;
        }
    }
    if (vk_state.depth.format == VK_FORMAT_UNDEFINED) return;

    vk_state.depth.width = vk_state.swapExtent.width;
    vk_state.depth.height = vk_state.swapExtent.height;

    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent.width = vk_state.depth.width;
    ici.extent.height = vk_state.depth.height;
    ici.extent.depth = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.format = vk_state.depth.format;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    vkCreateImage(vk_state.dev, &ici, NULL, &vk_state.depth.image);

    vkGetImageMemoryRequirements(vk_state.dev, vk_state.depth.image, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = VK_FindMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(vk_state.dev, &ai, NULL, &vk_state.depth.memory);
    vkBindImageMemory(vk_state.dev, vk_state.depth.image, vk_state.depth.memory, 0);

    ivci.image = vk_state.depth.image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = vk_state.depth.format;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;
    vkCreateImageView(vk_state.dev, &ivci, NULL, &vk_state.depth.view);
}

static void CreateSwapchainViews(void) {
    VkImageViewCreateInfo ci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = vk_state.swapFormat;
    ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.layerCount = 1;

    vk_state.swapViews = malloc(vk_state.swapCount * sizeof(VkImageView));
    for (uint32_t i = 0; i < vk_state.swapCount; i++) {
        ci.image = vk_state.swapImages[i];
        vkCreateImageView(vk_state.dev, &ci, NULL, &vk_state.swapViews[i]);
    }
}

void VK_SetupRenderPass(void) {
    VkAttachmentDescription attachments[2];
    VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass = { 0 };
    VkSubpassDependency dep[2] = {0};
    VkRenderPassCreateInfo ci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };

    attachments[0].format = vk_state.swapFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].format = vk_state.depth.format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    dep[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dep[0].dstSubpass = 0;
    dep[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep[0].srcAccessMask = 0;
    dep[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    dep[1].srcSubpass = VK_SUBPASS_EXTERNAL;
    dep[1].dstSubpass = 0;
    dep[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep[1].srcAccessMask = 0;
    dep[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    ci.attachmentCount = 2;
    ci.pAttachments = attachments;
    ci.subpassCount = 1;
    ci.pSubpasses = &subpass;
    ci.dependencyCount = 2;
    ci.pDependencies = dep;

    vkCreateRenderPass(vk_state.dev, &ci, NULL, &vk_state.renderPass);
}

void VK_SetupDescriptorSetLayout(void) {
    VkDescriptorSetLayoutBinding bindings[2];
    VkDescriptorSetLayoutCreateInfo ci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = NULL;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = NULL;

    ci.bindingCount = 2;
    ci.pBindings = bindings;
    vkCreateDescriptorSetLayout(vk_state.dev, &ci, NULL, &vk_state.descSetLayout);
}

void VK_SetupDescriptorPool(void) {
    VkDescriptorPoolSize poolSizes[2];
    VkDescriptorPoolCreateInfo ci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = VK_MAX_DESCRIPTOR_SETS * 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = VK_MAX_DESCRIPTOR_SETS * 2;

    ci.maxSets = VK_MAX_DESCRIPTOR_SETS;
    ci.poolSizeCount = 2;
    ci.pPoolSizes = poolSizes;
    vkCreateDescriptorPool(vk_state.dev, &ci, NULL, &vk_state.descPool);
}

void VK_SetupPipelineLayout(void) {
    VkPushConstantRange pcRange;
    VkPipelineLayoutCreateInfo ci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(vk_push_constants_t);

    ci.setLayoutCount = 1;
    ci.pSetLayouts = &vk_state.descSetLayout;
    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges = &pcRange;
    vkCreatePipelineLayout(vk_state.dev, &ci, NULL, &vk_state.pipelineLayout);
}

int BlendModeToPipeline(shader_t *shader) {
    unsigned stateBits = shader->stages[0] ? shader->stages[0]->stateBits : 0;
    int srcBlend = stateBits & GLS_SRCBLEND_BITS;
    int dstBlend = stateBits & GLS_DSTBLEND_BITS;
    int depthWrite = (stateBits & GLS_DEPTHMASK_TRUE) ? 1 : 0;
    int alphaTest = (stateBits & GLS_ATEST_BITS) ? 1 : 0;
    int isSky = shader->isSky ? 1 : 0;

    if (isSky) return VK_PIPELINE_SKY;
    if (srcBlend == 0 && dstBlend == 0) {
        return alphaTest ? VK_PIPELINE_ALPHA : VK_PIPELINE_OPAQUE;
    }
    if (srcBlend == GLS_SRCBLEND_ONE && dstBlend == GLS_DSTBLEND_ONE)
        return depthWrite ? VK_PIPELINE_ADDITIVE_DEPTHWRITE : VK_PIPELINE_ADDITIVE;
    if (srcBlend == GLS_SRCBLEND_SRC_ALPHA && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)
        return depthWrite ? VK_PIPELINE_ALPHA_DEPTHWRITE : VK_PIPELINE_ALPHA;
    if (srcBlend == GLS_SRCBLEND_DST_COLOR && (dstBlend == GLS_DSTBLEND_ZERO || dstBlend == 0))
        return depthWrite ? VK_PIPELINE_FILTER_DEPTHWRITE : VK_PIPELINE_FILTER;
    if (srcBlend == GLS_SRCBLEND_DST_COLOR && dstBlend == GLS_DSTBLEND_ONE)
        return depthWrite ? VK_PIPELINE_DSTCOLOR_ONE_DEPTHWRITE : VK_PIPELINE_DSTCOLOR_ONE;
    if (srcBlend == GLS_SRCBLEND_ZERO && dstBlend == GLS_DSTBLEND_SRC_COLOR)
        return depthWrite ? VK_PIPELINE_ONE_MINUS_DST_ALPHA_ONE_DEPTHWRITE : VK_PIPELINE_ONE_MINUS_DST_ALPHA_ONE;
    if (srcBlend == GLS_SRCBLEND_ONE && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)
        return depthWrite ? VK_PIPELINE_ONE_ONE_MINUS_SRC_ALPHA_DEPTHWRITE : VK_PIPELINE_ONE_ONE_MINUS_SRC_ALPHA;
    if (srcBlend == GLS_SRCBLEND_ONE && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR)
        return depthWrite ? VK_PIPELINE_ONE_ONE_MINUS_SRC_COLOR_DEPTHWRITE : VK_PIPELINE_ONE_ONE_MINUS_SRC_COLOR;
    if (srcBlend == GLS_SRCBLEND_SRC_ALPHA && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR)
        return depthWrite ? VK_PIPELINE_SRC_ALPHA_ONE_MINUS_SRC_COLOR_DEPTHWRITE : VK_PIPELINE_SRC_ALPHA_ONE_MINUS_SRC_COLOR;
    if (srcBlend == GLS_SRCBLEND_ZERO && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)
        return depthWrite ? VK_PIPELINE_ZERO_ONE_MINUS_SRC_ALPHA_DEPTHWRITE : VK_PIPELINE_ZERO_ONE_MINUS_SRC_ALPHA;
    if (srcBlend == GLS_SRCBLEND_ZERO && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR)
        return depthWrite ? VK_PIPELINE_ZERO_ONE_MINUS_SRC_COLOR_DEPTHWRITE : VK_PIPELINE_ZERO_ONE_MINUS_SRC_COLOR;
    if (srcBlend == GLS_SRCBLEND_ONE_MINUS_DST_COLOR && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR)
        return depthWrite ? VK_PIPELINE_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR_DEPTHWRITE : VK_PIPELINE_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR;

    return alphaTest ? VK_PIPELINE_ALPHA : VK_PIPELINE_OPAQUE;
}

static void VK_ConfigurePipelineBlend(int pipelineIndex, VkPipelineColorBlendAttachmentState *cba) {
    if (!cba) {
        return;
    }

    memset(cba, 0, sizeof(*cba));
    cba->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    switch (pipelineIndex) {
    case VK_PIPELINE_OPAQUE:
        cba->blendEnable = VK_FALSE;
        break;
    case VK_PIPELINE_ALPHA:
    case VK_PIPELINE_ALPHA_DEPTHWRITE:
    case VK_PIPELINE_DEPTH_DISABLED_ALPHA:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case VK_PIPELINE_ADDITIVE:
    case VK_PIPELINE_ADDITIVE_DEPTHWRITE:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;
    case VK_PIPELINE_SRC_ALPHA_ONE:
    case VK_PIPELINE_SRC_ALPHA_ONE_DEPTHWRITE:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;
    case VK_PIPELINE_FILTER:
    case VK_PIPELINE_FILTER_DEPTHWRITE:
    case VK_PIPELINE_FILTER_EQUAL:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        break;
    case VK_PIPELINE_DSTCOLOR_ONE:
    case VK_PIPELINE_DSTCOLOR_ONE_DEPTHWRITE:
    case VK_PIPELINE_DLIGHT:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;
    case VK_PIPELINE_FOG:
    case VK_PIPELINE_FOG_EQUAL:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case VK_PIPELINE_ONE_MINUS_DST_ALPHA_ONE:
    case VK_PIPELINE_ONE_MINUS_DST_ALPHA_ONE_DEPTHWRITE:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;
    case VK_PIPELINE_ONE_ONE_MINUS_SRC_ALPHA:
    case VK_PIPELINE_ONE_ONE_MINUS_SRC_ALPHA_DEPTHWRITE:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case VK_PIPELINE_DSTCOLOR_ONE_MINUS_DST_ALPHA:
    case VK_PIPELINE_DSTCOLOR_ONE_MINUS_DST_ALPHA_DEPTHWRITE:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        break;
    case VK_PIPELINE_ONE_ONE_MINUS_SRC_COLOR:
    case VK_PIPELINE_ONE_ONE_MINUS_SRC_COLOR_DEPTHWRITE:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case VK_PIPELINE_SRC_ALPHA_ONE_MINUS_SRC_COLOR:
    case VK_PIPELINE_SRC_ALPHA_ONE_MINUS_SRC_COLOR_DEPTHWRITE:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case VK_PIPELINE_ZERO_ONE_MINUS_SRC_ALPHA:
    case VK_PIPELINE_ZERO_ONE_MINUS_SRC_ALPHA_DEPTHWRITE:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case VK_PIPELINE_ZERO_ONE_MINUS_SRC_COLOR:
    case VK_PIPELINE_ZERO_ONE_MINUS_SRC_COLOR_DEPTHWRITE:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        break;
    case VK_PIPELINE_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR:
    case VK_PIPELINE_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR_DEPTHWRITE:
        cba->blendEnable = VK_TRUE;
        cba->srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        cba->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        break;
    case VK_PIPELINE_SKY:
    case VK_PIPELINE_SHADOW_STENCIL:
        cba->blendEnable = VK_FALSE;
        break;
    default:
        cba->blendEnable = VK_FALSE;
        break;
    }
}

void VK_SetupPipelines(void) {
    VkShaderModule vertMod, fragMod;
    VkPipelineShaderStageCreateInfo stages[2];
    VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkVertexInputBindingDescription binding;
    VkVertexInputAttributeDescription attrs[5];
    VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    VkPipelineColorBlendAttachmentState cba[VK_PIPELINE_COUNT];
    VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    VkDynamicState dyn[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS
    };
    VkPipelineDynamicStateCreateInfo dynCi = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    VkGraphicsPipelineCreateInfo gpci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    VkViewport viewport = { 0 };
    VkRect2D scissor = { 0 };

    const char *shaderPaths[][2] = {
        {"world_vert.spv", "world_frag.spv"},
        {"2d_vert.spv", "2d_frag.spv"},
        {"tess_vert.spv", "tess_frag.spv"},
        {NULL, NULL}
    };

    vertMod = VK_NULL_HANDLE;
    fragMod = VK_NULL_HANDLE;
    for (int i = 0; shaderPaths[i][0]; i++) {
        vertMod = LoadSPIRV(shaderPaths[i][0]);
        fragMod = LoadSPIRV(shaderPaths[i][1]);
        if (vertMod && fragMod) break;
        if (vertMod) { vkDestroyShaderModule(vk_state.dev, vertMod, NULL); vertMod = VK_NULL_HANDLE; }
        if (fragMod) { vkDestroyShaderModule(vk_state.dev, fragMod, NULL); fragMod = VK_NULL_HANDLE; }
    }

    if (!vertMod || !fragMod) {
        ri.Printf(PRINT_WARNING, "VK_SetupPipelines: could not load any SPIR-V shaders\n");
        return;
    }

    memset(stages, 0, sizeof(stages));
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    binding.binding = 0;
    binding.stride = sizeof(vec3_t) + sizeof(vec2_t) * 2 + sizeof(vec3_t) + sizeof(byte) * 4;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = 0;
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = sizeof(vec3_t);
    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset = sizeof(vec3_t) + sizeof(vec2_t);
    attrs[3].location = 3;
    attrs[3].binding = 0;
    attrs[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[3].offset = sizeof(vec3_t) + sizeof(vec2_t) * 2;
    attrs[4].location = 4;
    attrs[4].binding = 0;
    attrs[4].format = VK_FORMAT_R8G8B8A8_UNORM;
    attrs[4].offset = sizeof(vec3_t) + sizeof(vec2_t) * 2 + sizeof(vec3_t);

    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = 5;
    vi.pVertexAttributeDescriptions = attrs;

    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia.primitiveRestartEnable = VK_FALSE;

    vp.viewportCount = 1;
    vp.pViewports = &viewport;
    vp.scissorCount = 1;
    vp.pScissors = &scissor;

    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthBiasEnable = VK_TRUE;
    rs.lineWidth = 1.0f;

    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    cb.logicOpEnable = VK_FALSE;
    cb.attachmentCount = 1;
    cb.pAttachments = cba;

    dynCi.dynamicStateCount = 3;
    dynCi.pDynamicStates = dyn;

    for (int i = 0; i < VK_PIPELINE_COUNT; i++) {
        int useDepthWrite = 0;
        int useDepthTest = 1;

        if (i == VK_PIPELINE_2D || i == VK_PIPELINE_2D_OPAQUE ||
            i == VK_PIPELINE_2D_ADDITIVE || i == VK_PIPELINE_2D_MODULATE ||
            i == VK_PIPELINE_2D_SRC_ALPHA_ONE ||
            i == VK_PIPELINE_2D_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR) {
            continue;
        }

        VK_ConfigurePipelineBlend(i, &cba[i]);

        if (i == VK_PIPELINE_OPAQUE ||
            (i >= VK_PIPELINE_ALPHA_DEPTHWRITE &&
             i <= VK_PIPELINE_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR_DEPTHWRITE)) {
            useDepthWrite = 1;
        }

        if (i == VK_PIPELINE_DEPTH_DISABLED_ALPHA) {
            useDepthTest = 0;
            useDepthWrite = 0;
        } else if (i == VK_PIPELINE_SKY) {
            useDepthTest = 0;
            useDepthWrite = 0;
        } else if (i == VK_PIPELINE_DLIGHT || i == VK_PIPELINE_FILTER_EQUAL ||
                   i == VK_PIPELINE_FOG || i == VK_PIPELINE_FOG_EQUAL) {
            useDepthTest = 1;
            useDepthWrite = 0;
        } else if (i == VK_PIPELINE_SHADOW_STENCIL) {
            useDepthTest = 0;
            useDepthWrite = 0;
        }

        memset(&ds, 0, sizeof(ds));
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable = useDepthTest;
        ds.depthWriteEnable = useDepthWrite;
        ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        ds.depthBoundsTestEnable = VK_FALSE;
        ds.stencilTestEnable = VK_FALSE;

        if (i == VK_PIPELINE_SHADOW_STENCIL) {
            ds.stencilTestEnable = VK_TRUE;
            ds.front.failOp = VK_STENCIL_OP_KEEP;
            ds.front.passOp = VK_STENCIL_OP_REPLACE;
            ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
            ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
            ds.front.compareMask = 0xFF;
            ds.front.writeMask = 0xFF;
            ds.back = ds.front;
        }

        if (i == VK_PIPELINE_DLIGHT || i == VK_PIPELINE_FILTER_EQUAL ||
            i == VK_PIPELINE_FOG_EQUAL) {
            ds.depthCompareOp = VK_COMPARE_OP_EQUAL;
        }

        /* Regular fog pass must use LEQUAL to match OpenGL's default depth
         * function. The fog pass draws the same surface geometry at the same
         * depth; with LESS it fails when depth values are exactly equal,
         * causing fog to disappear on opaque surfaces at certain angles. */
        if (i == VK_PIPELINE_FOG) {
            ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        }

        rs.cullMode = VK_CULL_MODE_NONE;

        cb.pAttachments = &cba[i];

        memset(&gpci, 0, sizeof(gpci));
        gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpci.stageCount = 2;
        gpci.pStages = stages;
        gpci.pVertexInputState = &vi;
        gpci.pInputAssemblyState = &ia;
        gpci.pViewportState = &vp;
        gpci.pRasterizationState = &rs;
        gpci.pMultisampleState = &ms;
        gpci.pDepthStencilState = &ds;
        gpci.pColorBlendState = &cb;
        gpci.pDynamicState = &dynCi;
        gpci.layout = vk_state.pipelineLayout;
        gpci.renderPass = vk_state.renderPass;
        gpci.subpass = 0;

        if (vkCreateGraphicsPipelines(vk_state.dev, VK_NULL_HANDLE, 1, &gpci, NULL,
                                      &vk_state.pipelines[i]) != VK_SUCCESS) {
            ri.Printf(PRINT_WARNING, "VK_SetupPipelines: failed to create pipeline %d\n", i);
        }
    }

    vkDestroyShaderModule(vk_state.dev, vertMod, NULL);
    vkDestroyShaderModule(vk_state.dev, fragMod, NULL);
}

void VK_SetupFramebuffers(void) {
    VkFramebufferCreateInfo ci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    VkImageView attachments[2];

    attachments[1] = vk_state.depth.view;
    ci.renderPass = vk_state.renderPass;
    ci.attachmentCount = 2;
    ci.pAttachments = attachments;
    ci.width = vk_state.swapExtent.width;
    ci.height = vk_state.swapExtent.height;
    ci.layers = 1;

    vk_state.framebuffers = malloc(vk_state.swapCount * sizeof(VkFramebuffer));
    for (uint32_t i = 0; i < vk_state.swapCount; i++) {
        attachments[0] = vk_state.swapViews[i];
        vkCreateFramebuffer(vk_state.dev, &ci, NULL, &vk_state.framebuffers[i]);
    }
}

void VK_CreateSyncObjects(void) {
    VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        vkCreateSemaphore(vk_state.dev, &sci, NULL, &vk_state.imageAvailable[i]);
        vkCreateSemaphore(vk_state.dev, &sci, NULL, &vk_state.renderFinished[i]);
        vkCreateFence(vk_state.dev, &fci, NULL, &vk_state.inFlight[i]);
    }
}

void VK_SetupCommandBuffers(void) {
    VkCommandPoolCreateInfo cpci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };

    cpci.queueFamilyIndex = vk_state.gfxFamily;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(vk_state.dev, &cpci, NULL, &vk_state.cmdPool);

    cbai.commandPool = vk_state.cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = vk_state.swapCount;
    vk_state.cmdBuffers = malloc(vk_state.swapCount * sizeof(VkCommandBuffer));
    vkAllocateCommandBuffers(vk_state.dev, &cbai, vk_state.cmdBuffers);
}

void VK_UpdateSwapchain(int width, int height) {
    VK_DestroySwapchain();
    CreateSwapchain(width, height);
    CreateDepthImage();
    CreateSwapchainViews();
    VK_SetupFramebuffers();
}

uint32_t VK_FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem;
    vkGetPhysicalDeviceMemoryProperties(vk_state.phys, &mem);
    for (uint32_t i = 0; i < mem.memoryTypeCount; i++)
        if ((typeBits & (1 << i)) && (mem.memoryTypes[i].propertyFlags & props) == props)
            return i;
    return 0;
}

static int ChoosePhysicalDevice(void) {
    uint32_t count = 0;
    VkPhysicalDevice *devices;

    vkEnumeratePhysicalDevices(vk_state.instance, &count, NULL);
    if (!count) return 0;
    devices = malloc(count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(vk_state.instance, &count, devices);

    vk_state.phys = devices[0];
    for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        vkGetPhysicalDeviceFeatures(devices[i], &features);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            vk_state.phys = devices[i];
            vk_state.physFeatures = features;
            vk_state.maxSamplerAnisotropy = props.limits.maxSamplerAnisotropy;
            break;
        }
    }
    free(devices);

    if (!vk_state.physFeatures.samplerAnisotropy) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(vk_state.phys, &props);
        vkGetPhysicalDeviceFeatures(vk_state.phys, &vk_state.physFeatures);
        vk_state.maxSamplerAnisotropy = props.limits.maxSamplerAnisotropy;
    }

    vk_state.surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
    vk_state.surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    return 1;
}

qboolean VK_CreateInstance(const char **extensions, uint32_t extCount,
                           const char **layers, uint32_t layerCount) {
    VkApplicationInfo ai = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    VkInstanceCreateInfo ci = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };

    ai.pApplicationName = "RTCW-SP (Vulkan)";
    ai.applicationVersion = VK_MAKE_VERSION(1, 41, 0);
    ai.pEngineName = "idTech3-VK";
    ai.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    ai.apiVersion = VK_API_VERSION_1_0;

    ci.pApplicationInfo = &ai;
    ci.enabledExtensionCount = extCount;
    ci.ppEnabledExtensionNames = extensions;
    ci.enabledLayerCount = layerCount;
    ci.ppEnabledLayerNames = layers;

    return (vkCreateInstance(&ci, NULL, &vk_state.instance) == VK_SUCCESS);
}

void VK_DestroyInstance(void) {
    if (vk_state.instance) {
        vkDestroyInstance(vk_state.instance, NULL);
        vk_state.instance = VK_NULL_HANDLE;
    }
}

qboolean VK_InitFromPlatform(int width, int height,
                             VkInstance instance, VkSurfaceKHR surface) {
    VkDeviceQueueCreateInfo qci[2];
    VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    VkPhysicalDeviceFeatures features;
    float qprio = 1.0f;
    int qc = 0;
    const char *devExts[] = { "VK_KHR_swapchain" };

    vk_state.instance = instance;
    vk_state.surface = surface;

    if (!ChoosePhysicalDevice()) return qfalse;
    if (!PickQueues()) return qfalse;

    memset(qci, 0, sizeof(qci));
    qci[qc].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci[qc].queueFamilyIndex = vk_state.gfxFamily;
    qci[qc].queueCount = 1;
    qci[qc].pQueuePriorities = &qprio;
    qc++;
    if (vk_state.gfxFamily != vk_state.presentFamily) {
        qci[qc].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci[qc].queueFamilyIndex = vk_state.presentFamily;
        qci[qc].queueCount = 1;
        qci[qc].pQueuePriorities = &qprio;
        qc++;
    }

    features = vk_state.physFeatures;
    if (features.samplerAnisotropy) {
        features.samplerAnisotropy = VK_TRUE;
    }

    dci.queueCreateInfoCount = qc;
    dci.pQueueCreateInfos = qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = devExts;
    dci.pEnabledFeatures = &features;

    if (vkCreateDevice(vk_state.phys, &dci, NULL, &vk_state.dev) != VK_SUCCESS)
        return qfalse;

    vkGetDeviceQueue(vk_state.dev, vk_state.gfxFamily, 0, &vk_state.gfxQueue);
    vkGetDeviceQueue(vk_state.dev, vk_state.presentFamily, 0, &vk_state.presentQueue);

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_state.phys, vk_state.surface, &(VkSurfaceCapabilitiesKHR){0});

    CreateSwapchain(width, height);
    CreateDepthImage();
    CreateSwapchainViews();
    VK_SetupRenderPass();
    VK_SetupDescriptorSetLayout();
    VK_SetupDescriptorPool();
    VK_SetupPipelineLayout();
    VK_SetupPipelines();
    VK_Setup2DPipeline();
    VK_SetupFramebuffers();
    VK_CreateSyncObjects();
    VK_SetupCommandBuffers();

    {
        VkCommandPoolCreateInfo upci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        VkCommandBufferAllocateInfo ucbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };

        upci.queueFamilyIndex = vk_state.gfxFamily;
        upci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        if (vkCreateCommandPool(vk_state.dev, &upci, NULL, &vk_state.uploadPool) != VK_SUCCESS)
            return qfalse;

        ucbai.commandPool = vk_state.uploadPool;
        ucbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ucbai.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(vk_state.dev, &ucbai, &vk_state.uploadCmd) != VK_SUCCESS)
            return qfalse;
    }

    if (!VK_InitDynamicVBO()) {
        ri.Printf(PRINT_WARNING, "VK_InitFromPlatform: could not create dynamic VBO\n");
        return qfalse;
    }

    if (!VK_CreateWhiteTexture()) {
        ri.Printf(PRINT_WARNING, "VK_InitFromPlatform: could not create white texture\n");
        return qfalse;
    }

    if (!VK_CreateFogTexture()) {
        ri.Printf(PRINT_WARNING, "VK_InitFromPlatform: could not create fog texture\n");
        return qfalse;
    }

    VK_AllocateDescriptorSets();

    return qtrue;
}

void VK_DestroySwapchain(void) {
    vkDeviceWaitIdle(vk_state.dev);

    for (uint32_t i = 0; i < vk_state.swapCount; i++) {
        if (vk_state.framebuffers && vk_state.framebuffers[i])
            vkDestroyFramebuffer(vk_state.dev, vk_state.framebuffers[i], NULL);
        if (vk_state.swapViews && vk_state.swapViews[i])
            vkDestroyImageView(vk_state.dev, vk_state.swapViews[i], NULL);
    }
    free(vk_state.framebuffers);
    free(vk_state.swapViews);
    free(vk_state.swapImages);

    if (vk_state.depth.view)
        vkDestroyImageView(vk_state.dev, vk_state.depth.view, NULL);
    if (vk_state.depth.memory)
        vkFreeMemory(vk_state.dev, vk_state.depth.memory, NULL);
    if (vk_state.depth.image)
        vkDestroyImage(vk_state.dev, vk_state.depth.image, NULL);

    if (vk_state.swapchain)
        vkDestroySwapchainKHR(vk_state.dev, vk_state.swapchain, NULL);

    vk_state.framebuffers = NULL;
    vk_state.swapViews = NULL;
    vk_state.swapImages = NULL;
}

void VK_DestroyTexture(vk_texture_t *t) {
    if (t->sampler) vkDestroySampler(vk_state.dev, t->sampler, NULL);
    if (t->view) vkDestroyImageView(vk_state.dev, t->view, NULL);
    if (t->image) vkDestroyImage(vk_state.dev, t->image, NULL);
    if (t->memory) vkFreeMemory(vk_state.dev, t->memory, NULL);
    memset(t, 0, sizeof(*t));
}

static VkSamplerAddressMode VK_WrapToAddressMode(int wrapMode) {
    if (wrapMode == GL_REPEAT) {
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

qboolean VK_CreateTextureFromPixels(const uint8_t *pixels, int w, int h, vk_texture_t *out,
                                    int wrapMode, qboolean mipmap) {
    qboolean wasLoaded = out ? out->loaded : qfalse;
    VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkImageViewCreateInfo ivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    VkSamplerCreateInfo sci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    VkMemoryRequirements req;
    VkBuffer stagingBuf = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    void *data;
    VkCommandBuffer cmd;
    VkCommandBufferBeginInfo cbbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    uint32_t mipLevels = 1;
    uint32_t i;
    int32_t mipWidth, mipHeight;

    if (!out || !pixels || w <= 0 || h <= 0) {
        return qfalse;
    }

    if (wasLoaded) {
        VK_DestroyTexture(out);
    }

    out->width = w;
    out->height = h;

    if (mipmap) {
        uint32_t maxDim = (uint32_t)(w > h ? w : h);
        while (maxDim > 1) {
            mipLevels++;
            maxDim /= 2;
        }
    }

    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent.width = w;
    ici.extent.height = h;
    ici.extent.depth = 1;
    ici.mipLevels = mipLevels;
    ici.arrayLayers = 1;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(vk_state.dev, &ici, NULL, &out->image) != VK_SUCCESS)
        return qfalse;

    vkGetImageMemoryRequirements(vk_state.dev, out->image, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = VK_FindMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(vk_state.dev, &ai, NULL, &out->memory) != VK_SUCCESS) {
        vkDestroyImage(vk_state.dev, out->image, NULL);
        out->image = VK_NULL_HANDLE;
        return qfalse;
    }
    vkBindImageMemory(vk_state.dev, out->image, out->memory, 0);

    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size = w * h * 4;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vk_state.dev, &bci, NULL, &stagingBuf) != VK_SUCCESS)
        goto fail;
    vkGetBufferMemoryRequirements(vk_state.dev, stagingBuf, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = VK_FindMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(vk_state.dev, &ai, NULL, &stagingMem) != VK_SUCCESS)
        goto fail;
    vkBindBufferMemory(vk_state.dev, stagingBuf, stagingMem, 0);
    vkMapMemory(vk_state.dev, stagingMem, 0, bci.size, 0, &data);
    memcpy(data, pixels, bci.size);
    vkUnmapMemory(vk_state.dev, stagingMem);

    cmd = vk_state.uploadCmd;
    vkResetCommandBuffer(cmd, 0);
    vkBeginCommandBuffer(cmd, &cbbi);

    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = out->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    VkBufferImageCopy copy = { 0 };
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent.width = w;
    copy.imageExtent.height = h;
    copy.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(cmd, stagingBuf, out->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    mipWidth = w;
    mipHeight = h;
    for (i = 1; i < mipLevels; i++) {
        int32_t nextWidth = mipWidth > 1 ? mipWidth / 2 : 1;
        int32_t nextHeight = mipHeight > 1 ? mipHeight / 2 : 1;
        VkImageBlit blit = { 0 };

        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

        blit.srcOffsets[0].x = 0;
        blit.srcOffsets[0].y = 0;
        blit.srcOffsets[0].z = 0;
        blit.srcOffsets[1].x = mipWidth;
        blit.srcOffsets[1].y = mipHeight;
        blit.srcOffsets[1].z = 1;
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0].x = 0;
        blit.dstOffsets[0].y = 0;
        blit.dstOffsets[0].z = 0;
        blit.dstOffsets[1].x = nextWidth;
        blit.dstOffsets[1].y = nextHeight;
        blit.dstOffsets[1].z = 1;
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.layerCount = 1;
        vkCmdBlitImage(cmd, out->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            out->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

        mipWidth = nextWidth;
        mipHeight = nextHeight;
    }

    if (mipLevels > 1) {
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.subresourceRange.levelCount = 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    } else {
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    }

    vkEndCommandBuffer(cmd);
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(vk_state.gfxQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(vk_state.gfxQueue);

    ivci.image = out->image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = VK_FORMAT_R8G8B8A8_UNORM;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = mipLevels;
    ivci.subresourceRange.layerCount = 1;
    if (vkCreateImageView(vk_state.dev, &ivci, NULL, &out->view) != VK_SUCCESS)
        goto fail;

    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_WrapToAddressMode(wrapMode);
    sci.addressModeV = VK_WrapToAddressMode(wrapMode);
    sci.addressModeW = VK_WrapToAddressMode(wrapMode);
    sci.minLod = 0.0f;
    sci.maxLod = (float)(mipLevels - 1);

    if (vk_state.physFeatures.samplerAnisotropy && r_ext_texture_filter_anisotropic && r_ext_texture_filter_anisotropic->value > 0.0f) {
        float aniso = r_ext_texture_filter_anisotropic->value;
        if (aniso > vk_state.maxSamplerAnisotropy) {
            aniso = vk_state.maxSamplerAnisotropy;
        }
        if (aniso < 1.0f) {
            aniso = 1.0f;
        }
        sci.anisotropyEnable = VK_TRUE;
        sci.maxAnisotropy = aniso;
    }

    if (vkCreateSampler(vk_state.dev, &sci, NULL, &out->sampler) != VK_SUCCESS)
        goto fail;

    vkDestroyBuffer(vk_state.dev, stagingBuf, NULL);
    vkFreeMemory(vk_state.dev, stagingMem, NULL);
    out->loaded = qtrue;
    return qtrue;

fail:
    vkDestroyBuffer(vk_state.dev, stagingBuf, NULL);
    vkFreeMemory(vk_state.dev, stagingMem, NULL);
    VK_DestroyTexture(out);
    return qfalse;
}

static VkFormat VK_KTXFormatToVkFormat(const ktx_texture_t *tex) {
    /*
     * The rest of the engine treats uploaded textures as linear, so use
     * UNORM block formats even when the KTX file is marked sRGB.  This keeps
     * KTX replacements visually consistent with the original JPG/TGA assets.
     */
    (void)tex->srgb;
    switch (tex->format) {
        case KTX_FMT_RGBA8:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case KTX_FMT_BC1:
            return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case KTX_FMT_BC3:
            return VK_FORMAT_BC3_UNORM_BLOCK;
        case KTX_FMT_BC5:
            return VK_FORMAT_BC5_UNORM_BLOCK;
        case KTX_FMT_BC7:
            return VK_FORMAT_BC7_UNORM_BLOCK;
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

qboolean VK_CreateTextureFromKTX(const ktx_texture_t *tex, vk_texture_t *out,
                                 int wrapMode, qboolean mipmap) {
    qboolean wasLoaded = out ? out->loaded : qfalse;
    VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkImageViewCreateInfo ivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    VkSamplerCreateInfo sci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    VkMemoryRequirements req;
    VkBuffer stagingBuf = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    void *data;
    VkCommandBuffer cmd;
    VkCommandBufferBeginInfo cbbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    VkFormat format;
    uint32_t mipLevels;
    uint32_t i;

    (void)mipmap;

    if (!out || !tex || !tex->data || tex->width == 0 || tex->height == 0 || tex->mipLevels == 0) {
        return qfalse;
    }

    format = VK_KTXFormatToVkFormat(tex);
    if (format == VK_FORMAT_UNDEFINED) {
        return qfalse;
    }

    if (wasLoaded) {
        VK_DestroyTexture(out);
    }

    out->width = (int)tex->width;
    out->height = (int)tex->height;
    mipLevels = tex->mipLevels;

    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent.width = tex->width;
    ici.extent.height = tex->height;
    ici.extent.depth = 1;
    ici.mipLevels = mipLevels;
    ici.arrayLayers = 1;
    ici.format = format;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(vk_state.dev, &ici, NULL, &out->image) != VK_SUCCESS) {
        return qfalse;
    }

    vkGetImageMemoryRequirements(vk_state.dev, out->image, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = VK_FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(vk_state.dev, &ai, NULL, &out->memory) != VK_SUCCESS) {
        vkDestroyImage(vk_state.dev, out->image, NULL);
        out->image = VK_NULL_HANDLE;
        return qfalse;
    }
    vkBindImageMemory(vk_state.dev, out->image, out->memory, 0);

    {
        VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bci.size = tex->dataSize;
        bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(vk_state.dev, &bci, NULL, &stagingBuf) != VK_SUCCESS) {
            goto fail;
        }
    }
    vkGetBufferMemoryRequirements(vk_state.dev, stagingBuf, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = VK_FindMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(vk_state.dev, &ai, NULL, &stagingMem) != VK_SUCCESS) {
        goto fail;
    }
    vkBindBufferMemory(vk_state.dev, stagingBuf, stagingMem, 0);
    vkMapMemory(vk_state.dev, stagingMem, 0, tex->dataSize, 0, &data);
    memcpy(data, tex->data, tex->dataSize);
    vkUnmapMemory(vk_state.dev, stagingMem);

    cmd = vk_state.uploadCmd;
    vkResetCommandBuffer(cmd, 0);
    vkBeginCommandBuffer(cmd, &cbbi);

    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = out->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    for (i = 0; i < mipLevels; i++) {
        VkBufferImageCopy copy = { 0 };
        uint32_t mipW = tex->width >> i;
        uint32_t mipH = tex->height >> i;
        if (mipW < 1) mipW = 1;
        if (mipH < 1) mipH = 1;

        copy.bufferOffset = (VkDeviceSize)tex->levelOffsets[i];
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = i;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent.width = mipW;
        copy.imageExtent.height = mipH;
        copy.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(cmd, stagingBuf, out->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }

    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    vkEndCommandBuffer(cmd);
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(vk_state.gfxQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(vk_state.gfxQueue);

    ivci.image = out->image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = format;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = mipLevels;
    ivci.subresourceRange.layerCount = 1;
    if (vkCreateImageView(vk_state.dev, &ivci, NULL, &out->view) != VK_SUCCESS) {
        goto fail;
    }

    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_WrapToAddressMode(wrapMode);
    sci.addressModeV = VK_WrapToAddressMode(wrapMode);
    sci.addressModeW = VK_WrapToAddressMode(wrapMode);
    sci.minLod = 0.0f;
    sci.maxLod = (float)(mipLevels - 1);

    if (vk_state.physFeatures.samplerAnisotropy && r_ext_texture_filter_anisotropic && r_ext_texture_filter_anisotropic->value > 0.0f) {
        float aniso = r_ext_texture_filter_anisotropic->value;
        if (aniso > vk_state.maxSamplerAnisotropy) {
            aniso = vk_state.maxSamplerAnisotropy;
        }
        if (aniso < 1.0f) {
            aniso = 1.0f;
        }
        sci.anisotropyEnable = VK_TRUE;
        sci.maxAnisotropy = aniso;
    }

    if (vkCreateSampler(vk_state.dev, &sci, NULL, &out->sampler) != VK_SUCCESS) {
        goto fail;
    }

    vkDestroyBuffer(vk_state.dev, stagingBuf, NULL);
    vkFreeMemory(vk_state.dev, stagingMem, NULL);
    out->loaded = qtrue;
    return qtrue;

fail:
    vkDestroyBuffer(vk_state.dev, stagingBuf, NULL);
    vkFreeMemory(vk_state.dev, stagingMem, NULL);
    VK_DestroyTexture(out);
    return qfalse;
}

static void VK_WriteDescriptorPair(VkDescriptorSet dstSet, const vk_texture_t *base,
                                   const vk_texture_t *light);

#define VK_MAX_DESC_PAIRS 2048

typedef struct {
    int baseIdx;
    int lightIdx;
    VkDescriptorSet set;
} vk_desc_pair_t;

static vk_desc_pair_t vk_descPairs[VK_MAX_DESC_PAIRS];
static int vk_descPairCount;

static void VK_RefreshDescPairsForVkIdx(int vkIdx) {
    int i;

    for (i = 0; i < vk_descPairCount; i++) {
        vk_texture_t *baseTex;
        vk_texture_t *lightTex;

        if (vk_descPairs[i].baseIdx != vkIdx && vk_descPairs[i].lightIdx != vkIdx) {
            continue;
        }

        baseTex = &vk_state.textures[vk_descPairs[i].baseIdx];
        lightTex = &vk_state.textures[vk_descPairs[i].lightIdx];
        if (!baseTex->loaded || !lightTex->loaded || !vk_descPairs[i].set) {
            continue;
        }

        VK_WriteDescriptorPair(vk_descPairs[i].set, baseTex, lightTex);
    }
}

static void VK_RefreshImageDescriptor(int vkIdx, vk_texture_t *tex) {
    if (vkIdx < 0 || vkIdx >= VK_MAX_TEXTURES || !tex || !vk_state.descriptorSets) {
        return;
    }

    if (vk_state.descriptorSets[vkIdx] != VK_NULL_HANDLE) {
        VK_WriteDescriptorPair(vk_state.descriptorSets[vkIdx], tex, &vk_state.whiteTexture);
    }

    VK_RefreshDescPairsForVkIdx(vkIdx);
}

void VK_OnTextureUploaded(int vkIdx) {
    vk_texture_t *tex;

    if (vkIdx < 0 || vkIdx >= VK_MAX_TEXTURES) {
        return;
    }

    tex = &vk_state.textures[vkIdx];
    if (!tex->loaded || !tex->view) {
        return;
    }

    VK_RefreshImageDescriptor(vkIdx, tex);
}

static qboolean VK_SubImageUpload(vk_texture_t *out, const uint8_t *pixels, int w, int h) {
    VkBuffer stagingBuf = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VkMemoryRequirements req;
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkCommandBuffer cmd;
    VkCommandBufferBeginInfo cbbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    void *data;

    if (!out || !out->image || !pixels || w <= 0 || h <= 0) {
        return qfalse;
    }

    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size = (VkDeviceSize)w * h * 4;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vk_state.dev, &bci, NULL, &stagingBuf) != VK_SUCCESS) {
        return qfalse;
    }

    vkGetBufferMemoryRequirements(vk_state.dev, stagingBuf, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = VK_FindMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(vk_state.dev, &ai, NULL, &stagingMem) != VK_SUCCESS) {
        vkDestroyBuffer(vk_state.dev, stagingBuf, NULL);
        return qfalse;
    }

    vkBindBufferMemory(vk_state.dev, stagingBuf, stagingMem, 0);
    vkMapMemory(vk_state.dev, stagingMem, 0, bci.size, 0, &data);
    memcpy(data, pixels, (size_t)bci.size);
    vkUnmapMemory(vk_state.dev, stagingMem);

    cmd = vk_state.uploadCmd;
    vkResetCommandBuffer(cmd, 0);
    vkBeginCommandBuffer(cmd, &cbbi);

    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = out->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    VkBufferImageCopy copy = { 0 };
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent.width = (uint32_t)w;
    copy.imageExtent.height = (uint32_t)h;
    copy.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(cmd, stagingBuf, out->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    vkEndCommandBuffer(cmd);
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(vk_state.gfxQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(vk_state.gfxQueue);

    vkDestroyBuffer(vk_state.dev, stagingBuf, NULL);
    vkFreeMemory(vk_state.dev, stagingMem, NULL);
    return qtrue;
}

void VK_UploadScratchImage(image_t *image, const byte *data, int cols, int rows, qboolean dirty) {
    int vkIdx;
    vk_texture_t *tex;

    if (!image || !data || cols <= 0 || rows <= 0) {
        return;
    }

    vkIdx = (int)(image->texnum - 1024);
    if (vkIdx < 0 || vkIdx >= VK_MAX_TEXTURES) {
        return;
    }

    tex = &vk_state.textures[vkIdx];

    if (cols != image->width || rows != image->height || !tex->loaded || !tex->view) {
        image->width = image->uploadWidth = cols;
        image->height = image->uploadHeight = rows;
        VK_DestroyTexture(tex);
        if (VK_CreateTextureFromPixels(data, cols, rows, tex, image->wrapClampMode, image->mipmap)) {
            if (vkIdx >= vk_state.textureCount) {
                vk_state.textureCount = vkIdx + 1;
            }
            VK_OnTextureUploaded(vkIdx);
        }
        return;
    }

    if (dirty) {
        VK_SubImageUpload(tex, data, cols, rows);
    }
}

static qboolean VK_CreateWhiteTexture(void) {
    uint8_t white[4] = { 255, 255, 255, 255 };
    return VK_CreateTextureFromPixels(white, 1, 1, &vk_state.whiteTexture, GL_CLAMP, qfalse);
}

#define VK_FOG_S 256
#define VK_FOG_T 32

static qboolean VK_CreateFogTexture(void) {
    byte *data;
    int x, y;
    qboolean ok;

    data = ri.Hunk_AllocateTempMemory(VK_FOG_S * VK_FOG_T * 4);
    if (!data) {
        return qfalse;
    }

    for (x = 0; x < VK_FOG_S; x++) {
        for (y = 0; y < VK_FOG_T; y++) {
            float d = R_FogFactor((x + 0.5f) / (float)VK_FOG_S,
                                  (y + 0.5f) / (float)VK_FOG_T);
            byte b = (byte)(255 * d);
            data[(y * VK_FOG_S + x) * 4 + 0] = 255;
            data[(y * VK_FOG_S + x) * 4 + 1] = 255;
            data[(y * VK_FOG_S + x) * 4 + 2] = 255;
            data[(y * VK_FOG_S + x) * 4 + 3] = b;
        }
    }

    ok = VK_CreateTextureFromPixels(data, VK_FOG_S, VK_FOG_T,
                                    &vk_state.fogTexture, GL_CLAMP, qfalse);
    ri.Hunk_FreeTempMemory(data);
    return ok;
}

void VK_Setup2DPipeline(void) {
    static const int pipelineIds[] = {
        VK_PIPELINE_2D,
        VK_PIPELINE_2D_OPAQUE,
        VK_PIPELINE_2D_ADDITIVE,
        VK_PIPELINE_2D_MODULATE,
        VK_PIPELINE_2D_SRC_ALPHA_ONE,
        VK_PIPELINE_2D_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR
    };
    VkShaderModule vertMod, fragMod;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo gpci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkVertexInputBindingDescription binding;
    VkVertexInputAttributeDescription attrs[3];
    VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    VkPipelineColorBlendAttachmentState cba;
    VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    VkDynamicState dyn[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS
    };
    VkPipelineDynamicStateCreateInfo dynCi = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    VkViewport viewport = { 0 };
    VkRect2D scissor = { 0 };
    int p;

    vertMod = LoadSPIRV("2d_vert.spv");
    fragMod = LoadSPIRV("2d_frag.spv");
    if (!vertMod || !fragMod) {
        ri.Printf(PRINT_WARNING, "VK_Setup2DPipeline: could not load 2D shaders\n");
        return;
    }

    memset(stages, 0, sizeof(stages));
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    binding.binding = 0;
    binding.stride = sizeof(drawVert_t);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(drawVert_t, xyz);
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = offsetof(drawVert_t, st);
    attrs[2].location = 4;
    attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attrs[2].offset = offsetof(drawVert_t, color);

    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = 3;
    vi.pVertexAttributeDescriptions = attrs;

    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia.primitiveRestartEnable = VK_FALSE;

    vp.viewportCount = 1;
    vp.pViewports = &viewport;
    vp.scissorCount = 1;
    vp.pScissors = &scissor;

    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.depthBiasEnable = VK_FALSE;
    rs.lineWidth = 1.0f;

    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    cb.logicOpEnable = VK_FALSE;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    dynCi.dynamicStateCount = 2;
    dynCi.pDynamicStates = dyn;

    memset(&ds, 0, sizeof(ds));
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable = VK_FALSE;

    for (p = 0; p < (int)(sizeof(pipelineIds) / sizeof(pipelineIds[0])); p++) {
        int pipelineIndex = pipelineIds[p];

        if (vk_state.pipelines[pipelineIndex]) {
            vkDestroyPipeline(vk_state.dev, vk_state.pipelines[pipelineIndex], NULL);
            vk_state.pipelines[pipelineIndex] = VK_NULL_HANDLE;
        }

        memset(&cba, 0, sizeof(cba));
        cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        switch (pipelineIndex) {
        case VK_PIPELINE_2D_OPAQUE:
            cba.blendEnable = VK_FALSE;
            break;
        case VK_PIPELINE_2D_ADDITIVE:
            cba.blendEnable = VK_TRUE;
            cba.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            break;
        case VK_PIPELINE_2D_MODULATE:
            cba.blendEnable = VK_TRUE;
            cba.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
            cba.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
            cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            break;
        case VK_PIPELINE_2D_SRC_ALPHA_ONE:
            cba.blendEnable = VK_TRUE;
            cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            break;
        case VK_PIPELINE_2D_ONE_MINUS_DST_COLOR_ONE_MINUS_SRC_COLOR:
            cba.blendEnable = VK_TRUE;
            cba.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            break;
        default:
            cba.blendEnable = VK_TRUE;
            cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;
        }

        memset(&gpci, 0, sizeof(gpci));
        gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpci.stageCount = 2;
        gpci.pStages = stages;
        gpci.pVertexInputState = &vi;
        gpci.pInputAssemblyState = &ia;
        gpci.pViewportState = &vp;
        gpci.pRasterizationState = &rs;
        gpci.pMultisampleState = &ms;
        gpci.pDepthStencilState = &ds;
        gpci.pColorBlendState = &cb;
        gpci.pDynamicState = &dynCi;
        gpci.layout = vk_state.pipelineLayout;
        gpci.renderPass = vk_state.renderPass;
        gpci.subpass = 0;

        if (vkCreateGraphicsPipelines(vk_state.dev, VK_NULL_HANDLE, 1, &gpci, NULL,
            &vk_state.pipelines[pipelineIndex]) != VK_SUCCESS) {
            ri.Printf(PRINT_WARNING, "VK_Setup2DPipeline: failed to create 2D pipeline %d\n", pipelineIndex);
        }
    }

    vkDestroyShaderModule(vk_state.dev, vertMod, NULL);
    vkDestroyShaderModule(vk_state.dev, fragMod, NULL);
}

static void VK_WriteDescriptorPair(VkDescriptorSet dstSet, const vk_texture_t *base,
                                   const vk_texture_t *light) {
    VkDescriptorImageInfo infos[2];
    VkWriteDescriptorSet wr[2];

    if (!dstSet || !base || !light || !base->view || !light->view) {
        return;
    }

    infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    infos[0].imageView = base->view;
    infos[0].sampler = base->sampler;
    infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    infos[1].imageView = light->view;
    infos[1].sampler = light->sampler;

    memset(wr, 0, sizeof(wr));
    wr[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr[0].dstSet = dstSet;
    wr[0].dstBinding = 0;
    wr[0].descriptorCount = 1;
    wr[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wr[0].pImageInfo = &infos[0];
    wr[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr[1].dstSet = dstSet;
    wr[1].dstBinding = 1;
    wr[1].descriptorCount = 1;
    wr[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wr[1].pImageInfo = &infos[1];
    vkUpdateDescriptorSets(vk_state.dev, 2, wr, 0, NULL);
}

void VK_AllocateDescriptorSets(void) {
    VkDescriptorSetAllocateInfo ai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };

    vk_state.descriptorSets = (VkDescriptorSet *)calloc(VK_MAX_TEXTURES, sizeof(VkDescriptorSet));
    if (!vk_state.descriptorSets) {
        ri.Printf(PRINT_WARNING, "VK_AllocateDescriptorSets: out of memory\n");
        return;
    }

    ai.descriptorPool = vk_state.descPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &vk_state.descSetLayout;
    vkAllocateDescriptorSets(vk_state.dev, &ai, &vk_state.whiteDescSet);
    VK_WriteDescriptorPair(vk_state.whiteDescSet, &vk_state.whiteTexture, &vk_state.whiteTexture);

    vkAllocateDescriptorSets(vk_state.dev, &ai, &vk_state.fogDescSet);
    VK_WriteDescriptorPair(vk_state.fogDescSet, &vk_state.fogTexture, &vk_state.whiteTexture);
}

VkDescriptorSet VK_GetDescriptorSetForImage(image_t *image) {
    int vkIdx;
    vk_texture_t *tex;
    VkDescriptorSetAllocateInfo ai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };

    if (!image) {
        return vk_state.whiteDescSet;
    }

    vkIdx = (int)(image->texnum - 1024);
    if (vkIdx < 0 || vkIdx >= VK_MAX_TEXTURES || !vk_state.descriptorSets) {
        return vk_state.whiteDescSet;
    }

    tex = &vk_state.textures[vkIdx];
    if (!tex->loaded || !tex->view) {
        return vk_state.whiteDescSet;
    }

    if (vk_state.descriptorSets[vkIdx] != VK_NULL_HANDLE) {
        return vk_state.descriptorSets[vkIdx];
    }

    ai.descriptorPool = vk_state.descPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &vk_state.descSetLayout;
    if (vkAllocateDescriptorSets(vk_state.dev, &ai, &vk_state.descriptorSets[vkIdx]) != VK_SUCCESS) {
        return vk_state.whiteDescSet;
    }

    VK_WriteDescriptorPair(vk_state.descriptorSets[vkIdx], tex, &vk_state.whiteTexture);
    return vk_state.descriptorSets[vkIdx];
}

static int VK_ImageVkIdx(image_t *image) {
    if (!image) {
        return -1;
    }
    return (int)(image->texnum - 1024);
}

static vk_texture_t *VK_ImageToTexture(image_t *image) {
    int vkIdx;

    if (!image) {
        return &vk_state.whiteTexture;
    }

    vkIdx = VK_ImageVkIdx(image);
    if (vkIdx < 0 || vkIdx >= VK_MAX_TEXTURES) {
        return &vk_state.whiteTexture;
    }

    if (!vk_state.textures[vkIdx].loaded || !vk_state.textures[vkIdx].view) {
        return &vk_state.whiteTexture;
    }

    return &vk_state.textures[vkIdx];
}

VkDescriptorSet VK_GetDescriptorSetForImages(image_t *base, image_t *light) {
    int baseIdx;
    int lightIdx;
    int i;
    vk_texture_t *baseTex;
    vk_texture_t *lightTex;
    VkDescriptorSetAllocateInfo ai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };

    if (!base) {
        base = tr.whiteImage;
    }
    if (!light) {
        light = tr.whiteImage;
    }

    if (light == tr.whiteImage) {
        return VK_GetDescriptorSetForImage(base);
    }

    baseIdx = VK_ImageVkIdx(base);
    lightIdx = VK_ImageVkIdx(light);
    if (baseIdx < 0 || lightIdx < 0) {
        return vk_state.whiteDescSet;
    }

    for (i = 0; i < vk_descPairCount; i++) {
        if (vk_descPairs[i].baseIdx == baseIdx && vk_descPairs[i].lightIdx == lightIdx) {
            return vk_descPairs[i].set;
        }
    }

    if (vk_descPairCount >= VK_MAX_DESC_PAIRS) {
        return VK_GetDescriptorSetForImage(base);
    }

    baseTex = VK_ImageToTexture(base);
    lightTex = VK_ImageToTexture(light);

    ai.descriptorPool = vk_state.descPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &vk_state.descSetLayout;
    if (vkAllocateDescriptorSets(vk_state.dev, &ai, &vk_descPairs[vk_descPairCount].set) != VK_SUCCESS) {
        return VK_GetDescriptorSetForImage(base);
    }

    vk_descPairs[vk_descPairCount].baseIdx = baseIdx;
    vk_descPairs[vk_descPairCount].lightIdx = lightIdx;
    VK_WriteDescriptorPair(vk_descPairs[vk_descPairCount].set, baseTex, lightTex);
    return vk_descPairs[vk_descPairCount++].set;
}

void VK_Init(void) {
    ri.Printf(PRINT_ALL, "VK_Init: Vulkan renderer starting...\n");
}

void VK_DestroyBuffer(vk_buffer_t *b) {
    if (b->buffer) vkDestroyBuffer(vk_state.dev, b->buffer, NULL);
    if (b->memory) vkFreeMemory(vk_state.dev, b->memory, NULL);
    memset(b, 0, sizeof(*b));
}

void VK_Shutdown(void) {
    if (!vk_state.dev) return;

    vkDeviceWaitIdle(vk_state.dev);

    for (int i = 0; i < VK_PIPELINE_COUNT; i++) {
        if (vk_state.pipelines[i])
            vkDestroyPipeline(vk_state.dev, vk_state.pipelines[i], NULL);
    }
    if (vk_state.pipelineLayout)
        vkDestroyPipelineLayout(vk_state.dev, vk_state.pipelineLayout, NULL);
    if (vk_state.descPool)
        vkDestroyDescriptorPool(vk_state.dev, vk_state.descPool, NULL);
    if (vk_state.descSetLayout)
        vkDestroyDescriptorSetLayout(vk_state.dev, vk_state.descSetLayout, NULL);
    if (vk_state.renderPass)
        vkDestroyRenderPass(vk_state.dev, vk_state.renderPass, NULL);

    VK_DestroySwapchain();
    VK_DestroyDynamicVBO();
    VK_DestroyTexture(&vk_state.whiteTexture);
    VK_DestroyTexture(&vk_state.fogTexture);

    if (vk_state.descriptorSets) {
        free(vk_state.descriptorSets);
        vk_state.descriptorSets = NULL;
    }

    if (vk_state.uploadPool) {
        if (vk_state.uploadCmd) {
            vkFreeCommandBuffers(vk_state.dev, vk_state.uploadPool, 1, &vk_state.uploadCmd);
            vk_state.uploadCmd = VK_NULL_HANDLE;
        }
        vkDestroyCommandPool(vk_state.dev, vk_state.uploadPool, NULL);
        vk_state.uploadPool = VK_NULL_HANDLE;
    }

    if (vk_state.cmdPool) {
        if (vk_state.cmdBuffers) {
            vkFreeCommandBuffers(vk_state.dev, vk_state.cmdPool,
                                 vk_state.swapCount, vk_state.cmdBuffers);
            free(vk_state.cmdBuffers);
        }
        vkDestroyCommandPool(vk_state.dev, vk_state.cmdPool, NULL);
    }

    for (int i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++) {
        if (vk_state.imageAvailable[i])
            vkDestroySemaphore(vk_state.dev, vk_state.imageAvailable[i], NULL);
        if (vk_state.renderFinished[i])
            vkDestroySemaphore(vk_state.dev, vk_state.renderFinished[i], NULL);
        if (vk_state.inFlight[i])
            vkDestroyFence(vk_state.dev, vk_state.inFlight[i], NULL);
    }

    if (vk_state.dev) {
        vkDestroyDevice(vk_state.dev, NULL);
        vk_state.dev = VK_NULL_HANDLE;
    }

    memset(&vk_state, 0, sizeof(vk_state));
    vk_active = qfalse;
}

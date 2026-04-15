/*
===========================================================================

Return to Castle Wolfenstein Vulkan Renderer
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Return to Castle Wolfenstein single player GPL Source Code.

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/

// vk_init.c - Vulkan initialization and core functions

#include "vk_local.h"
#include "../renderer/tr_local.h"
#include "../client/client.h"
#include <stdio.h>
#include <string.h>

// Global Vulkan state
vkconfig_t vkConfig;
vkstate_t vkState;
vkFunctions_t vkFunc;

// Cvars
cvar_t *vk_validation;
cvar_t *vk_debug;
cvar_t *vk_skipBackend;
cvar_t *vk_presentMode;

// External SDL window from unix_sdl2.c
extern SDL_Window *window;

// Debug callback
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) 
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        ri.Printf(PRINT_ALL, "Vulkan Validation: %s\n", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

/**
 * Find memory type index for buffer/image allocation
 */
uint32_t VK_FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkFunc.vkGetPhysicalDeviceMemoryProperties(vkState.physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    ri.Error(ERR_FATAL, "Failed to find suitable memory type!");
    return 0;
}

/**
 * Check if physical device is suitable for our needs
 */
qboolean VK_CheckPhysicalDeviceSuitable(VkPhysicalDevice device) {
    // Get queue family properties
    uint32_t queueFamilyCount = 0;
    vkFunc.vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
    
    // Check for graphics and present support
    qboolean graphicsFound = qfalse;
    qboolean presentFound = qfalse;
    
    VkQueueFamilyProperties *queueFamilies = (VkQueueFamilyProperties*)
        malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkFunc.vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);
    
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFound = qtrue;
        }
        
        VkBool32 presentSupport = qfalse;
        vkFunc.vkGetPhysicalDeviceSurfaceSupportKHR(device, i, vkState.surface, &presentSupport);
        if (presentSupport) {
            presentFound = qtrue;
        }
    }
    
    free(queueFamilies);
    
    // Get device properties
    VkPhysicalDeviceProperties deviceProperties;
    vkFunc.vkGetPhysicalDeviceProperties(device, &deviceProperties);
    
    // Store device info
    Q_strncpyz(vkConfig.renderer_string, deviceProperties.deviceName, MAX_STRING_CHARS);
    vkConfig.apiVersion = deviceProperties.apiVersion;
    vkConfig.maxTextureSize = deviceProperties.limits.maxImageDimension2D;
    vkConfig.maxDescriptorSets = deviceProperties.limits.maxDescriptorSetUniformBuffers;
    vkConfig.maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;
    
    return (graphicsFound && presentFound);
}

/**
 * Initialize Vulkan instance
 */
qboolean VK_InitInstance(void) {
    ri.Printf(PRINT_ALL, "Initializing Vulkan instance...\n");
    
    // Application info
    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RTCW Vulkan Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "id Tech 3 Vulkan";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    
    // Get required extensions for SDL2 + Vulkan
    uint32_t extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, NULL);
    const char **extensions = (const char**)malloc(extensionCount * sizeof(char*));
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions);
    
    // Add validation layer extension if enabled
    const char *validationExtensions[] = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
    
    qboolean validationEnabled = vk_validation->integer ? qtrue : qfalse;
    if (validationEnabled) {
        // Merge arrays (simplified - in production code use dynamic array)
        const char **allExtensions = (const char**)malloc((extensionCount + 1) * sizeof(char*));
        memcpy(allExtensions, extensions, extensionCount * sizeof(char*));
        allExtensions[extensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        extensionCount++;
        free(extensions);
        extensions = allExtensions;
    }
    
    // Instance create info
    VkInstanceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;
    
    // Validation layers (optional)
    const char *validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    if (validationEnabled) {
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = validationLayers;
        vkConfig.validationLayersEnabled = qtrue;
    } else {
        createInfo.enabledLayerCount = 0;
        vkConfig.validationLayersEnabled = qfalse;
    }
    
    // Create instance
    VkResult result = vkFunc.vkCreateInstance(&createInfo, NULL, &vkState.instance);
    free(extensions);
    
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to create Vulkan instance (error: %d)\n", result);
        return qfalse;
    }
    
    ri.Printf(PRINT_ALL, "Vulkan instance created successfully\n");
    return qtrue;
}

/**
 * Create Vulkan surface from SDL window
 */
qboolean VKimp_CreateSurface(SDL_Window *sdlWindow) {
    if (!sdlWindow) {
        ri.Printf(PRINT_ALL, "No SDL window available for Vulkan surface\n");
        return qfalse;
    }
    
    ri.Printf(PRINT_ALL, "Creating Vulkan surface...\n");
    
    if (SDL_Vulkan_CreateSurface(sdlWindow, vkState.instance, &vkState.surface) == 0) {
        ri.Printf(PRINT_ALL, "Failed to create Vulkan surface: %s\n", SDL_GetError());
        return qfalse;
    }
    
    ri.Printf(PRINT_ALL, "Vulkan surface created successfully\n");
    return qtrue;
}

/**
 * Initialize Vulkan device
 */
qboolean VK_InitDevice(void) {
    ri.Printf(PRINT_ALL, "Initializing Vulkan device...\n");
    
    // Enumerate physical devices
    uint32_t deviceCount = 0;
    vkFunc.vkEnumeratePhysicalDevices(vkState.instance, &deviceCount, NULL);
    
    if (deviceCount == 0) {
        ri.Printf(PRINT_ALL, "No Vulkan-capable GPUs found\n");
        return qfalse;
    }
    
    VkPhysicalDevice *devices = (VkPhysicalDevice*)malloc(deviceCount * sizeof(VkPhysicalDevice));
    vkFunc.vkEnumeratePhysicalDevices(vkState.instance, &deviceCount, devices);
    
    // Find suitable device
    qboolean found = qfalse;
    for (uint32_t i = 0; i < deviceCount; i++) {
        if (VK_CheckPhysicalDeviceSuitable(devices[i])) {
            vkState.physicalDevice = devices[i];
            found = qtrue;
            break;
        }
    }
    
    free(devices);
    
    if (!found) {
        ri.Printf(PRINT_ALL, "No suitable Vulkan device found\n");
        return qfalse;
    }
    
    ri.Printf(PRINT_ALL, "Selected GPU: %s\n", vkConfig.renderer_string);
    
    // Get queue family indices
    uint32_t queueFamilyCount = 0;
    vkFunc.vkGetPhysicalDeviceQueueFamilyProperties(vkState.physicalDevice, &queueFamilyCount, NULL);
    
    VkQueueFamilyProperties *queueFamilies = (VkQueueFamilyProperties*)
        malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkFunc.vkGetPhysicalDeviceQueueFamilyProperties(vkState.physicalDevice, &queueFamilyCount, queueFamilies);
    
    uint32_t graphicsFamilyIndex = UINT32_MAX;
    uint32_t presentFamilyIndex = UINT32_MAX;
    
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamilyIndex = i;
        }
        
        VkBool32 presentSupport = qfalse;
        vkFunc.vkGetPhysicalDeviceSurfaceSupportKHR(vkState.physicalDevice, i, vkState.surface, &presentSupport);
        if (presentSupport) {
            presentFamilyIndex = i;
        }
    }
    
    free(queueFamilies);
    
    // Create logical device
    float queuePriority = 1.0f;
    
    VkDeviceQueueCreateInfo queueCreateInfos[2] = {0};
    uint32_t queueCreateInfoCount = 0;
    
    // Graphics queue
    if (graphicsFamilyIndex != UINT32_MAX) {
        queueCreateInfos[queueCreateInfoCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[queueCreateInfoCount].queueFamilyIndex = graphicsFamilyIndex;
        queueCreateInfos[queueCreateInfoCount].queueCount = 1;
        queueCreateInfos[queueCreateInfoCount].pQueuePriorities = &queuePriority;
        queueCreateInfoCount++;
    }
    
    // Present queue (if different)
    if (presentFamilyIndex != graphicsFamilyIndex && presentFamilyIndex != UINT32_MAX) {
        queueCreateInfos[queueCreateInfoCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[queueCreateInfoCount].queueFamilyIndex = presentFamilyIndex;
        queueCreateInfos[queueCreateInfoCount].queueCount = 1;
        queueCreateInfos[queueCreateInfoCount].pQueuePriorities = &queuePriority;
        queueCreateInfoCount++;
    }
    
    // Device features (empty for now)
    VkPhysicalDeviceFeatures deviceFeatures = {0};
    
    // Device create info
    VkDeviceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = queueCreateInfoCount;
    createInfo.pQueueCreateInfos = queueCreateInfos;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 0;
    createInfo.ppEnabledExtensionNames = NULL;
    
    if (vkConfig.validationLayersEnabled) {
        createInfo.enabledLayerCount = 1;
        const char *validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
        createInfo.ppEnabledLayerNames = validationLayers;
    } else {
        createInfo.enabledLayerCount = 0;
    }
    
    VkResult result = vkFunc.vkCreateDevice(vkState.physicalDevice, &createInfo, NULL, &vkState.device);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to create Vulkan device (error: %d)\n", result);
        return qfalse;
    }
    
    // Get queues
    vkFunc.vkGetDeviceQueue(vkState.device, graphicsFamilyIndex, 0, &vkState.graphicsQueue);
    
    if (presentFamilyIndex != graphicsFamilyIndex) {
        vkFunc.vkGetDeviceQueue(vkState.device, presentFamilyIndex, 0, &vkState.presentQueue);
    } else {
        vkState.presentQueue = vkState.graphicsQueue;
    }
    
    ri.Printf(PRINT_ALL, "Vulkan device created successfully\n");
    return qtrue;
}

/**
 * Initialize Vulkan swapchain
 */
qboolean VK_InitSwapchain(void) {
    ri.Printf(PRINT_ALL, "Initializing Vulkan swapchain...\n");
    
    // Get surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkFunc.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkState.physicalDevice, vkState.surface, &capabilities);
    
    // Get surface formats
    uint32_t formatCount;
    vkFunc.vkGetPhysicalDeviceSurfaceFormatsKHR(vkState.physicalDevice, vkState.surface, &formatCount, NULL);
    
    if (formatCount == 0) {
        ri.Printf(PRINT_ALL, "No surface formats available\n");
        return qfalse;
    }
    
    VkSurfaceFormatKHR *formats = (VkSurfaceFormatKHR*)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    vkFunc.vkGetPhysicalDeviceSurfaceFormatsKHR(vkState.physicalDevice, vkState.surface, &formatCount, formats);
    
    // Choose swapchain format
    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (uint32_t i = 0; i < formatCount; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM || 
            formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
            chosenFormat = formats[i];
            break;
        }
    }
    free(formats);
    
    vkState.swapchainImageFormat = chosenFormat.format;
    
    // Choose present mode
    uint32_t presentModeCount;
    vkFunc.vkGetPhysicalDeviceSurfacePresentModesKHR(vkState.physicalDevice, vkState.surface, &presentModeCount, NULL);
    
    VkPresentModeKHR *presentModes = (VkPresentModeKHR*)malloc(presentModeCount * sizeof(VkPresentModeKHR));
    vkFunc.vkGetPhysicalDeviceSurfacePresentModesKHR(vkState.physicalDevice, vkState.surface, &presentModeCount, presentModes);
    
    VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR; // VSync
    for (uint32_t i = 0; i < presentModeCount; i++) {
        if (vk_presentMode->integer == 0 && presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            chosenPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;
        }
    }
    free(presentModes);
    
    // Determine swapchain extent
    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        extent.width = w;
        extent.height = h;
    }
    
    // Clamp to min/max
    extent.width = Q_clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = Q_clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    
    vkState.swapchainExtent = extent;
    vkConfig.vidWidth = extent.width;
    vkConfig.vidHeight = extent.height;
    vkConfig.windowAspect = (float)extent.width / (float)extent.height;
    
    // Determine image count
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    
    // Create swapchain
    VkSwapchainCreateInfoKHR createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = vkState.surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = chosenFormat.format;
    createInfo.imageColorSpace = chosenFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = chosenPresentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    
    VkResult result = vkFunc.vkCreateSwapchainKHR(vkState.device, &createInfo, NULL, &vkState.swapchain);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to create Vulkan swapchain (error: %d)\n", result);
        return qfalse;
    }
    
    // Get swapchain images
    vkFunc.vkGetSwapchainImagesKHR(vkState.device, vkState.swapchain, &imageCount, NULL);
    vkState.swapchainImageCount = imageCount;
    
    if (imageCount > MAX_SWAPCHAIN_IMAGES) {
        ri.Printf(PRINT_ALL, "Warning: Swapchain image count exceeds maximum\n");
        imageCount = MAX_SWAPCHAIN_IMAGES;
    }
    
    vkFunc.vkGetSwapchainImagesKHR(vkState.device, vkState.swapchain, &imageCount, vkState.swapchainImages);
    
    // Create image views
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {0};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vkState.swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = vkState.swapchainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        result = vkFunc.vkCreateImageView(vkState.device, &viewInfo, NULL, &vkState.swapchainImageViews[i]);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ALL, "Failed to create swapchain image view\n");
            return qfalse;
        }
    }
    
    ri.Printf(PRINT_ALL, "Vulkan swapchain created: %dx%d\n", extent.width, extent.height);
    return qtrue;
}

/**
 * Initialize command buffers
 */
qboolean VK_InitCommandBuffers(void) {
    ri.Printf(PRINT_ALL, "Initializing Vulkan command buffers...\n");
    
    // Create command pool
    VkCommandPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    // Use graphics queue family (simplified - should get actual index)
    poolInfo.queueFamilyIndex = 0; 
    
    VkResult result = vkFunc.vkCreateCommandPool(vkState.device, &poolInfo, NULL, &vkState.commandPool);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to create command pool\n");
        return qfalse;
    }
    
    // Allocate command buffers
    VkCommandBufferAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vkState.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    
    result = vkFunc.vkAllocateCommandBuffers(vkState.device, &allocInfo, vkState.commandBuffers);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to allocate command buffers\n");
        return qfalse;
    }
    
    ri.Printf(PRINT_ALL, "Vulkan command buffers initialized\n");
    return qtrue;
}

/**
 * Initialize render pass
 */
qboolean VK_InitRenderPass(void) {
    ri.Printf(PRINT_ALL, "Initializing Vulkan render pass...\n");
    
    // Color attachment description
    VkAttachmentDescription colorAttachment = {0};
    colorAttachment.format = vkState.swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    // Color attachment reference
    VkAttachmentReference colorAttachmentRef = {0};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    // Subpass description
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    
    // Subpass dependency
    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    // Render pass create info
    VkRenderPassCreateInfo renderPassInfo = {0};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    VkResult result = vkFunc.vkCreateRenderPass(vkState.device, &renderPassInfo, NULL, &vkState.renderPass);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to create render pass\n");
        return qfalse;
    }
    
    ri.Printf(PRINT_ALL, "Vulkan render pass initialized\n");
    return qtrue;
}

/**
 * Initialize graphics pipeline (placeholder - minimal clear pipeline)
 */
qboolean VK_InitPipeline(void) {
    ri.Printf(PRINT_ALL, "Initializing Vulkan graphics pipeline...\n");
    
    // Pipeline layout (empty for now)
    VkPipelineLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 0;
    layoutInfo.pushConstantRangeCount = 0;
    
    VkResult result = vkFunc.vkCreatePipelineLayout(vkState.device, &layoutInfo, NULL, &vkState.pipelineLayout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to create pipeline layout\n");
        return qfalse;
    }
    
    // Note: Full pipeline creation requires shader modules
    // This is a placeholder - actual implementation will come later
    
    ri.Printf(PRINT_ALL, "Vulkan pipeline layout initialized (shaders pending)\n");
    return qtrue;
}

/**
 * Initialize descriptor pool
 */
qboolean VK_InitDescriptorPool(void) {
    ri.Printf(PRINT_ALL, "Initializing Vulkan descriptor pool...\n");
    
    // Simple pool for uniform buffers and samplers
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_UNIFORM_BUFFERS},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_DESCRIPTOR_SETS}
    };
    
    VkDescriptorPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = MAX_DESCRIPTOR_SETS;
    
    VkResult result = vkFunc.vkCreateDescriptorPool(vkState.device, &poolInfo, NULL, &vkState.descriptorPool);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to create descriptor pool\n");
        return qfalse;
    }
    
    ri.Printf(PRINT_ALL, "Vulkan descriptor pool initialized\n");
    return qtrue;
}

/**
 * Initialize synchronization objects
 */
qboolean VK_InitSyncObjects(void) {
    ri.Printf(PRINT_ALL, "Initializing Vulkan sync objects...\n");
    
    VkSemaphoreCreateInfo semaphoreInfo = {0};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo = {0};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkFunc.vkCreateSemaphore(vkState.device, &semaphoreInfo, NULL, &vkState.imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkFunc.vkCreateSemaphore(vkState.device, &semaphoreInfo, NULL, &vkState.renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkFunc.vkCreateFence(vkState.device, &fenceInfo, NULL, &vkState.fences[i]) != VK_SUCCESS) {
            ri.Printf(PRINT_ALL, "Failed to create sync objects\n");
            return qfalse;
        }
    }
    
    ri.Printf(PRINT_ALL, "Vulkan sync objects initialized\n");
    return qtrue;
}

/**
 * Cleanup swapchain resources
 */
void VK_CleanupSwapchain(void) {
    ri.Printf(PRINT_ALL, "Cleaning up Vulkan swapchain...\n");
    
    for (uint32_t i = 0; i < vkState.swapchainImageCount; i++) {
        if (vkState.swapchainFramebuffers[i] != VK_NULL_HANDLE) {
            vkFunc.vkDestroyFramebuffer(vkState.device, vkState.swapchainFramebuffers[i], NULL);
        }
        if (vkState.swapchainImageViews[i] != VK_NULL_HANDLE) {
            vkFunc.vkDestroyImageView(vkState.device, vkState.swapchainImageViews[i], NULL);
        }
    }
    
    if (vkState.swapchain != VK_NULL_HANDLE) {
        vkFunc.vkDestroySwapchainKHR(vkState.device, vkState.swapchain, NULL);
        vkState.swapchain = VK_NULL_HANDLE;
    }
}

/**
 * Recreate swapchain (on window resize)
 */
void VK_RecreateSwapchain(void) {
    ri.Printf(PRINT_ALL, "Recreating Vulkan swapchain...\n");
    
    vkState.recreatingSwapchain = qtrue;
    
    // Wait for device to be idle
    vkFunc.vkDeviceWaitIdle(vkState.device);
    
    // Cleanup old swapchain
    VK_CleanupSwapchain();
    
    // Recreate
    if (!VK_InitSwapchain()) {
        ri.Error(ERR_FATAL, "Failed to recreate swapchain");
    }
    
    vkState.recreatingSwapchain = qfalse;
}

/**
 * Main Vulkan initialization function
 */
qboolean VKimp_Init(void) {
    ri.Printf(PRINT_ALL, "===== Vulkan Renderer Initialization =====\n");
    ri.Printf(PRINT_ALL, "Version: %s\n", VK_RENDERER_VERSION);
    
    // Register cvars
    vk_validation = ri.Cvar_Get("vk_validation", "0", CVAR_LATCH | CVAR_ARCHIVE);
    vk_debug = ri.Cvar_Get("vk_debug", "0", CVAR_ARCHIVE);
    vk_skipBackend = ri.Cvar_Get("vk_skipBackend", "0", CVAR_CHEAT);
    vk_presentMode = ri.Cvar_Get("vk_presentMode", "0", CVAR_ARCHIVE); // 0 = immediate, 1 = vsync
    
    // Initialize config
    memset(&vkConfig, 0, sizeof(vkConfig));
    memset(&vkState, 0, sizeof(vkState));
    
    // Load Vulkan functions (using system loader)
    // In production, use volk or manual loading
    vkFunc.vkCreateInstance = vkCreateInstance;
    vkFunc.vkDestroyInstance = vkDestroyInstance;
    vkFunc.vkEnumeratePhysicalDevices = vkEnumeratePhysicalDevices;
    vkFunc.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vkFunc.vkGetPhysicalDeviceQueueFamilyProperties = vkGetPhysicalDeviceQueueFamilyProperties;
    vkFunc.vkGetPhysicalDeviceSurfaceSupportKHR = vkGetPhysicalDeviceSurfaceSupportKHR;
    vkFunc.vkGetPhysicalDeviceSurfaceCapabilitiesKHR = vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    vkFunc.vkGetPhysicalDeviceSurfaceFormatsKHR = vkGetPhysicalDeviceSurfaceFormatsKHR;
    vkFunc.vkGetPhysicalDeviceSurfacePresentModesKHR = vkGetPhysicalDeviceSurfacePresentModesKHR;
    vkFunc.vkCreateDevice = vkCreateDevice;
    vkFunc.vkDestroyDevice = vkDestroyDevice;
    vkFunc.vkGetDeviceQueue = vkGetDeviceQueue;
    vkFunc.vkQueueSubmit = vkQueueSubmit;
    vkFunc.vkQueueWaitIdle = vkQueueWaitIdle;
    vkFunc.vkCreateSwapchainKHR = vkCreateSwapchainKHR;
    vkFunc.vkDestroySwapchainKHR = vkDestroySwapchainKHR;
    vkFunc.vkGetSwapchainImagesKHR = vkGetSwapchainImagesKHR;
    vkFunc.vkAcquireNextImageKHR = vkAcquireNextImageKHR;
    vkFunc.vkQueuePresentKHR = vkQueuePresentKHR;
    vkFunc.vkCreateCommandPool = vkCreateCommandPool;
    vkFunc.vkDestroyCommandPool = vkDestroyCommandPool;
    vkFunc.vkAllocateCommandBuffers = vkAllocateCommandBuffers;
    vkFunc.vkFreeCommandBuffers = vkFreeCommandBuffers;
    vkFunc.vkBeginCommandBuffer = vkBeginCommandBuffer;
    vkFunc.vkEndCommandBuffer = vkEndCommandBuffer;
    vkFunc.vkResetCommandBuffer = vkResetCommandBuffer;
    vkFunc.vkCreateBuffer = vkCreateBuffer;
    vkFunc.vkDestroyBuffer = vkDestroyBuffer;
    vkFunc.vkAllocateMemory = vkAllocateMemory;
    vkFunc.vkFreeMemory = vkFreeMemory;
    vkFunc.vkBindBufferMemory = vkBindBufferMemory;
    vkFunc.vkMapMemory = vkMapMemory;
    vkFunc.vkUnmapMemory = vkUnmapMemory;
    vkFunc.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vkFunc.vkCreateImage = vkCreateImage;
    vkFunc.vkDestroyImage = vkDestroyImage;
    vkFunc.vkBindImageMemory = vkBindImageMemory;
    vkFunc.vkGetImageSubresourceLayout = vkGetImageSubresourceLayout;
    vkFunc.vkCreateImageView = vkCreateImageView;
    vkFunc.vkDestroyImageView = vkDestroyImageView;
    vkFunc.vkCreateFramebuffer = vkCreateFramebuffer;
    vkFunc.vkDestroyFramebuffer = vkDestroyFramebuffer;
    vkFunc.vkCreateRenderPass = vkCreateRenderPass;
    vkFunc.vkDestroyRenderPass = vkDestroyRenderPass;
    vkFunc.vkCreatePipelineLayout = vkCreatePipelineLayout;
    vkFunc.vkDestroyPipelineLayout = vkDestroyPipelineLayout;
    vkFunc.vkCreateGraphicsPipelines = vkCreateGraphicsPipelines;
    vkFunc.vkDestroyPipeline = vkDestroyPipeline;
    vkFunc.vkCreateDescriptorPool = vkCreateDescriptorPool;
    vkFunc.vkDestroyDescriptorPool = vkDestroyDescriptorPool;
    vkFunc.vkCreateDescriptorSetLayout = vkCreateDescriptorSetLayout;
    vkFunc.vkDestroyDescriptorSetLayout = vkDestroyDescriptorSetLayout;
    vkFunc.vkAllocateDescriptorSets = vkAllocateDescriptorSets;
    vkFunc.vkUpdateDescriptorSets = vkUpdateDescriptorSets;
    vkFunc.vkCreateSemaphore = vkCreateSemaphore;
    vkFunc.vkDestroySemaphore = vkDestroySemaphore;
    vkFunc.vkCreateFence = vkCreateFence;
    vkFunc.vkDestroyFence = vkDestroyFence;
    vkFunc.vkWaitForFences = vkWaitForFences;
    vkFunc.vkResetFences = vkResetFences;
    vkFunc.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vkFunc.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vkFunc.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    
    // Step 1: Create instance
    if (!VK_InitInstance()) {
        ri.Printf(PRINT_ALL, "Failed to initialize Vulkan instance\n");
        return qfalse;
    }
    
    // Step 2: Create surface
    if (!VKimp_CreateSurface(window)) {
        ri.Printf(PRINT_ALL, "Failed to create Vulkan surface\n");
        return qfalse;
    }
    
    // Step 3: Create device
    if (!VK_InitDevice()) {
        ri.Printf(PRINT_ALL, "Failed to initialize Vulkan device\n");
        return qfalse;
    }
    
    // Step 4: Create swapchain
    if (!VK_InitSwapchain()) {
        ri.Printf(PRINT_ALL, "Failed to initialize Vulkan swapchain\n");
        return qfalse;
    }
    
    // Step 5: Initialize command buffers
    if (!VK_InitCommandBuffers()) {
        ri.Printf(PRINT_ALL, "Failed to initialize command buffers\n");
        return qfalse;
    }
    
    // Step 6: Initialize render pass
    if (!VK_InitRenderPass()) {
        ri.Printf(PRINT_ALL, "Failed to initialize render pass\n");
        return qfalse;
    }
    
    // Step 7: Initialize pipeline
    if (!VK_InitPipeline()) {
        ri.Printf(PRINT_ALL, "Failed to initialize pipeline\n");
        return qfalse;
    }
    
    // Step 8: Initialize descriptor pool
    if (!VK_InitDescriptorPool()) {
        ri.Printf(PRINT_ALL, "Failed to initialize descriptor pool\n");
        return qfalse;
    }
    
    // Step 9: Initialize sync objects
    if (!VK_InitSyncObjects()) {
        ri.Printf(PRINT_ALL, "Failed to initialize sync objects\n");
        return qfalse;
    }
    
    // Copy config to glConfig for compatibility
    glConfig.colorBits = 32;
    glConfig.depthBits = 24;
    glConfig.stencilBits = 8;
    glConfig.vidWidth = vkConfig.vidWidth;
    glConfig.vidHeight = vkConfig.vidHeight;
    glConfig.windowAspect = vkConfig.windowAspect;
    Q_strncpyz(glConfig.renderer_string, vkConfig.renderer_string, MAX_STRING_CHARS);
    
    ri.Printf(PRINT_ALL, "===== Vulkan Renderer Initialized Successfully =====\n");
    return qtrue;
}

/**
 * Vulkan shutdown
 */
void VKimp_Shutdown(qboolean destroyWindow) {
    ri.Printf(PRINT_ALL, "Shutting down Vulkan renderer...\n");
    
    // Wait for device to be idle
    if (vkState.device != VK_NULL_HANDLE) {
        vkFunc.vkDeviceWaitIdle(vkState.device);
    }
    
    // Cleanup sync objects
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkState.imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkFunc.vkDestroySemaphore(vkState.device, vkState.imageAvailableSemaphores[i], NULL);
        }
        if (vkState.renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkFunc.vkDestroySemaphore(vkState.device, vkState.renderFinishedSemaphores[i], NULL);
        }
        if (vkState.fences[i] != VK_NULL_HANDLE) {
            vkFunc.vkDestroyFence(vkState.device, vkState.fences[i], NULL);
        }
    }
    
    // Cleanup swapchain
    VK_CleanupSwapchain();
    
    // Cleanup other resources
    if (vkState.descriptorPool != VK_NULL_HANDLE) {
        vkFunc.vkDestroyDescriptorPool(vkState.device, vkState.descriptorPool, NULL);
    }
    if (vkState.pipelineLayout != VK_NULL_HANDLE) {
        vkFunc.vkDestroyPipelineLayout(vkState.device, vkState.pipelineLayout, NULL);
    }
    if (vkState.renderPass != VK_NULL_HANDLE) {
        vkFunc.vkDestroyRenderPass(vkState.device, vkState.renderPass, NULL);
    }
    if (vkState.commandPool != VK_NULL_HANDLE) {
        vkFunc.vkDestroyCommandPool(vkState.device, vkState.commandPool, NULL);
    }
    
    // Destroy device
    if (vkState.device != VK_NULL_HANDLE) {
        vkFunc.vkDestroyDevice(vkState.device, NULL);
        vkState.device = VK_NULL_HANDLE;
    }
    
    // Destroy surface
    if (vkState.surface != VK_NULL_HANDLE) {
        vkFunc.vkDestroySurfaceKHR(vkState.instance, vkState.surface, NULL);
        vkState.surface = VK_NULL_HANDLE;
    }
    
    // Destroy instance
    if (vkState.instance != VK_NULL_HANDLE) {
        vkFunc.vkDestroyInstance(vkState.instance, NULL);
        vkState.instance = VK_NULL_HANDLE;
    }
    
    memset(&vkConfig, 0, sizeof(vkConfig));
    memset(&vkState, 0, sizeof(vkState));
    
    ri.Printf(PRINT_ALL, "Vulkan renderer shutdown complete\n");
}

/**
 * Begin frame - acquire swapchain image
 */
void VK_BeginFrame(stereoFrame_t stereoFrame) {
    if (vk_skipBackend->integer) {
        return;
    }
    
    // Wait for previous frame
    vkFunc.vkWaitForFences(vkState.device, 1, &vkState.fences[vkState.currentFrame], VK_TRUE, UINT64_MAX);
    vkFunc.vkResetFences(vkState.device, 1, &vkState.fences[vkState.currentFrame]);
    
    // Acquire next image
    VkResult result = vkFunc.vkAcquireNextImageKHR(
        vkState.device, 
        vkState.swapchain, 
        UINT64_MAX,
        vkState.imageAvailableSemaphores[vkState.currentFrame],
        VK_NULL_HANDLE,
        &vkState.imageIndex
    );
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        VK_RecreateSwapchain();
        return;
    } else if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to acquire swapchain image\n");
        return;
    }
    
    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    if (vkFunc.vkBeginCommandBuffer(vkState.commandBuffers[vkState.currentFrame], &beginInfo) != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to begin command buffer\n");
        return;
    }
    
    // Clear color
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    
    VkRenderPassBeginInfo renderPassInfo = {0};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = vkState.renderPass;
    // Framebuffer would be set here once we create them
    renderPassInfo.framebuffer = VK_NULL_HANDLE; // Placeholder
    renderPassInfo.renderArea.offset = (VkOffset2D){0, 0};
    renderPassInfo.renderArea.extent = vkState.swapchainExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    
    // Note: Actual rendering commands will be added in future iterations
    // vkCmdBeginRenderPass(...);
    // ... drawing commands ...
    // vkCmdEndRenderPass(...);
    
    ri.Printf(PRINT_ALL, "VK_BeginFrame called (placeholder)\n");
}

/**
 * End frame - submit and present
 */
void VK_EndFrame(int *frontEndMsec, int *backEndMsec) {
    if (vk_skipBackend->integer) {
        return;
    }
    
    // End command buffer
    if (vkFunc.vkEndCommandBuffer(vkState.commandBuffers[vkState.currentFrame]) != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to end command buffer\n");
        return;
    }
    
    // Submit command buffer
    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {vkState.imageAvailableSemaphores[vkState.currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vkState.commandBuffers[vkState.currentFrame];
    
    VkSemaphore signalSemaphores[] = {vkState.renderFinishedSemaphores[vkState.currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    if (vkFunc.vkQueueSubmit(vkState.graphicsQueue, 1, &submitInfo, vkState.fences[vkState.currentFrame]) != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to submit command buffer\n");
        return;
    }
    
    // Present
    VkPresentInfoKHR presentInfo = {0};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapchains[] = {vkState.swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &vkState.imageIndex;
    
    VkResult result = vkFunc.vkQueuePresentKHR(vkState.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        VK_RecreateSwapchain();
    } else if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Failed to present\n");
    }
    
    // Move to next frame
    vkState.currentFrame = (vkState.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    
    ri.Printf(PRINT_ALL, "VK_EndFrame called (placeholder)\n");
}

/**
 * Draw view - placeholder
 */
void VK_DrawView(void) {
    // This will be implemented in subsequent iterations
    // For now, just clear the screen
}

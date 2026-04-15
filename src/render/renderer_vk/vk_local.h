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

#ifndef __VK_LOCAL_H
#define __VK_LOCAL_H

#include "../cgame/tr_types.h"
#include "../qcommon/q_shared.h"
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>

// Vulkan renderer version
#define VK_RENDERER_VERSION "0.1.0"

// Maximum resources
#define MAX_FRAMES_IN_FLIGHT      2
#define MAX_SWAPCHAIN_IMAGES      4
#define MAX_DESCRIPTOR_SETS       1024
#define MAX_UNIFORM_BUFFERS       64

// Vulkan function pointers (loaded dynamically)
typedef struct {
    // Instance functions
    PFN_vkCreateInstance vkCreateInstance;
    PFN_vkDestroyInstance vkDestroyInstance;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
    
    // Device functions
    PFN_vkCreateDevice vkCreateDevice;
    PFN_vkDestroyDevice vkDestroyDevice;
    PFN_vkGetDeviceQueue vkGetDeviceQueue;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkQueueWaitIdle vkQueueWaitIdle;
    
    // Swapchain functions
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    PFN_vkQueuePresentKHR vkQueuePresentKHR;
    
    // Command buffer functions
    PFN_vkCreateCommandPool vkCreateCommandPool;
    PFN_vkDestroyCommandPool vkDestroyCommandPool;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    PFN_vkEndCommandBuffer vkEndCommandBuffer;
    PFN_vkResetCommandBuffer vkResetCommandBuffer;
    
    // Buffer functions
    PFN_vkCreateBuffer vkCreateBuffer;
    PFN_vkDestroyBuffer vkDestroyBuffer;
    PFN_vkAllocateMemory vkAllocateMemory;
    PFN_vkFreeMemory vkFreeMemory;
    PFN_vkBindBufferMemory vkBindBufferMemory;
    PFN_vkMapMemory vkMapMemory;
    PFN_vkUnmapMemory vkUnmapMemory;
    PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
    
    // Image functions
    PFN_vkCreateImage vkCreateImage;
    PFN_vkDestroyImage vkDestroyImage;
    PFN_vkBindImageMemory vkBindImageMemory;
    PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
    
    // View and framebuffer functions
    PFN_vkCreateImageView vkCreateImageView;
    PFN_vkDestroyImageView vkDestroyImageView;
    PFN_vkCreateFramebuffer vkCreateFramebuffer;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
    
    // Render pass functions
    PFN_vkCreateRenderPass vkCreateRenderPass;
    PFN_vkDestroyRenderPass vkDestroyRenderPass;
    
    // Pipeline functions
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
    PFN_vkDestroyPipeline vkDestroyPipeline;
    
    // Descriptor set functions
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
    
    // Synchronization functions
    PFN_vkCreateSemaphore vkCreateSemaphore;
    PFN_vkDestroySemaphore vkDestroySemaphore;
    PFN_vkCreateFence vkCreateFence;
    PFN_vkDestroyFence vkDestroyFence;
    PFN_vkWaitForFences vkWaitForFences;
    PFN_vkResetFences vkResetFences;
    
    // Memory functions
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    
    // Debug utils (optional)
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
} vkFunctions_t;

// Vulkan configuration (similar to glconfig_t)
typedef struct {
    char renderer_string[MAX_STRING_CHARS];
    char vendor_string[MAX_STRING_CHARS];
    char driver_version_string[MAX_STRING_CHARS];
    
    int maxTextureSize;
    int maxDescriptorSets;
    int maxUniformBufferRange;
    
    int colorBits, depthBits, stencilBits;
    
    qboolean deviceSupportsHDR;
    qboolean supportsSamplerAnisotropy;
    float maxAnisotropy;
    
    int vidWidth, vidHeight;
    float windowAspect;
    
    int displayFrequency;
    qboolean isFullscreen;
    qboolean smpActive;
    
    // Vulkan-specific
    uint32_t apiVersion;
    uint32_t driverVersion;
    qboolean validationLayersEnabled;
} vkconfig_t;

// Vulkan state (similar to glstate_t)
typedef struct {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    
    VkImage swapchainImages[MAX_SWAPCHAIN_IMAGES];
    VkImageView swapchainImageViews[MAX_SWAPCHAIN_IMAGES];
    VkFramebuffer swapchainFramebuffers[MAX_SWAPCHAIN_IMAGES];
    uint32_t swapchainImageCount;
    
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];
    
    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    Vk fences[MAX_FRAMES_IN_FLIGHT];
    
    VkDescriptorPool descriptorPool;
    
    uint32_t currentFrame;
    uint32_t imageIndex;
    qboolean recreatingSwapchain;
} vkstate_t;

// Global Vulkan state
extern vkconfig_t vkConfig;
extern vkstate_t vkState;
extern vkFunctions_t vkFunc;

// Cvar declarations
extern cvar_t *vk_validation;
extern cvar_t *vk_debug;
extern cvar_t *vk_skipBackend;
extern cvar_t *vk_presentMode;

// Core functions
qboolean VKimp_Init(void);
void VKimp_Shutdown(qboolean destroyWindow);
qboolean VKimp_CreateSurface(SDL_Window *window);
void VKimp_SwapBuffers(void);

// Initialization functions
qboolean VK_InitInstance(void);
qboolean VK_InitDevice(void);
qboolean VK_InitSwapchain(void);
qboolean VK_InitCommandBuffers(void);
qboolean VK_InitRenderPass(void);
qboolean VK_InitPipeline(void);
qboolean VK_InitDescriptorPool(void);
qboolean VK_InitSyncObjects(void);

// Swapchain management
void VK_RecreateSwapchain(void);
void VK_CleanupSwapchain(void);

// Rendering functions
void VK_BeginFrame(stereoFrame_t stereoFrame);
void VK_EndFrame(int *frontEndMsec, int *backEndMsec);
void VK_DrawView(void);

// Resource management
VkBool32 VK_DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                          VkDebugUtilsMessageTypeFlagsEXT messageType,
                          const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                          void *pUserData);

// Helper functions
uint32_t VK_FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
qboolean VK_CheckPhysicalDeviceSuitable(VkPhysicalDevice device);

#endif // __VK_LOCAL_H

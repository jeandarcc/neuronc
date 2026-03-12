#ifndef NPP_RUNTIME_GRAPHICS_VK_INTERNAL_H
#define NPP_RUNTIME_GRAPHICS_VK_INTERNAL_H

#include "graphics/backend/graphics_backend_internal.h"
#include "graphics/graphics_core_internal.h"
#include "gpu_internal.h"
#include "vulkan_common.h"

#ifndef NPP_ENABLE_VULKAN_BACKEND
#define NPP_ENABLE_VULKAN_BACKEND 0
#endif

#if defined(_WIN32) && NPP_VK_COMMON_HAS_HEADERS && NPP_ENABLE_VULKAN_BACKEND
#define NPP_GRAPHICS_VK_ENABLED 1
#else
#define NPP_GRAPHICS_VK_ENABLED 0
#endif

#if NPP_GRAPHICS_VK_ENABLED

#define NEURON_GRAPHICS_FRAMES_IN_FLIGHT 2

typedef struct {
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;

  PFN_vkDestroyInstance vkDestroyInstance;
  PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
  PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
  PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties
      vkGetPhysicalDeviceQueueFamilyProperties;
  PFN_vkGetPhysicalDeviceMemoryProperties
      vkGetPhysicalDeviceMemoryProperties;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
  PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
  PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
      vkGetPhysicalDeviceSurfacePresentModesKHR;
  PFN_vkCreateDevice vkCreateDevice;

  PFN_vkDestroyDevice vkDestroyDevice;
  PFN_vkGetDeviceQueue vkGetDeviceQueue;
  PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
  PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
  PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
  PFN_vkCreateImageView vkCreateImageView;
  PFN_vkDestroyImageView vkDestroyImageView;
  PFN_vkCreateImage vkCreateImage;
  PFN_vkDestroyImage vkDestroyImage;
  PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
  PFN_vkBindImageMemory vkBindImageMemory;
  PFN_vkCreateRenderPass vkCreateRenderPass;
  PFN_vkDestroyRenderPass vkDestroyRenderPass;
  PFN_vkCreateFramebuffer vkCreateFramebuffer;
  PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
  PFN_vkCreateCommandPool vkCreateCommandPool;
  PFN_vkDestroyCommandPool vkDestroyCommandPool;
  PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
  PFN_vkResetCommandBuffer vkResetCommandBuffer;
  PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
  PFN_vkEndCommandBuffer vkEndCommandBuffer;
  PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
  PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
  PFN_vkQueueSubmit vkQueueSubmit;
  PFN_vkQueuePresentKHR vkQueuePresentKHR;
  PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
  PFN_vkCreateSemaphore vkCreateSemaphore;
  PFN_vkDestroySemaphore vkDestroySemaphore;
  PFN_vkCreateFence vkCreateFence;
  PFN_vkDestroyFence vkDestroyFence;
  PFN_vkWaitForFences vkWaitForFences;
  PFN_vkResetFences vkResetFences;
  PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
  PFN_vkCreateShaderModule vkCreateShaderModule;
  PFN_vkDestroyShaderModule vkDestroyShaderModule;
  PFN_vkCreateBuffer vkCreateBuffer;
  PFN_vkDestroyBuffer vkDestroyBuffer;
  PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
  PFN_vkAllocateMemory vkAllocateMemory;
  PFN_vkFreeMemory vkFreeMemory;
  PFN_vkBindBufferMemory vkBindBufferMemory;
  PFN_vkMapMemory vkMapMemory;
  PFN_vkUnmapMemory vkUnmapMemory;
  PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
  PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
  PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
  PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
  PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
  PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
  PFN_vkResetDescriptorPool vkResetDescriptorPool;
  PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
  PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
  PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
  PFN_vkDestroyPipeline vkDestroyPipeline;
  PFN_vkCmdBindPipeline vkCmdBindPipeline;
  PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
  PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
  PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
  PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
  PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
  PFN_vkCmdDraw vkCmdDraw;
  PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
  PFN_vkCmdSetViewport vkCmdSetViewport;
  PFN_vkCmdSetScissor vkCmdSetScissor;
  PFN_vkCreateSampler vkCreateSampler;
  PFN_vkDestroySampler vkDestroySampler;
} NeuronVulkanDispatch;

typedef struct {
  VkBuffer buffer;
  VkDeviceMemory memory;
} NeuronGraphicsVkTransientBuffer;

typedef struct {
  NeuronGraphicsVkTransientBuffer *buffers;
  uint32_t count;
  uint32_t capacity;
} NeuronGraphicsVkFrameResources;

typedef struct {
  const NeuronGraphicsShaderDescriptor *shader_descriptor;
  VkDescriptorSetLayout descriptor_set_layout;
  VkPipelineLayout pipeline_layout;
  VkPipeline graphics_pipeline;
} NeuronGraphicsVkPipelineEntry;

struct NeuronGraphicsBackend {
  NeuronGraphicsWindow *window;
  NeuronVulkanDispatch vk;

  VkInstance instance;
  VkSurfaceKHR surface;
  VkPhysicalDevice physical_device;
  VkPhysicalDeviceMemoryProperties memory_properties;
  VkDevice device;
  uint32_t graphics_queue_family;
  uint32_t present_queue_family;
  VkQueue graphics_queue;
  VkQueue present_queue;

  VkSwapchainKHR swapchain;
  VkFormat swapchain_format;
  VkExtent2D swapchain_extent;
  uint32_t swapchain_image_count;
  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;
  VkFramebuffer *framebuffers;
  VkRenderPass render_pass;
  VkDescriptorPool descriptor_pools[NEURON_GRAPHICS_FRAMES_IN_FLIGHT];
  NeuronGraphicsVkPipelineEntry *pipelines;
  uint32_t pipeline_count;
  uint32_t pipeline_capacity;

  VkCommandPool command_pool;
  VkCommandBuffer command_buffers[NEURON_GRAPHICS_FRAMES_IN_FLIGHT];

  VkSemaphore image_available[NEURON_GRAPHICS_FRAMES_IN_FLIGHT];
  VkSemaphore render_finished[NEURON_GRAPHICS_FRAMES_IN_FLIGHT];
  VkFence in_flight[NEURON_GRAPHICS_FRAMES_IN_FLIGHT];
  NeuronGraphicsVkFrameResources
      frame_resources[NEURON_GRAPHICS_FRAMES_IN_FLIGHT];

  uint32_t frame_index;
  uint32_t acquired_image_index;
  uint32_t submitted_frame_index;
  int frame_submitted;
  int resize_pending;
  int common_context_acquired;
  VkBuffer tensor_interop_buffer;
  VkDeviceSize tensor_interop_size;
  uint32_t tensor_interop_source_queue_family;
  int tensor_interop_requires_ownership_transfer;
  int tensor_interop_ready;
};

int neuron_vk_load_global(NeuronVulkanDispatch *vk,
                          const NeuronVulkanCommonContext *shared);
int neuron_vk_load_instance(NeuronVulkanDispatch *vk, VkInstance instance);
int neuron_vk_load_device(NeuronVulkanDispatch *vk, VkDevice device);

void neuron_graphics_backend_destroy_swapchain(NeuronGraphicsBackend *backend);
int neuron_graphics_backend_create_swapchain(NeuronGraphicsBackend *backend);
int neuron_graphics_backend_recreate_swapchain(NeuronGraphicsBackend *backend);
int neuron_graphics_backend_create_pipeline(NeuronGraphicsBackend *backend);
NeuronGraphicsVkPipelineEntry *neuron_graphics_backend_get_pipeline(
    NeuronGraphicsBackend *backend,
    const NeuronGraphicsShaderDescriptor *shader_descriptor);
void neuron_graphics_backend_reset_frame_resources(NeuronGraphicsBackend *backend,
                                                   uint32_t frame);

#endif

#endif

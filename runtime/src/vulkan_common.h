#ifndef Neuron_RUNTIME_VULKAN_COMMON_H
#define Neuron_RUNTIME_VULKAN_COMMON_H

#include "neuron_gpu.h"

#include <stddef.h>
#include <stdint.h>

#if defined(__has_include)
#if __has_include(<vulkan/vulkan.h>)
#define Neuron_VK_COMMON_HAS_HEADERS 1
#if defined(_WIN32) && !defined(VK_USE_PLATFORM_WIN32_KHR)
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif
#include <vulkan/vulkan.h>
#else
#define Neuron_VK_COMMON_HAS_HEADERS 0
#endif
#else
#define Neuron_VK_COMMON_HAS_HEADERS 0
#endif

#if !Neuron_VK_COMMON_HAS_HEADERS
typedef void *VkInstance;
typedef void *VkPhysicalDevice;
typedef void *VkDevice;
typedef void *VkQueue;
typedef void *VkSurfaceKHR;
typedef struct VkPhysicalDeviceMemoryProperties {
  uint32_t memoryTypeCount;
} VkPhysicalDeviceMemoryProperties;
typedef void *PFN_vkGetInstanceProcAddr;
typedef void *PFN_vkGetDeviceProcAddr;
#ifndef VK_MAX_PHYSICAL_DEVICE_NAME_SIZE
#define VK_MAX_PHYSICAL_DEVICE_NAME_SIZE 256
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue compute_queue;
  VkQueue present_queue;
  uint32_t graphics_queue_family;
  uint32_t compute_queue_family;
  uint32_t present_queue_family;
  int has_graphics_queue;
  int has_present_queue;
  int has_swapchain_extension;
  VkPhysicalDeviceMemoryProperties memory_properties;
  NeuronGpuDeviceClass selected_device_class;
  char selected_device_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
} NeuronVulkanCommonContext;

int neuron_vk_common_acquire(NeuronGpuScopeMode scope_mode,
                             NeuronGpuDeviceClass preferred_device_class,
                             int require_graphics_queue,
                             NeuronVulkanCommonContext *out_context,
                             char *error_buffer, size_t error_size);
void neuron_vk_common_release(void);
int neuron_vk_common_pick_present_queue(VkSurfaceKHR surface,
                                        uint32_t *out_queue_family,
                                        VkQueue *out_queue,
                                        char *error_buffer,
                                        size_t error_size);

#ifdef __cplusplus
}
#endif

#endif // Neuron_RUNTIME_VULKAN_COMMON_H

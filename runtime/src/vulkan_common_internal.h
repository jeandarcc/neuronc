#ifndef Neuron_RUNTIME_VULKAN_COMMON_INTERNAL_H
#define Neuron_RUNTIME_VULKAN_COMMON_INTERNAL_H

#include "vulkan_common.h"
#include "neuron_platform.h"

#if Neuron_VK_COMMON_HAS_HEADERS

typedef struct {
  NeuronPlatformLibraryHandle loader;
  int initialized;
  int available;
  int ref_count;
  char last_error[512];

  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
  PFN_vkCreateInstance vkCreateInstance;
  PFN_vkDestroyInstance vkDestroyInstance;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties
      vkGetPhysicalDeviceQueueFamilyProperties;
  PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
  PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
  PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
  PFN_vkCreateDevice vkCreateDevice;
  PFN_vkDestroyDevice vkDestroyDevice;
  PFN_vkGetDeviceQueue vkGetDeviceQueue;

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
} VulkanCommonState;

extern VulkanCommonState g_common;

void set_errorf(const char *fmt, ...);
void clear_error(void);
void copy_error_out(char *error_buffer, size_t error_size);
void reset_state_values(void);
void fill_context(NeuronVulkanCommonContext *out_context);
NeuronGpuDeviceClass classify_device_type(VkPhysicalDeviceType type);

int open_loader(void);
void close_loader(void);
int load_global_functions(void);
int load_instance_functions(void);
int load_device_functions(void);

int select_queue_families(VkPhysicalDevice device, uint32_t *out_compute_family,
                          uint32_t *out_graphics_family,
                          int *out_has_graphics);
int device_supports_extension(VkPhysicalDevice device,
                              const char *extension_name);
int create_instance(void);
int pick_physical_device(NeuronGpuScopeMode scope_mode,
                         NeuronGpuDeviceClass preferred_device_class,
                         int require_graphics_queue);
int create_device(void);

int initialize_runtime(NeuronGpuScopeMode scope_mode,
                       NeuronGpuDeviceClass preferred_device_class,
                       int require_graphics_queue);
void destroy_runtime(void);

#endif

#endif

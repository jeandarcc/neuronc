#include "vulkan_common_internal.h"

#include <stdio.h>
#include <string.h>

#if NPP_VK_COMMON_HAS_HEADERS

int initialize_runtime(NeuronGpuScopeMode scope_mode,
                       NeuronGpuDeviceClass preferred_device_class,
                       int require_graphics_queue) {
  reset_state_values();
  clear_error();
  if (!open_loader()) {
    return 0;
  }
  if (!load_global_functions()) {
    return 0;
  }
  if (!create_instance()) {
    return 0;
  }
  if (!load_instance_functions()) {
    return 0;
  }
  if (!pick_physical_device(scope_mode, preferred_device_class,
                            require_graphics_queue)) {
    return 0;
  }
  if (!create_device()) {
    return 0;
  }

  g_common.vkGetDeviceQueue(g_common.device, g_common.compute_queue_family, 0,
                            &g_common.compute_queue);
  if (g_common.compute_queue == VK_NULL_HANDLE) {
    set_errorf("vkGetDeviceQueue failed for compute family");
    return 0;
  }
  if (g_common.has_graphics_queue &&
      g_common.graphics_queue_family != UINT32_MAX) {
    g_common.vkGetDeviceQueue(g_common.device, g_common.graphics_queue_family, 0,
                              &g_common.graphics_queue);
  }
  g_common.vkGetPhysicalDeviceMemoryProperties(g_common.physical_device,
                                               &g_common.memory_properties);

  g_common.available = 1;
  g_common.initialized = 1;
  fprintf(stderr,
          "[VULKAN_COMMON] initialized device=%s compute_family=%u graphics_family=%u swapchain=%s\n",
          g_common.selected_device_name, g_common.compute_queue_family,
          g_common.has_graphics_queue ? g_common.graphics_queue_family
                                      : UINT32_MAX,
          g_common.has_swapchain_extension ? "enabled" : "disabled");
  return 1;
}

void destroy_runtime(void) {
  if (g_common.device != VK_NULL_HANDLE && g_common.vkDestroyDevice != NULL) {
    g_common.vkDestroyDevice(g_common.device, NULL);
  }
  if (g_common.instance != VK_NULL_HANDLE &&
      g_common.vkDestroyInstance != NULL) {
    g_common.vkDestroyInstance(g_common.instance, NULL);
  }

  reset_state_values();
  close_loader();
  g_common.vkGetInstanceProcAddr = NULL;
  g_common.vkGetDeviceProcAddr = NULL;
  g_common.vkCreateInstance = NULL;
  g_common.vkDestroyInstance = NULL;
  g_common.vkEnumeratePhysicalDevices = NULL;
  g_common.vkGetPhysicalDeviceQueueFamilyProperties = NULL;
  g_common.vkGetPhysicalDeviceProperties = NULL;
  g_common.vkGetPhysicalDeviceMemoryProperties = NULL;
  g_common.vkEnumerateDeviceExtensionProperties = NULL;
  g_common.vkGetPhysicalDeviceSurfaceSupportKHR = NULL;
  g_common.vkCreateDevice = NULL;
  g_common.vkDestroyDevice = NULL;
  g_common.vkGetDeviceQueue = NULL;
  g_common.available = 0;
  g_common.initialized = 0;
  g_common.ref_count = 0;
}

int neuron_vk_common_acquire(NeuronGpuScopeMode scope_mode,
                             NeuronGpuDeviceClass preferred_device_class,
                             int require_graphics_queue,
                             NeuronVulkanCommonContext *out_context,
                             char *error_buffer, size_t error_size) {
  if (out_context == NULL) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size,
               "Vulkan common acquire out_context is null");
    }
    return -1;
  }

  if (g_common.initialized && g_common.available) {
    if (require_graphics_queue && !g_common.has_graphics_queue) {
      set_errorf("Shared Vulkan device does not expose a graphics queue");
      copy_error_out(error_buffer, error_size);
      return -1;
    }
    g_common.ref_count++;
    fill_context(out_context);
    copy_error_out(error_buffer, error_size);
    return 0;
  }

  if (!initialize_runtime(scope_mode, preferred_device_class,
                          require_graphics_queue)) {
    copy_error_out(error_buffer, error_size);
    destroy_runtime();
    return -1;
  }

  g_common.ref_count = 1;
  fill_context(out_context);
  clear_error();
  copy_error_out(error_buffer, error_size);
  return 0;
}

void neuron_vk_common_release(void) {
  if (!g_common.initialized || !g_common.available) {
    return;
  }
  if (g_common.ref_count > 0) {
    g_common.ref_count--;
  }
  if (g_common.ref_count == 0) {
    destroy_runtime();
  }
}

int neuron_vk_common_pick_present_queue(VkSurfaceKHR surface,
                                        uint32_t *out_queue_family,
                                        VkQueue *out_queue,
                                        char *error_buffer,
                                        size_t error_size) {
  if (!g_common.initialized || !g_common.available) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size,
               "Vulkan common context is not initialized");
    }
    return 0;
  }
  if (surface == VK_NULL_HANDLE) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size,
               "Vulkan present selection requires a surface");
    }
    return 0;
  }
  if (g_common.vkGetPhysicalDeviceSurfaceSupportKHR == NULL) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size,
               "vkGetPhysicalDeviceSurfaceSupportKHR is unavailable");
    }
    return 0;
  }

  uint32_t queue_family_count = 0;
  g_common.vkGetPhysicalDeviceQueueFamilyProperties(g_common.physical_device,
                                                    &queue_family_count, NULL);
  if (queue_family_count == 0) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size,
               "Selected Vulkan device has no queue families");
    }
    return 0;
  }

  uint32_t chosen = UINT32_MAX;
  for (uint32_t i = 0; i < queue_family_count; ++i) {
    VkBool32 supported = VK_FALSE;
    if (g_common.vkGetPhysicalDeviceSurfaceSupportKHR(g_common.physical_device, i,
                                                      surface,
                                                      &supported) != VK_SUCCESS) {
      continue;
    }
    if (!supported) {
      continue;
    }
    if (i == g_common.graphics_queue_family) {
      chosen = i;
      break;
    }
    if (chosen == UINT32_MAX) {
      chosen = i;
    }
  }
  if (chosen == UINT32_MAX) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size,
               "No queue family supports present for the given surface");
    }
    return 0;
  }

  VkQueue queue = VK_NULL_HANDLE;
  g_common.vkGetDeviceQueue(g_common.device, chosen, 0, &queue);
  if (queue == VK_NULL_HANDLE) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size,
               "vkGetDeviceQueue failed for selected present queue family");
    }
    return 0;
  }

  g_common.present_queue_family = chosen;
  g_common.present_queue = queue;
  g_common.has_present_queue = 1;

  if (out_queue_family != NULL) {
    *out_queue_family = chosen;
  }
  if (out_queue != NULL) {
    *out_queue = queue;
  }
  if (error_buffer != NULL && error_size > 0) {
    error_buffer[0] = '\0';
  }
  return 1;
}

#else

int neuron_vk_common_acquire(NeuronGpuScopeMode scope_mode,
                             NeuronGpuDeviceClass preferred_device_class,
                             int require_graphics_queue,
                             NeuronVulkanCommonContext *out_context,
                             char *error_buffer, size_t error_size) {
  (void)scope_mode;
  (void)preferred_device_class;
  (void)require_graphics_queue;
  if (out_context != NULL) {
    memset(out_context, 0, sizeof(*out_context));
  }
  if (error_buffer != NULL && error_size > 0) {
    snprintf(error_buffer, error_size,
             "Vulkan headers were not available at build time");
  }
  return -1;
}

void neuron_vk_common_release(void) {}

int neuron_vk_common_pick_present_queue(VkSurfaceKHR surface,
                                        uint32_t *out_queue_family,
                                        VkQueue *out_queue,
                                        char *error_buffer,
                                        size_t error_size) {
  (void)surface;
  if (out_queue_family != NULL) {
    *out_queue_family = 0;
  }
  if (out_queue != NULL) {
    *out_queue = NULL;
  }
  if (error_buffer != NULL && error_size > 0) {
    snprintf(error_buffer, error_size,
             "Vulkan headers were not available at build time");
  }
  return 0;
}

#endif

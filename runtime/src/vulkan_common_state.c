#include "vulkan_common_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if Neuron_VK_COMMON_HAS_HEADERS

VulkanCommonState g_common = {0};

void set_errorf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_common.last_error, sizeof(g_common.last_error), fmt, args);
  va_end(args);
}

void clear_error(void) { g_common.last_error[0] = '\0'; }

void copy_error_out(char *error_buffer, size_t error_size) {
  if (error_buffer == NULL || error_size == 0) {
    return;
  }
  if (g_common.last_error[0] == '\0') {
    error_buffer[0] = '\0';
    return;
  }
  snprintf(error_buffer, error_size, "%s", g_common.last_error);
}

void reset_state_values(void) {
  g_common.instance = VK_NULL_HANDLE;
  g_common.physical_device = VK_NULL_HANDLE;
  g_common.device = VK_NULL_HANDLE;
  g_common.graphics_queue = VK_NULL_HANDLE;
  g_common.compute_queue = VK_NULL_HANDLE;
  g_common.present_queue = VK_NULL_HANDLE;
  g_common.graphics_queue_family = UINT32_MAX;
  g_common.compute_queue_family = UINT32_MAX;
  g_common.present_queue_family = UINT32_MAX;
  g_common.has_graphics_queue = 0;
  g_common.has_present_queue = 0;
  g_common.has_swapchain_extension = 0;
  g_common.selected_device_class = NEURON_GPU_DEVICE_CLASS_ANY;
  g_common.selected_device_name[0] = '\0';
  memset(&g_common.memory_properties, 0, sizeof(g_common.memory_properties));
}

void fill_context(NeuronVulkanCommonContext *out_context) {
  if (out_context == NULL) {
    return;
  }
  memset(out_context, 0, sizeof(*out_context));
  out_context->vkGetInstanceProcAddr = g_common.vkGetInstanceProcAddr;
  out_context->vkGetDeviceProcAddr = g_common.vkGetDeviceProcAddr;
  out_context->instance = g_common.instance;
  out_context->physical_device = g_common.physical_device;
  out_context->device = g_common.device;
  out_context->graphics_queue = g_common.graphics_queue;
  out_context->compute_queue = g_common.compute_queue;
  out_context->present_queue = g_common.present_queue;
  out_context->graphics_queue_family = g_common.graphics_queue_family;
  out_context->compute_queue_family = g_common.compute_queue_family;
  out_context->present_queue_family = g_common.present_queue_family;
  out_context->has_graphics_queue = g_common.has_graphics_queue;
  out_context->has_present_queue = g_common.has_present_queue;
  out_context->has_swapchain_extension = g_common.has_swapchain_extension;
  out_context->memory_properties = g_common.memory_properties;
  out_context->selected_device_class = g_common.selected_device_class;
  snprintf(out_context->selected_device_name,
           sizeof(out_context->selected_device_name), "%s",
           g_common.selected_device_name);
}

NeuronGpuDeviceClass classify_device_type(VkPhysicalDeviceType type) {
  switch (type) {
  case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
    return NEURON_GPU_DEVICE_CLASS_DISCRETE;
  case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
    return NEURON_GPU_DEVICE_CLASS_INTEGRATED;
  default:
    return NEURON_GPU_DEVICE_CLASS_ANY;
  }
}

#endif

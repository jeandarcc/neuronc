#include "vulkan_common_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if NPP_VK_COMMON_HAS_HEADERS

int select_queue_families(VkPhysicalDevice device, uint32_t *out_compute_family,
                          uint32_t *out_graphics_family,
                          int *out_has_graphics) {
  if (out_compute_family == NULL || out_graphics_family == NULL ||
      out_has_graphics == NULL) {
    return 0;
  }

  *out_compute_family = UINT32_MAX;
  *out_graphics_family = UINT32_MAX;
  *out_has_graphics = 0;

  uint32_t queue_family_count = 0;
  g_common.vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                                    NULL);
  if (queue_family_count == 0) {
    return 0;
  }

  VkQueueFamilyProperties *families = (VkQueueFamilyProperties *)calloc(
      queue_family_count, sizeof(VkQueueFamilyProperties));
  if (families == NULL) {
    return 0;
  }

  g_common.vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                                    families);

  uint32_t compute_any = UINT32_MAX;
  uint32_t compute_graphics = UINT32_MAX;
  uint32_t graphics_any = UINT32_MAX;
  for (uint32_t i = 0; i < queue_family_count; ++i) {
    const VkQueueFlags flags = families[i].queueFlags;
    if ((flags & VK_QUEUE_COMPUTE_BIT) != 0) {
      if (compute_any == UINT32_MAX) {
        compute_any = i;
      }
      if ((flags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
          compute_graphics == UINT32_MAX) {
        compute_graphics = i;
      }
    }
    if ((flags & VK_QUEUE_GRAPHICS_BIT) != 0 && graphics_any == UINT32_MAX) {
      graphics_any = i;
    }
  }

  free(families);
  if (compute_any == UINT32_MAX) {
    return 0;
  }

  *out_compute_family =
      compute_graphics != UINT32_MAX ? compute_graphics : compute_any;
  *out_graphics_family = graphics_any;
  *out_has_graphics = graphics_any != UINT32_MAX ? 1 : 0;
  return 1;
}

int device_supports_extension(VkPhysicalDevice device,
                              const char *extension_name) {
  if (extension_name == NULL) {
    return 0;
  }
  uint32_t extension_count = 0;
  if (g_common.vkEnumerateDeviceExtensionProperties(device, NULL,
                                                    &extension_count,
                                                    NULL) != VK_SUCCESS ||
      extension_count == 0) {
    return 0;
  }

  VkExtensionProperties *extensions = (VkExtensionProperties *)calloc(
      extension_count, sizeof(VkExtensionProperties));
  if (extensions == NULL) {
    return 0;
  }

  int found = 0;
  if (g_common.vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count,
                                                    extensions) == VK_SUCCESS) {
    for (uint32_t i = 0; i < extension_count; ++i) {
      if (strcmp(extensions[i].extensionName, extension_name) == 0) {
        found = 1;
        break;
      }
    }
  }
  free(extensions);
  return found;
}

int create_instance(void) {
  VkApplicationInfo app_info = {0};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "npp-runtime";
  app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app_info.pEngineName = "npp";
  app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  app_info.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo create_info = {0};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;

#if defined(_WIN32)
  const char *instance_extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME,
                                       VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
  create_info.enabledExtensionCount = 2;
  create_info.ppEnabledExtensionNames = instance_extensions;
#endif

  if (g_common.vkCreateInstance(&create_info, NULL, &g_common.instance) !=
      VK_SUCCESS) {
    set_errorf("vkCreateInstance failed");
    return 0;
  }
  return 1;
}

int pick_physical_device(NeuronGpuScopeMode scope_mode,
                         NeuronGpuDeviceClass preferred_device_class,
                         int require_graphics_queue) {
  uint32_t device_count = 0;
  if (g_common.vkEnumeratePhysicalDevices(g_common.instance, &device_count, NULL) !=
          VK_SUCCESS ||
      device_count == 0) {
    set_errorf("No Vulkan physical devices were found");
    return 0;
  }
  VkPhysicalDevice *devices =
      (VkPhysicalDevice *)calloc(device_count, sizeof(VkPhysicalDevice));
  if (devices == NULL) {
    set_errorf("Failed to allocate Vulkan physical device list");
    return 0;
  }
  if (g_common.vkEnumeratePhysicalDevices(g_common.instance, &device_count,
                                          devices) != VK_SUCCESS) {
    free(devices);
    set_errorf("vkEnumeratePhysicalDevices failed");
    return 0;
  }

  VkPhysicalDevice best = VK_NULL_HANDLE;
  uint32_t best_compute_family = UINT32_MAX;
  uint32_t best_graphics_family = UINT32_MAX;
  int best_has_graphics = 0;
  int best_score = -1;
  NeuronGpuDeviceClass best_class = NEURON_GPU_DEVICE_CLASS_ANY;
  char best_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {0};

  const int has_preference = preferred_device_class != NEURON_GPU_DEVICE_CLASS_ANY;
  const int force_preference =
      scope_mode == NEURON_GPU_SCOPE_MODE_FORCE && has_preference;
  for (uint32_t i = 0; i < device_count; ++i) {
    uint32_t compute_family = UINT32_MAX;
    uint32_t graphics_family = UINT32_MAX;
    int has_graphics = 0;
    if (!select_queue_families(devices[i], &compute_family, &graphics_family,
                               &has_graphics)) {
      continue;
    }
    if (require_graphics_queue && !has_graphics) {
      continue;
    }

    VkPhysicalDeviceProperties props = {0};
    g_common.vkGetPhysicalDeviceProperties(devices[i], &props);
    const NeuronGpuDeviceClass device_class =
        classify_device_type(props.deviceType);
    const int class_matches =
        !has_preference || device_class == preferred_device_class;
    if (force_preference && !class_matches) {
      continue;
    }

    int score = 0;
    if (class_matches) {
      score += 500;
    }
    if (has_graphics) {
      score += 100;
    }
    if (compute_family == graphics_family) {
      score += 50;
    }
    if (device_class == NEURON_GPU_DEVICE_CLASS_DISCRETE) {
      score += 10;
    }
    if (score > best_score) {
      best_score = score;
      best = devices[i];
      best_compute_family = compute_family;
      best_graphics_family = graphics_family;
      best_has_graphics = has_graphics;
      best_class = device_class;
      snprintf(best_name, sizeof(best_name), "%s", props.deviceName);
    }
  }
  free(devices);

  if (best == VK_NULL_HANDLE) {
    set_errorf("No Vulkan GPU with required queues was found");
    return 0;
  }
  g_common.physical_device = best;
  g_common.compute_queue_family = best_compute_family;
  g_common.graphics_queue_family = best_graphics_family;
  g_common.has_graphics_queue = best_has_graphics;
  g_common.selected_device_class = best_class;
  snprintf(g_common.selected_device_name, sizeof(g_common.selected_device_name),
           "%s", best_name);
  return 1;
}

int create_device(void) {
  uint32_t queue_family_count = 0;
  g_common.vkGetPhysicalDeviceQueueFamilyProperties(g_common.physical_device,
                                                    &queue_family_count, NULL);
  if (queue_family_count == 0) {
    set_errorf("Selected Vulkan device has no queue families");
    return 0;
  }

  VkQueueFamilyProperties *families = (VkQueueFamilyProperties *)calloc(
      queue_family_count, sizeof(VkQueueFamilyProperties));
  if (families == NULL) {
    set_errorf("Failed to allocate Vulkan queue family list");
    return 0;
  }
  g_common.vkGetPhysicalDeviceQueueFamilyProperties(g_common.physical_device,
                                                    &queue_family_count, families);

  VkDeviceQueueCreateInfo *queue_infos = (VkDeviceQueueCreateInfo *)calloc(
      queue_family_count, sizeof(VkDeviceQueueCreateInfo));
  float *queue_priorities = (float *)calloc(queue_family_count, sizeof(float));
  if (queue_infos == NULL || queue_priorities == NULL) {
    free(families);
    free(queue_infos);
    free(queue_priorities);
    set_errorf("Failed to allocate Vulkan queue create structures");
    return 0;
  }

  uint32_t queue_info_count = 0;
  for (uint32_t i = 0; i < queue_family_count; ++i) {
    if (families[i].queueCount == 0) {
      continue;
    }
    queue_priorities[queue_info_count] = 1.0f;
    queue_infos[queue_info_count].sType =
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[queue_info_count].queueFamilyIndex = i;
    queue_infos[queue_info_count].queueCount = 1;
    queue_infos[queue_info_count].pQueuePriorities =
        &queue_priorities[queue_info_count];
    queue_info_count++;
  }

  const char *enabled_extensions[1];
  g_common.has_swapchain_extension =
      device_supports_extension(g_common.physical_device,
                                VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  uint32_t enabled_extension_count = 0;
  if (g_common.has_swapchain_extension) {
    enabled_extensions[enabled_extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
  }

  VkDeviceCreateInfo device_info = {0};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = queue_info_count;
  device_info.pQueueCreateInfos = queue_infos;
  device_info.enabledExtensionCount = enabled_extension_count;
  device_info.ppEnabledExtensionNames =
      enabled_extension_count > 0 ? enabled_extensions : NULL;
  const VkResult result = g_common.vkCreateDevice(g_common.physical_device,
                                                  &device_info, NULL,
                                                  &g_common.device);
  free(families);
  free(queue_priorities);
  free(queue_infos);
  if (result != VK_SUCCESS) {
    set_errorf("vkCreateDevice failed");
    return 0;
  }
  return load_device_functions();
}

#endif

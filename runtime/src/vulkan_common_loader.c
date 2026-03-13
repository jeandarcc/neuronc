#include "vulkan_common_internal.h"
#include "neuron_platform.h"

#if Neuron_VK_COMMON_HAS_HEADERS

static void *load_loader_symbol(const char *name) {
  if (name == NULL || g_common.loader == NULL) {
    return NULL;
  }
  return neuron_platform_load_symbol(g_common.loader, name);
}

int open_loader(void) {
#if defined(_WIN32)
  const char *vulkan_lib = "vulkan-1.dll";
#elif defined(__APPLE__)
  const char *vulkan_lib = "libvulkan.dylib";
#else
  const char *vulkan_lib = "libvulkan.so.1";
#endif
  g_common.loader = neuron_platform_open_library(vulkan_lib);
#if defined(__linux__)
  if (g_common.loader == NULL) {
    g_common.loader = neuron_platform_open_library("libvulkan.so");
  }
#endif
  if (g_common.loader == NULL) {
    set_errorf("Vulkan loader was not found at runtime");
    return 0;
  }
  return 1;
}

void close_loader(void) {
  if (g_common.loader == NULL) {
    return;
  }
  neuron_platform_close_library(g_common.loader);
  g_common.loader = NULL;
}

int load_global_functions(void) {
  g_common.vkGetInstanceProcAddr =
      (PFN_vkGetInstanceProcAddr)load_loader_symbol("vkGetInstanceProcAddr");
  if (g_common.vkGetInstanceProcAddr == NULL) {
    set_errorf("vkGetInstanceProcAddr was not found in Vulkan loader");
    return 0;
  }

  g_common.vkCreateInstance =
      (PFN_vkCreateInstance)g_common.vkGetInstanceProcAddr(VK_NULL_HANDLE,
                                                           "vkCreateInstance");
  if (g_common.vkCreateInstance == NULL) {
    set_errorf("Missing Vulkan global function: vkCreateInstance");
    return 0;
  }
  return 1;
}

int load_instance_functions(void) {
#define LOAD_INSTANCE_FN(name)                                                  \
  do {                                                                          \
    g_common.name =                                                             \
        (PFN_##name)g_common.vkGetInstanceProcAddr(g_common.instance, #name);  \
    if (g_common.name == NULL) {                                                \
      set_errorf("Missing Vulkan instance function: %s", #name);               \
      return 0;                                                                 \
    }                                                                           \
  } while (0)

  LOAD_INSTANCE_FN(vkDestroyInstance);
  LOAD_INSTANCE_FN(vkEnumeratePhysicalDevices);
  LOAD_INSTANCE_FN(vkGetPhysicalDeviceQueueFamilyProperties);
  LOAD_INSTANCE_FN(vkGetPhysicalDeviceProperties);
  LOAD_INSTANCE_FN(vkGetPhysicalDeviceMemoryProperties);
  LOAD_INSTANCE_FN(vkEnumerateDeviceExtensionProperties);
  LOAD_INSTANCE_FN(vkCreateDevice);
  LOAD_INSTANCE_FN(vkGetDeviceProcAddr);

  g_common.vkGetPhysicalDeviceSurfaceSupportKHR =
      (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)g_common.vkGetInstanceProcAddr(
          g_common.instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
#undef LOAD_INSTANCE_FN
  return 1;
}

int load_device_functions(void) {
#define LOAD_DEVICE_FN(name)                                                    \
  do {                                                                          \
    g_common.name =                                                             \
        (PFN_##name)g_common.vkGetDeviceProcAddr(g_common.device, #name);      \
    if (g_common.name == NULL) {                                                \
      set_errorf("Missing Vulkan device function: %s", #name);                 \
      return 0;                                                                 \
    }                                                                           \
  } while (0)

  LOAD_DEVICE_FN(vkDestroyDevice);
  LOAD_DEVICE_FN(vkGetDeviceQueue);
#undef LOAD_DEVICE_FN
  return 1;
}

#endif

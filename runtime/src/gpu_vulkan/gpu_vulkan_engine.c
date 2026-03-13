/* Restored single translation unit for the Vulkan engine.
 * The previous engine/ fragment chain split preprocessor blocks across files,
 * which made the build invalid. Keep this file buildable first; refactor later
 * on real function boundaries. */


#include "gpu_internal.h"
#include "gpu_vulkan/gpu_vulkan_internal.h"
#include "vulkan_common.h"
#include "vulkan_shaders.h"
#include "neuron_platform.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#if Neuron_VK_COMMON_HAS_HEADERS

typedef struct {
  VkBuffer buffer;
  VkDeviceMemory memory;
  void *mapped_host;
} VulkanBuffer;

typedef struct {
  void *host_ptr;
  VkDeviceSize size;
  int dirty_output;
  int from_host_cache;
  int consumed_after_write;
  VulkanBuffer buffer;
} VulkanScopeBufferEntry;

typedef struct {
  VulkanBuffer buffer;
  VkDeviceSize size;
  VkBufferUsageFlags usage;
} VulkanTransientBufferEntry;

typedef struct {
  VulkanBuffer buffer;
  VkDeviceSize size;
  VkBufferUsageFlags usage;
  int memory_class;
} VulkanScopePooledBuffer;

typedef struct {
  VkDescriptorSet set;
  uint32_t buffer_count;
  VkBuffer buffers[8];
} VulkanScopeDescriptorUpdate;

typedef struct {
  const void *host_ptr;
  VkDeviceSize size;
  uint64_t checksum;
  uint64_t last_use_tick;
  VulkanBuffer buffer;
} VulkanHostInputCacheEntry;

typedef struct {
  VulkanBuffer buffer;
  VkDeviceSize size;
  VkDeviceSize write_offset;
} VulkanUploadRingBuffer;

typedef struct {
  int active;
  uint32_t opcode;
  uint32_t element_count;
  const float *lhs_host;
  const float *rhs_host;
  float *out_host;
  VulkanBuffer lhs_buffer;
  VulkanBuffer rhs_buffer;
  VulkanBuffer out_buffer;
} VulkanPendingBinaryOp;

typedef struct {
  int active;
  uint32_t first_opcode;
  uint32_t second_opcode;
  uint32_t temp_is_lhs;
  uint32_t element_count;
  float *tmp_host;
  float *mid_host;
  VulkanBuffer first_lhs_buffer;
  VulkanBuffer first_rhs_buffer;
  VulkanBuffer second_other_buffer;
  VulkanBuffer tmp_buffer;
  VulkanBuffer mid_buffer;
} VulkanPendingBinaryChainOp;

typedef struct {
  NeuronPlatformLibraryHandle loader;
  int initialized;
  int available;
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
  PFN_vkCreateDevice vkCreateDevice;
  PFN_vkDestroyDevice vkDestroyDevice;
  PFN_vkGetDeviceQueue vkGetDeviceQueue;

  PFN_vkCreateCommandPool vkCreateCommandPool;
  PFN_vkDestroyCommandPool vkDestroyCommandPool;
  PFN_vkCreateBuffer vkCreateBuffer;
  PFN_vkDestroyBuffer vkDestroyBuffer;
  PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
  PFN_vkAllocateMemory vkAllocateMemory;
  PFN_vkFreeMemory vkFreeMemory;
  PFN_vkBindBufferMemory vkBindBufferMemory;
  PFN_vkMapMemory vkMapMemory;
  PFN_vkUnmapMemory vkUnmapMemory;
  PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
  PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
  PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
  PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
  PFN_vkCreateShaderModule vkCreateShaderModule;
  PFN_vkDestroyShaderModule vkDestroyShaderModule;
  PFN_vkCreateComputePipelines vkCreateComputePipelines;
  PFN_vkDestroyPipeline vkDestroyPipeline;
  PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
  PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
  PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
  PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
  PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
  PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
  PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
  PFN_vkEndCommandBuffer vkEndCommandBuffer;
  PFN_vkCmdBindPipeline vkCmdBindPipeline;
  PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
  PFN_vkCmdPushConstants vkCmdPushConstants;
  PFN_vkCmdDispatch vkCmdDispatch;
  PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
  PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
  PFN_vkQueueSubmit vkQueueSubmit;
  PFN_vkQueueWaitIdle vkQueueWaitIdle;
  PFN_vkCreateFence vkCreateFence;
  PFN_vkDestroyFence vkDestroyFence;
  PFN_vkWaitForFences vkWaitForFences;
  PFN_vkDeviceWaitIdle vkDeviceWaitIdle;

  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue queue;
  uint32_t queue_family_index;
  int common_context_acquired;
  VkPhysicalDeviceMemoryProperties memory_properties;
  VkCommandPool command_pool;
  NeuronGpuScopeMode requested_scope_mode;
  NeuronGpuDeviceClass requested_device_class;
  NeuronGpuDeviceClass selected_device_class;
  char selected_device_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];

  VkDescriptorSetLayout binary_descriptor_set_layout;
  VkPipelineLayout binary_pipeline_layout;
  VkShaderModule binary_shader_module;
  VkPipeline binary_pipeline;

  VkDescriptorSetLayout binary_chain_descriptor_set_layout;
  VkPipelineLayout binary_chain_pipeline_layout;
  VkShaderModule binary_chain_shader_module;
  VkPipeline binary_chain_pipeline;

  VkDescriptorSetLayout binary_then_fma_descriptor_set_layout;
  VkPipelineLayout binary_then_fma_pipeline_layout;
  VkShaderModule binary_then_fma_shader_module;
  VkPipeline binary_then_fma_pipeline;

  VkDescriptorSetLayout binary_chain_then_fma_descriptor_set_layout;
  VkPipelineLayout binary_chain_then_fma_pipeline_layout;
  VkShaderModule binary_chain_then_fma_shader_module;
  VkPipeline binary_chain_then_fma_pipeline;

  VkDescriptorSetLayout fma_descriptor_set_layout;
  VkPipelineLayout fma_pipeline_layout;
  VkShaderModule fma_shader_module;
  VkPipeline fma_pipeline;

  VkDescriptorSetLayout matmul_dense_descriptor_set_layout;
  VkPipelineLayout matmul_dense_pipeline_layout;
  VkShaderModule matmul_dense_shader_module;
  VkPipeline matmul_dense_pipeline;

  VkDescriptorSetLayout matmul_packed_descriptor_set_layout;
  VkPipelineLayout matmul_packed_pipeline_layout;
  VkShaderModule matmul_packed_shader_module;
  VkPipeline matmul_packed_pipeline;

  int scope_active;
  VkDescriptorPool scope_descriptor_pool;
  VkCommandBuffer scope_command_buffer;
  VulkanScopeBufferEntry *scope_entries;
  uint32_t scope_entry_count;
  uint32_t scope_entry_capacity;
  VulkanTransientBufferEntry *scope_transient_buffers;
  uint32_t scope_transient_count;
  uint32_t scope_transient_capacity;
  VulkanBuffer scope_zero_buffer;
  int scope_has_zero_buffer;
  VulkanScopePooledBuffer *scope_buffer_pool;
  uint32_t scope_buffer_pool_count;
  uint32_t scope_buffer_pool_capacity;
  VulkanScopeDescriptorUpdate *scope_descriptor_updates;
  uint32_t scope_descriptor_update_count;
  uint32_t scope_descriptor_update_capacity;
  VulkanHostInputCacheEntry *host_input_cache_entries;
  uint32_t host_input_cache_count;
  uint32_t host_input_cache_capacity;
  uint64_t host_input_cache_tick;
  int host_input_cache_enabled;
  int host_input_cache_verify;
  VulkanUploadRingBuffer *scope_upload_ring;
  uint32_t scope_upload_ring_count;



  uint32_t scope_upload_ring_capacity;
  uint32_t scope_upload_ring_cursor;
  VkDeviceSize scope_upload_ring_chunk_size;
  int scope_readback_sink_only;
  int scope_batch_enabled;
  int scope_fusion_enabled;
  int scope_metrics_enabled;
  int scope_has_recorded_work;
  uint64_t scope_metric_dispatch_count;
  uint64_t scope_metric_barrier_count;
  uint64_t scope_metric_descriptor_writes;
  uint64_t scope_metric_readback_bytes;
  VulkanPendingBinaryOp scope_pending_binary;
  VulkanPendingBinaryChainOp scope_pending_binary_chain;
} VulkanState;

static VulkanState g_vulkan = {0};

#define Neuron_VK_SCOPE_MAX_SETS 8192u
#define Neuron_VK_SCOPE_MAX_DESCRIPTORS (Neuron_VK_SCOPE_MAX_SETS * 8u)
#define Neuron_VK_MAX_DISPATCH_BUFFERS 8u
#define Neuron_VK_SCOPE_BUFFER_POOL_MAX_COUNT 2048u
#define Neuron_VK_HOST_INPUT_CACHE_MAX_COUNT 1024u
#define Neuron_VK_MEMORY_HOST_VISIBLE 1
#define Neuron_VK_MEMORY_DEVICE_LOCAL 2
#define Neuron_VK_UPLOAD_RING_DEFAULT_CHUNK (8u * 1024u * 1024u)
#define Neuron_VK_UPLOAD_ALIGNMENT 16u
#define Neuron_VK_USAGE_STORAGE                                                    \
  (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |    \
   VK_BUFFER_USAGE_TRANSFER_DST_BIT)
#define Neuron_VK_USAGE_STAGING_DST VK_BUFFER_USAGE_TRANSFER_DST_BIT
#define Neuron_VK_USAGE_STAGING_SRC VK_BUFFER_USAGE_TRANSFER_SRC_BIT

static int create_host_visible_storage_buffer(VkDeviceSize byte_size,
                                              VulkanBuffer *out_buffer);
static int create_host_visible_buffer(VkDeviceSize byte_size,
                                      VkBufferUsageFlags usage,
                                      VulkanBuffer *out_buffer);
static int create_device_local_storage_buffer(VkDeviceSize byte_size,
                                              VulkanBuffer *out_buffer);
static int map_copy_host_to_buffer(const VulkanBuffer *buffer,
                                   const void *source, VkDeviceSize size);
static int map_copy_buffer_to_host(const VulkanBuffer *buffer,
                                   void *destination, VkDeviceSize size);
static int dispatch_compute(VkPipeline pipeline, VkPipelineLayout pipeline_layout,
                            VkDescriptorSetLayout descriptor_set_layout,
                            const VulkanBuffer *buffers, uint32_t buffer_count,
                            const void *push_constants,
                            uint32_t push_constants_size,
                            uint32_t dispatch_x, uint32_t dispatch_y,
                            uint32_t dispatch_z);

static void set_errorf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_vulkan.last_error, sizeof(g_vulkan.last_error), fmt, args);
  va_end(args);
}

static void clear_error(void) { g_vulkan.last_error[0] = '\0'; }

static void copy_error_out(char *error_buffer, size_t error_size) {
  if (error_buffer == NULL || error_size == 0) {
    return;
  }
  if (g_vulkan.last_error[0] == '\0') {
    error_buffer[0] = '\0';
    return;
  }
  snprintf(error_buffer, error_size, "%s", g_vulkan.last_error);
}

static const char *device_class_name(NeuronGpuDeviceClass device_class) {
  switch (device_class) {
  case NEURON_GPU_DEVICE_CLASS_DISCRETE:
    return "discrete";
  case NEURON_GPU_DEVICE_CLASS_INTEGRATED:
    return "integrated";
  case NEURON_GPU_DEVICE_CLASS_ANY:
  default:
    return "any";
  }
}

static int equals_ignore_case(const char *lhs, const char *rhs) {
  if (lhs == NULL || rhs == NULL) {
    return 0;
  }
  while (*lhs != '\0' && *rhs != '\0') {
    char cl = *lhs;
    char cr = *rhs;
    if (cl >= 'A' && cl <= 'Z') {
      cl = (char)(cl - 'A' + 'a');
    }
    if (cr >= 'A' && cr <= 'Z') {
      cr = (char)(cr - 'A' + 'a');
    }
    if (cl != cr) {
      return 0;
    }
    ++lhs;
    ++rhs;
  }
  return *lhs == '\0' && *rhs == '\0';
}

static int env_flag_enabled(const char *name) {
  const char *value = getenv(name);
  if (value == NULL || value[0] == '\0') {
    return 0;
  }
  return equals_ignore_case(value, "1") || equals_ignore_case(value, "true") ||
         equals_ignore_case(value, "yes") || equals_ignore_case(value, "on");
}

static void *load_loader_symbol(const char *name) {
  if (name == NULL || g_vulkan.loader == NULL) {
    return NULL;
  }
  return neuron_platform_load_symbol(g_vulkan.loader, name);
}

static int open_vulkan_loader(void) {
#if defined(_WIN32)
  const char *vulkan_lib = "vulkan-1.dll";
#elif defined(__APPLE__)
  const char *vulkan_lib = "libvulkan.dylib";
#else
  const char *vulkan_lib = "libvulkan.so.1";
#endif
  g_vulkan.loader = neuron_platform_open_library(vulkan_lib);
#if defined(__linux__)
  if (g_vulkan.loader == NULL) {
    g_vulkan.loader = neuron_platform_open_library("libvulkan.so");
  }
#endif
  if (g_vulkan.loader == NULL) {
    set_errorf("Vulkan loader was not found at runtime");
    return 0;
  }
  return 1;
}

static void close_vulkan_loader(void) {
  if (g_vulkan.loader == NULL) {
    return;
  }
  neuron_platform_close_library(g_vulkan.loader);
  g_vulkan.loader = NULL;
}

static int load_global_functions(void) {
  g_vulkan.vkGetInstanceProcAddr =
      (PFN_vkGetInstanceProcAddr)load_loader_symbol("vkGetInstanceProcAddr");
  if (g_vulkan.vkGetInstanceProcAddr == NULL) {
    set_errorf("vkGetInstanceProcAddr was not found in Vulkan loader");
    return 0;
  }

  g_vulkan.vkCreateInstance = (PFN_vkCreateInstance)
      g_vulkan.vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
  if (g_vulkan.vkCreateInstance == NULL) {
    set_errorf("vkCreateInstance was not found");
    return 0;
  }
  return 1;
}

static int load_instance_functions(void) {
#define LOAD_INSTANCE_FN(name)                                                   \
  do {                                                                           \
    g_vulkan.name =                                                              \
        (PFN_##name)g_vulkan.vkGetInstanceProcAddr(g_vulkan.instance, #name);   \
    if (g_vulkan.name == NULL) {                                                 \
      set_errorf("Missing Vulkan instance function: %s", #name);                \
      return 0;                                                                  \
    }                                                                            \
  } while (0)

  LOAD_INSTANCE_FN(vkDestroyInstance);
  LOAD_INSTANCE_FN(vkEnumeratePhysicalDevices);
  LOAD_INSTANCE_FN(vkGetPhysicalDeviceQueueFamilyProperties);
  LOAD_INSTANCE_FN(vkGetPhysicalDeviceProperties);
  LOAD_INSTANCE_FN(vkGetPhysicalDeviceMemoryProperties);
  LOAD_INSTANCE_FN(vkCreateDevice);
#undef LOAD_INSTANCE_FN
  return 1;
}

static int load_device_functions(void) {
  g_vulkan.vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)
      g_vulkan.vkGetInstanceProcAddr(g_vulkan.instance, "vkGetDeviceProcAddr");
  if (g_vulkan.vkGetDeviceProcAddr == NULL) {
    set_errorf("Missing Vulkan instance function: vkGetDeviceProcAddr");
    return 0;
  }

#define LOAD_DEVICE_FN(name)                                                     \
  do {                                                                           \
    g_vulkan.name =                                                              \
        (PFN_##name)g_vulkan.vkGetDeviceProcAddr(g_vulkan.device, #name);       \
    if (g_vulkan.name == NULL) {                                                 \
      set_errorf("Missing Vulkan device function: %s", #name);                  \
      return 0;                                                                  \
    }                                                                            \
  } while (0)

  LOAD_DEVICE_FN(vkDestroyDevice);
  LOAD_DEVICE_FN(vkGetDeviceQueue);
  LOAD_DEVICE_FN(vkCreateCommandPool);
  LOAD_DEVICE_FN(vkDestroyCommandPool);
  LOAD_DEVICE_FN(vkCreateBuffer);
  LOAD_DEVICE_FN(vkDestroyBuffer);
  LOAD_DEVICE_FN(vkGetBufferMemoryRequirements);
  LOAD_DEVICE_FN(vkAllocateMemory);
  LOAD_DEVICE_FN(vkFreeMemory);
  LOAD_DEVICE_FN(vkBindBufferMemory);
  LOAD_DEVICE_FN(vkMapMemory);
  LOAD_DEVICE_FN(vkUnmapMemory);
  LOAD_DEVICE_FN(vkCreateDescriptorSetLayout);
  LOAD_DEVICE_FN(vkDestroyDescriptorSetLayout);
  LOAD_DEVICE_FN(vkCreatePipelineLayout);



  LOAD_DEVICE_FN(vkDestroyPipelineLayout);
  LOAD_DEVICE_FN(vkCreateShaderModule);
  LOAD_DEVICE_FN(vkDestroyShaderModule);
  LOAD_DEVICE_FN(vkCreateComputePipelines);
  LOAD_DEVICE_FN(vkDestroyPipeline);
  LOAD_DEVICE_FN(vkCreateDescriptorPool);
  LOAD_DEVICE_FN(vkDestroyDescriptorPool);
  LOAD_DEVICE_FN(vkAllocateDescriptorSets);
  LOAD_DEVICE_FN(vkUpdateDescriptorSets);
  LOAD_DEVICE_FN(vkAllocateCommandBuffers);
  LOAD_DEVICE_FN(vkFreeCommandBuffers);
  LOAD_DEVICE_FN(vkBeginCommandBuffer);
  LOAD_DEVICE_FN(vkEndCommandBuffer);
  LOAD_DEVICE_FN(vkCmdBindPipeline);
  LOAD_DEVICE_FN(vkCmdBindDescriptorSets);
  LOAD_DEVICE_FN(vkCmdPushConstants);
  LOAD_DEVICE_FN(vkCmdDispatch);
  LOAD_DEVICE_FN(vkCmdPipelineBarrier);
  LOAD_DEVICE_FN(vkCmdCopyBuffer);
  LOAD_DEVICE_FN(vkQueueSubmit);
  LOAD_DEVICE_FN(vkQueueWaitIdle);
  LOAD_DEVICE_FN(vkCreateFence);
  LOAD_DEVICE_FN(vkDestroyFence);
  LOAD_DEVICE_FN(vkWaitForFences);
  LOAD_DEVICE_FN(vkDeviceWaitIdle);
#undef LOAD_DEVICE_FN
  return 1;
}

static void destroy_pipeline_bundle(VkPipeline *pipeline,
                                    VkShaderModule *shader_module,
                                    VkPipelineLayout *pipeline_layout,
                                    VkDescriptorSetLayout *set_layout) {
  if (g_vulkan.device == VK_NULL_HANDLE) {
    return;
  }
  if (*pipeline != VK_NULL_HANDLE && g_vulkan.vkDestroyPipeline != NULL) {
    g_vulkan.vkDestroyPipeline(g_vulkan.device, *pipeline, NULL);
    *pipeline = VK_NULL_HANDLE;
  }
  if (*shader_module != VK_NULL_HANDLE && g_vulkan.vkDestroyShaderModule != NULL) {
    g_vulkan.vkDestroyShaderModule(g_vulkan.device, *shader_module, NULL);
    *shader_module = VK_NULL_HANDLE;
  }
  if (*pipeline_layout != VK_NULL_HANDLE &&
      g_vulkan.vkDestroyPipelineLayout != NULL) {
    g_vulkan.vkDestroyPipelineLayout(g_vulkan.device, *pipeline_layout, NULL);
    *pipeline_layout = VK_NULL_HANDLE;
  }
  if (*set_layout != VK_NULL_HANDLE && g_vulkan.vkDestroyDescriptorSetLayout != NULL) {
    g_vulkan.vkDestroyDescriptorSetLayout(g_vulkan.device, *set_layout, NULL);
    *set_layout = VK_NULL_HANDLE;
  }
}

static void destroy_buffer(VulkanBuffer *buffer) {
  if (buffer == NULL || g_vulkan.device == VK_NULL_HANDLE) {
    return;
  }
  if (buffer->mapped_host != NULL && g_vulkan.vkUnmapMemory != NULL &&
      buffer->memory != VK_NULL_HANDLE) {
    g_vulkan.vkUnmapMemory(g_vulkan.device, buffer->memory);
    buffer->mapped_host = NULL;
  }
  if (buffer->buffer != VK_NULL_HANDLE && g_vulkan.vkDestroyBuffer != NULL) {
    g_vulkan.vkDestroyBuffer(g_vulkan.device, buffer->buffer, NULL);
    buffer->buffer = VK_NULL_HANDLE;
  }
  if (buffer->memory != VK_NULL_HANDLE && g_vulkan.vkFreeMemory != NULL) {
    g_vulkan.vkFreeMemory(g_vulkan.device, buffer->memory, NULL);
    buffer->memory = VK_NULL_HANDLE;
  }
}

static int scope_batch_active(void) {
  return g_vulkan.scope_active && g_vulkan.scope_batch_enabled &&
         g_vulkan.scope_descriptor_pool != VK_NULL_HANDLE &&
         g_vulkan.scope_command_buffer != VK_NULL_HANDLE;
}

static int scope_pool_reserve(uint32_t needed) {
  if (needed <= g_vulkan.scope_buffer_pool_capacity) {
    return 1;
  }
  uint32_t new_capacity =
      g_vulkan.scope_buffer_pool_capacity > 0 ? g_vulkan.scope_buffer_pool_capacity
                                              : 64u;
  while (new_capacity < needed) {
    if (new_capacity > (UINT32_MAX / 2u)) {
      new_capacity = needed;
      break;
    }
    new_capacity *= 2u;
  }

  VulkanScopePooledBuffer *resized = (VulkanScopePooledBuffer *)realloc(
      g_vulkan.scope_buffer_pool,
      (size_t)new_capacity * sizeof(VulkanScopePooledBuffer));
  if (resized == NULL) {
    return 0;
  }
  g_vulkan.scope_buffer_pool = resized;
  g_vulkan.scope_buffer_pool_capacity = new_capacity;
  return 1;
}

static int scope_pool_take(VkDeviceSize size, VkBufferUsageFlags usage,
                           int memory_class, VulkanBuffer *out_buffer) {
  if (out_buffer == NULL || size == 0) {
    return 0;
  }

  uint32_t best_index = UINT32_MAX;
  VkDeviceSize best_size = 0;
  for (uint32_t i = 0; i < g_vulkan.scope_buffer_pool_count; ++i) {
    const VulkanScopePooledBuffer *entry = &g_vulkan.scope_buffer_pool[i];
    if (entry->usage != usage || entry->memory_class != memory_class ||
        entry->size < size) {
      continue;
    }
    if (best_index == UINT32_MAX || entry->size < best_size) {
      best_index = i;
      best_size = entry->size;
    }
  }

  if (best_index == UINT32_MAX) {
    return 0;
  }

  *out_buffer = g_vulkan.scope_buffer_pool[best_index].buffer;
  const uint32_t last = g_vulkan.scope_buffer_pool_count - 1u;
  if (best_index != last) {
    g_vulkan.scope_buffer_pool[best_index] = g_vulkan.scope_buffer_pool[last];
  }
  g_vulkan.scope_buffer_pool_count--;
  return 1;
}

static void scope_pool_release(VulkanBuffer *buffer, VkDeviceSize size,
                               VkBufferUsageFlags usage, int memory_class) {
  if (buffer == NULL || buffer->buffer == VK_NULL_HANDLE ||
      buffer->memory == VK_NULL_HANDLE || size == 0) {
    return;
  }

  if (g_vulkan.scope_buffer_pool_count >= Neuron_VK_SCOPE_BUFFER_POOL_MAX_COUNT ||
      !scope_pool_reserve(g_vulkan.scope_buffer_pool_count + 1u)) {
    destroy_buffer(buffer);
    return;
  }

  VulkanScopePooledBuffer *entry =
      &g_vulkan.scope_buffer_pool[g_vulkan.scope_buffer_pool_count];
  entry->buffer = *buffer;
  entry->size = size;
  entry->usage = usage;
  entry->memory_class = memory_class;
  g_vulkan.scope_buffer_pool_count++;

  buffer->buffer = VK_NULL_HANDLE;
  buffer->memory = VK_NULL_HANDLE;
  buffer->mapped_host = NULL;
}

static void scope_pool_destroy_all(void) {
  if (g_vulkan.scope_buffer_pool != NULL) {
    for (uint32_t i = 0; i < g_vulkan.scope_buffer_pool_count; ++i) {
      destroy_buffer(&g_vulkan.scope_buffer_pool[i].buffer);
    }
    free(g_vulkan.scope_buffer_pool);
  }
  g_vulkan.scope_buffer_pool = NULL;
  g_vulkan.scope_buffer_pool_count = 0;
  g_vulkan.scope_buffer_pool_capacity = 0;
}

static uint64_t compute_host_checksum(const void *data, VkDeviceSize size) {
  const uint8_t *bytes = (const uint8_t *)data;
  uint64_t hash = 1469598103934665603ULL;
  for (VkDeviceSize i = 0; i < size; ++i) {
    hash ^= (uint64_t)bytes[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

static int host_input_cache_reserve(uint32_t needed) {
  if (needed <= g_vulkan.host_input_cache_capacity) {
    return 1;
  }
  uint32_t new_capacity =
      g_vulkan.host_input_cache_capacity > 0 ? g_vulkan.host_input_cache_capacity
                                             : 64u;
  while (new_capacity < needed) {
    if (new_capacity > (UINT32_MAX / 2u)) {
      new_capacity = needed;
      break;
    }
    new_capacity *= 2u;
  }
  VulkanHostInputCacheEntry *resized = (VulkanHostInputCacheEntry *)realloc(
      g_vulkan.host_input_cache_entries,
      (size_t)new_capacity * sizeof(VulkanHostInputCacheEntry));
  if (resized == NULL) {
    return 0;
  }
  g_vulkan.host_input_cache_entries = resized;
  g_vulkan.host_input_cache_capacity = new_capacity;
  return 1;
}

static int host_input_cache_find(const void *host_ptr, VkDeviceSize size) {
  for (uint32_t i = 0; i < g_vulkan.host_input_cache_count; ++i) {
    const VulkanHostInputCacheEntry *entry = &g_vulkan.host_input_cache_entries[i];
    if (entry->host_ptr == host_ptr && entry->size == size) {
      return (int)i;
    }
  }
  return -1;
}

static int scope_has_host_cache_reference(const void *host_ptr,
                                          VkDeviceSize size) {
  for (uint32_t i = 0; i < g_vulkan.scope_entry_count; ++i) {
    const VulkanScopeBufferEntry *entry = &g_vulkan.scope_entries[i];
    if (entry->from_host_cache && entry->host_ptr == host_ptr &&
        entry->size == size) {
      return 1;
    }
  }
  return 0;
}

static void host_input_cache_remove_at(uint32_t index, int destroy_buffer_entry) {
  if (index >= g_vulkan.host_input_cache_count) {
    return;
  }
  if (destroy_buffer_entry) {
    destroy_buffer(&g_vulkan.host_input_cache_entries[index].buffer);



  }
  const uint32_t last = g_vulkan.host_input_cache_count - 1u;
  if (index != last) {
    g_vulkan.host_input_cache_entries[index] =
        g_vulkan.host_input_cache_entries[last];
  }
  g_vulkan.host_input_cache_count--;
}

static int host_input_cache_make_room(void) {
  if (g_vulkan.host_input_cache_count < Neuron_VK_HOST_INPUT_CACHE_MAX_COUNT) {
    return host_input_cache_reserve(g_vulkan.host_input_cache_count + 1u);
  }

  int found = 0;
  uint32_t lru_index = 0;
  uint64_t lru_tick = UINT64_MAX;
  for (uint32_t i = 0; i < g_vulkan.host_input_cache_count; ++i) {
    const VulkanHostInputCacheEntry *entry = &g_vulkan.host_input_cache_entries[i];
    if (scope_has_host_cache_reference(entry->host_ptr, entry->size)) {
      continue;
    }
    if (!found || entry->last_use_tick < lru_tick) {
      found = 1;
      lru_tick = entry->last_use_tick;
      lru_index = i;
    }
  }
  if (!found) {
    return 0;
  }
  host_input_cache_remove_at(lru_index, 1);
  return host_input_cache_reserve(g_vulkan.host_input_cache_count + 1u);
}

static int host_input_cache_insert(const void *host_ptr, VkDeviceSize size,
                                   uint64_t checksum, const VulkanBuffer *buffer) {
  if (host_ptr == NULL || size == 0 || buffer == NULL ||
      buffer->buffer == VK_NULL_HANDLE || buffer->memory == VK_NULL_HANDLE) {
    return 0;
  }
  if (!host_input_cache_make_room()) {
    return 0;
  }
  VulkanHostInputCacheEntry *entry =
      &g_vulkan.host_input_cache_entries[g_vulkan.host_input_cache_count];
  entry->host_ptr = host_ptr;
  entry->size = size;
  entry->checksum = checksum;
  entry->last_use_tick = ++g_vulkan.host_input_cache_tick;
  entry->buffer = *buffer;
  g_vulkan.host_input_cache_count++;
  return 1;
}

static void host_input_cache_invalidate(const void *host_ptr, VkDeviceSize size) {
  const int index = host_input_cache_find(host_ptr, size);
  if (index < 0) {
    return;
  }

  VulkanBuffer detached = g_vulkan.host_input_cache_entries[index].buffer;
  const int has_scope_refs = scope_has_host_cache_reference(host_ptr, size);
  host_input_cache_remove_at((uint32_t)index, has_scope_refs ? 0 : 1);

  if (!has_scope_refs) {
    return;
  }

  for (uint32_t i = 0; i < g_vulkan.scope_entry_count; ++i) {
    VulkanScopeBufferEntry *entry = &g_vulkan.scope_entries[i];
    if (entry->from_host_cache && entry->host_ptr == host_ptr &&
        entry->size == size) {
      entry->from_host_cache = 0;
      entry->buffer = detached;
      return;
    }
  }

  destroy_buffer(&detached);
}

static void host_input_cache_destroy_all(void) {
  if (g_vulkan.host_input_cache_entries != NULL) {
    for (uint32_t i = 0; i < g_vulkan.host_input_cache_count; ++i) {
      destroy_buffer(&g_vulkan.host_input_cache_entries[i].buffer);
    }
    free(g_vulkan.host_input_cache_entries);
  }
  g_vulkan.host_input_cache_entries = NULL;
  g_vulkan.host_input_cache_count = 0;
  g_vulkan.host_input_cache_capacity = 0;
  g_vulkan.host_input_cache_tick = 0;
}

static int scope_upload_ring_reserve(uint32_t needed) {
  if (needed <= g_vulkan.scope_upload_ring_capacity) {
    return 1;
  }
  uint32_t new_capacity =
      g_vulkan.scope_upload_ring_capacity > 0 ? g_vulkan.scope_upload_ring_capacity
                                              : 4u;
  while (new_capacity < needed) {
    if (new_capacity > (UINT32_MAX / 2u)) {
      new_capacity = needed;
      break;
    }
    new_capacity *= 2u;
  }

  VulkanUploadRingBuffer *resized = (VulkanUploadRingBuffer *)realloc(
      g_vulkan.scope_upload_ring,
      (size_t)new_capacity * sizeof(VulkanUploadRingBuffer));
  if (resized == NULL) {
    return 0;
  }
  g_vulkan.scope_upload_ring = resized;
  g_vulkan.scope_upload_ring_capacity = new_capacity;
  return 1;
}

static void scope_upload_ring_reset(void) {
  g_vulkan.scope_upload_ring_cursor = 0;
  for (uint32_t i = 0; i < g_vulkan.scope_upload_ring_count; ++i) {
    g_vulkan.scope_upload_ring[i].write_offset = 0;
  }
}

static void scope_upload_ring_destroy_all(void) {
  if (g_vulkan.scope_upload_ring != NULL) {
    for (uint32_t i = 0; i < g_vulkan.scope_upload_ring_count; ++i) {
      destroy_buffer(&g_vulkan.scope_upload_ring[i].buffer);
      g_vulkan.scope_upload_ring[i].size = 0;
      g_vulkan.scope_upload_ring[i].write_offset = 0;
    }
    free(g_vulkan.scope_upload_ring);
  }
  g_vulkan.scope_upload_ring = NULL;
  g_vulkan.scope_upload_ring_count = 0;
  g_vulkan.scope_upload_ring_capacity = 0;
  g_vulkan.scope_upload_ring_cursor = 0;
}

static int scope_upload_ring_acquire(VkDeviceSize size, VulkanBuffer **out_buffer,
                                     VkDeviceSize *out_offset) {
  if (out_buffer == NULL || out_offset == NULL || size == 0) {
    return 0;
  }
  if (g_vulkan.scope_upload_ring_chunk_size == 0) {
    g_vulkan.scope_upload_ring_chunk_size = (VkDeviceSize)Neuron_VK_UPLOAD_RING_DEFAULT_CHUNK;
  }

  for (uint32_t pass = 0; pass < 2u; ++pass) {
    for (uint32_t i = 0; i < g_vulkan.scope_upload_ring_count; ++i) {
      const uint32_t idx =
          (g_vulkan.scope_upload_ring_cursor + i) % g_vulkan.scope_upload_ring_count;
      VulkanUploadRingBuffer *ring = &g_vulkan.scope_upload_ring[idx];
      const VkDeviceSize aligned =
          (ring->write_offset + (VkDeviceSize)(Neuron_VK_UPLOAD_ALIGNMENT - 1u)) &
          ~((VkDeviceSize)Neuron_VK_UPLOAD_ALIGNMENT - 1u);
      if (aligned <= ring->size && size <= ring->size - aligned) {
        ring->write_offset = aligned + size;
        g_vulkan.scope_upload_ring_cursor = idx;
        *out_buffer = &ring->buffer;
        *out_offset = aligned;
        return 1;
      }
    }

    if (pass == 0) {
      const VkDeviceSize alloc_size =
          size > g_vulkan.scope_upload_ring_chunk_size
              ? size
              : g_vulkan.scope_upload_ring_chunk_size;
      if (!scope_upload_ring_reserve(g_vulkan.scope_upload_ring_count + 1u)) {
        return 0;
      }
      VulkanUploadRingBuffer *ring =
          &g_vulkan.scope_upload_ring[g_vulkan.scope_upload_ring_count];
      ring->buffer.buffer = VK_NULL_HANDLE;
      ring->buffer.memory = VK_NULL_HANDLE;
      ring->buffer.mapped_host = NULL;
      ring->size = 0;
      ring->write_offset = 0;
      if (!create_host_visible_buffer(alloc_size, Neuron_VK_USAGE_STAGING_SRC,
                                      &ring->buffer)) {
        return 0;
      }
      ring->size = alloc_size;
      g_vulkan.scope_upload_ring_count++;
    }
  }
  return 0;
}

static int scope_upload_host_to_device(const void *source, VkDeviceSize size,
                                       const VulkanBuffer *destination,
                                       const char *label) {
  if (!scope_batch_active() || source == NULL || size == 0 ||
      destination == NULL || destination->buffer == VK_NULL_HANDLE) {
    set_errorf("Invalid Vulkan scope upload request for %s",
               label != NULL ? label : "upload");
    return 0;
  }
  if (g_vulkan.vkCmdCopyBuffer == NULL || g_vulkan.vkCmdPipelineBarrier == NULL) {
    set_errorf("Missing Vulkan transfer functions for %s",
               label != NULL ? label : "upload");
    return 0;
  }

  VulkanBuffer *staging = NULL;
  VkDeviceSize staging_offset = 0;
  if (!scope_upload_ring_acquire(size, &staging, &staging_offset) ||
      staging == NULL || staging->mapped_host == NULL) {
    set_errorf("Failed to acquire Vulkan upload staging for %s",
               label != NULL ? label : "upload");
    return 0;
  }

  memcpy(((uint8_t *)staging->mapped_host) + staging_offset, source, (size_t)size);

  VkBufferCopy copy = {0};
  copy.srcOffset = staging_offset;
  copy.dstOffset = 0;
  copy.size = size;
  g_vulkan.vkCmdCopyBuffer(g_vulkan.scope_command_buffer, staging->buffer,
                           destination->buffer, 1, &copy);

  VkMemoryBarrier barrier = {0};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  g_vulkan.vkCmdPipelineBarrier(
      g_vulkan.scope_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, NULL, 0, NULL);
  g_vulkan.scope_metric_barrier_count++;
  return 1;
}

static void scope_reset_pending_binary(void) {



  memset(&g_vulkan.scope_pending_binary, 0, sizeof(g_vulkan.scope_pending_binary));
}

static void scope_reset_pending_binary_chain(void) {
  memset(&g_vulkan.scope_pending_binary_chain, 0,
         sizeof(g_vulkan.scope_pending_binary_chain));
}

static void scope_reset_pending_fusion_state(void) {
  scope_reset_pending_binary();
  scope_reset_pending_binary_chain();
}

static int scope_flush_pending_binary(void) {
  if (!g_vulkan.scope_pending_binary.active) {
    return 1;
  }

  const VulkanPendingBinaryOp pending = g_vulkan.scope_pending_binary;
  VulkanBuffer buffers[3] = {pending.lhs_buffer, pending.rhs_buffer,
                             pending.out_buffer};
  struct {
    uint32_t n;
    uint32_t op;
  } push = {pending.element_count, pending.opcode};
  const uint32_t group_x = (pending.element_count + 255u) / 256u;

  if (!dispatch_compute(g_vulkan.binary_pipeline, g_vulkan.binary_pipeline_layout,
                        g_vulkan.binary_descriptor_set_layout, buffers, 3, &push,
                        sizeof(push), group_x, 1u, 1u)) {
    return 0;
  }

  scope_reset_pending_binary();
  return 1;
}

static int scope_flush_pending_binary_chain(void) {
  if (!g_vulkan.scope_pending_binary_chain.active) {
    return 1;
  }

  const VulkanPendingBinaryChainOp pending = g_vulkan.scope_pending_binary_chain;
  const uint32_t group_x = (pending.element_count + 255u) / 256u;

  if (g_vulkan.binary_chain_pipeline != VK_NULL_HANDLE &&
      g_vulkan.binary_chain_pipeline_layout != VK_NULL_HANDLE &&
      g_vulkan.binary_chain_descriptor_set_layout != VK_NULL_HANDLE) {
    VulkanBuffer buffers[5] = {pending.first_lhs_buffer, pending.first_rhs_buffer,
                               pending.second_other_buffer, pending.tmp_buffer,
                               pending.mid_buffer};
    struct {
      uint32_t n;
      uint32_t op0;
      uint32_t op1;
      uint32_t temp_is_lhs;
    } push = {pending.element_count, pending.first_opcode, pending.second_opcode,
              pending.temp_is_lhs};

    if (!dispatch_compute(g_vulkan.binary_chain_pipeline,
                          g_vulkan.binary_chain_pipeline_layout,
                          g_vulkan.binary_chain_descriptor_set_layout, buffers, 5,
                          &push, sizeof(push), group_x, 1u, 1u)) {
      return 0;
    }
    scope_reset_pending_binary_chain();
    return 1;
  }

  {
    struct {
      uint32_t n;
      uint32_t op;
    } first_push = {pending.element_count, pending.first_opcode};
    VulkanBuffer first_buffers[3] = {pending.first_lhs_buffer,
                                     pending.first_rhs_buffer, pending.tmp_buffer};
    if (!dispatch_compute(g_vulkan.binary_pipeline, g_vulkan.binary_pipeline_layout,
                          g_vulkan.binary_descriptor_set_layout, first_buffers, 3,
                          &first_push, sizeof(first_push), group_x, 1u, 1u)) {
      return 0;
    }
  }

  {
    struct {
      uint32_t n;
      uint32_t op;
    } second_push = {pending.element_count, pending.second_opcode};
    VulkanBuffer second_buffers[3] = {
        pending.temp_is_lhs ? pending.tmp_buffer : pending.second_other_buffer,
        pending.temp_is_lhs ? pending.second_other_buffer : pending.tmp_buffer,
        pending.mid_buffer};
    if (!dispatch_compute(g_vulkan.binary_pipeline, g_vulkan.binary_pipeline_layout,
                          g_vulkan.binary_descriptor_set_layout, second_buffers, 3,
                          &second_push, sizeof(second_push), group_x, 1u, 1u)) {
      return 0;
    }
  }

  scope_reset_pending_binary_chain();
  return 1;
}

static int scope_flush_pending_fusions(void) {
  if (!scope_flush_pending_binary_chain()) {
    return 0;
  }
  if (!scope_flush_pending_binary()) {
    return 0;
  }
  return 1;
}

static int scope_dispatch_fused_binary_then_fma(
    const VulkanPendingBinaryOp *pending, const VulkanBuffer *fma_b_buffer,
    const VulkanBuffer *fma_c_buffer, const VulkanBuffer *fma_out_buffer) {
  if (pending == NULL || fma_b_buffer == NULL || fma_c_buffer == NULL ||
      fma_out_buffer == NULL) {
    return -1;
  }
  if (g_vulkan.binary_then_fma_pipeline == VK_NULL_HANDLE ||
      g_vulkan.binary_then_fma_pipeline_layout == VK_NULL_HANDLE ||
      g_vulkan.binary_then_fma_descriptor_set_layout == VK_NULL_HANDLE) {
    return 0;
  }

  VulkanBuffer buffers[6] = {pending->lhs_buffer, pending->rhs_buffer,
                             *fma_b_buffer,      *fma_c_buffer,
                             pending->out_buffer, *fma_out_buffer};
  struct {
    uint32_t n;
    uint32_t op0;
  } push = {pending->element_count, pending->opcode};

  const uint32_t group_x = (pending->element_count + 255u) / 256u;
  if (!dispatch_compute(g_vulkan.binary_then_fma_pipeline,
                        g_vulkan.binary_then_fma_pipeline_layout,
                        g_vulkan.binary_then_fma_descriptor_set_layout, buffers,
                        6, &push, sizeof(push), group_x, 1u, 1u)) {
    return -1;
  }
  return 1;
}

static int scope_dispatch_fused_binary_chain_then_fma(
    const VulkanPendingBinaryChainOp *pending, const VulkanBuffer *fma_b_buffer,
    const VulkanBuffer *fma_c_buffer, const VulkanBuffer *fma_out_buffer) {
  if (pending == NULL || fma_b_buffer == NULL || fma_c_buffer == NULL ||
      fma_out_buffer == NULL) {
    return -1;
  }
  if (g_vulkan.binary_chain_then_fma_pipeline == VK_NULL_HANDLE ||
      g_vulkan.binary_chain_then_fma_pipeline_layout == VK_NULL_HANDLE ||
      g_vulkan.binary_chain_then_fma_descriptor_set_layout == VK_NULL_HANDLE) {
    return 0;
  }

  VulkanBuffer buffers[8] = {
      pending->first_lhs_buffer, pending->first_rhs_buffer,
      pending->second_other_buffer, *fma_b_buffer,
      *fma_c_buffer,             pending->tmp_buffer,
      pending->mid_buffer,       *fma_out_buffer};
  struct {
    uint32_t n;
    uint32_t op0;
    uint32_t op1;
    uint32_t temp_is_lhs;
  } push = {pending->element_count, pending->first_opcode, pending->second_opcode,
            pending->temp_is_lhs};

  const uint32_t group_x = (pending->element_count + 255u) / 256u;
  if (!dispatch_compute(g_vulkan.binary_chain_then_fma_pipeline,
                        g_vulkan.binary_chain_then_fma_pipeline_layout,
                        g_vulkan.binary_chain_then_fma_descriptor_set_layout,
                        buffers, 8, &push, sizeof(push), group_x, 1u, 1u)) {
    return -1;
  }
  return 1;
}

static void scope_destroy_tracking_buffers(void) {
  scope_reset_pending_fusion_state();

  if (g_vulkan.scope_entries != NULL) {
    for (uint32_t i = 0; i < g_vulkan.scope_entry_count; ++i) {
      if (!g_vulkan.scope_entries[i].from_host_cache) {
        scope_pool_release(&g_vulkan.scope_entries[i].buffer,
                           g_vulkan.scope_entries[i].size, Neuron_VK_USAGE_STORAGE,
                           Neuron_VK_MEMORY_DEVICE_LOCAL);
      }
      g_vulkan.scope_entries[i].host_ptr = NULL;
      g_vulkan.scope_entries[i].size = 0;
      g_vulkan.scope_entries[i].dirty_output = 0;
      g_vulkan.scope_entries[i].from_host_cache = 0;
      g_vulkan.scope_entries[i].consumed_after_write = 0;
    }
  }
  g_vulkan.scope_entry_count = 0;

  if (g_vulkan.scope_transient_buffers != NULL) {
    for (uint32_t i = 0; i < g_vulkan.scope_transient_count; ++i) {
      scope_pool_release(&g_vulkan.scope_transient_buffers[i].buffer,
                         g_vulkan.scope_transient_buffers[i].size,
                         g_vulkan.scope_transient_buffers[i].usage,
                         Neuron_VK_MEMORY_DEVICE_LOCAL);
      g_vulkan.scope_transient_buffers[i].size = 0;
      g_vulkan.scope_transient_buffers[i].usage = 0;
    }
  }
  g_vulkan.scope_transient_count = 0;

  if (g_vulkan.scope_has_zero_buffer) {
    scope_pool_release(&g_vulkan.scope_zero_buffer, sizeof(uint32_t),
                       Neuron_VK_USAGE_STORAGE, Neuron_VK_MEMORY_DEVICE_LOCAL);
  }
  g_vulkan.scope_zero_buffer.buffer = VK_NULL_HANDLE;
  g_vulkan.scope_zero_buffer.memory = VK_NULL_HANDLE;
  g_vulkan.scope_has_zero_buffer = 0;
}

static void scope_release_recording_resources(void) {
  if (g_vulkan.scope_command_buffer != VK_NULL_HANDLE &&
      g_vulkan.vkFreeCommandBuffers != NULL && g_vulkan.device != VK_NULL_HANDLE &&
      g_vulkan.command_pool != VK_NULL_HANDLE) {
    g_vulkan.vkFreeCommandBuffers(g_vulkan.device, g_vulkan.command_pool, 1,
                                  &g_vulkan.scope_command_buffer);
  }
  g_vulkan.scope_command_buffer = VK_NULL_HANDLE;

  if (g_vulkan.scope_descriptor_pool != VK_NULL_HANDLE &&
      g_vulkan.vkDestroyDescriptorPool != NULL &&
      g_vulkan.device != VK_NULL_HANDLE) {
    g_vulkan.vkDestroyDescriptorPool(g_vulkan.device, g_vulkan.scope_descriptor_pool,
                                     NULL);
  }
  g_vulkan.scope_descriptor_pool = VK_NULL_HANDLE;
  g_vulkan.scope_descriptor_update_count = 0;
  g_vulkan.scope_has_recorded_work = 0;
  scope_reset_pending_fusion_state();
}




static int scope_reserve_entry_capacity(uint32_t needed) {
  if (needed <= g_vulkan.scope_entry_capacity) {
    return 1;
  }
  uint32_t new_capacity =
      g_vulkan.scope_entry_capacity > 0 ? g_vulkan.scope_entry_capacity : 16u;
  while (new_capacity < needed) {
    if (new_capacity > (UINT32_MAX / 2u)) {
      new_capacity = needed;
      break;
    }
    new_capacity *= 2u;
  }
  VulkanScopeBufferEntry *resized = (VulkanScopeBufferEntry *)realloc(
      g_vulkan.scope_entries, (size_t)new_capacity * sizeof(VulkanScopeBufferEntry));
  if (resized == NULL) {
    set_errorf("Failed to grow Vulkan scope entry table");
    return 0;
  }
  g_vulkan.scope_entries = resized;
  g_vulkan.scope_entry_capacity = new_capacity;
  return 1;
}

static int scope_reserve_transient_capacity(uint32_t needed) {
  if (needed <= g_vulkan.scope_transient_capacity) {
    return 1;
  }
  uint32_t new_capacity = g_vulkan.scope_transient_capacity > 0
                              ? g_vulkan.scope_transient_capacity
                              : 32u;
  while (new_capacity < needed) {
    if (new_capacity > (UINT32_MAX / 2u)) {
      new_capacity = needed;
      break;
    }
    new_capacity *= 2u;
  }
  VulkanTransientBufferEntry *resized = (VulkanTransientBufferEntry *)realloc(
      g_vulkan.scope_transient_buffers,
      (size_t)new_capacity * sizeof(VulkanTransientBufferEntry));
  if (resized == NULL) {
    set_errorf("Failed to grow Vulkan scope transient buffer table");
    return 0;
  }
  g_vulkan.scope_transient_buffers = resized;
  g_vulkan.scope_transient_capacity = new_capacity;
  return 1;
}

static int scope_reserve_descriptor_update_capacity(uint32_t needed) {
  if (needed <= g_vulkan.scope_descriptor_update_capacity) {
    return 1;
  }
  uint32_t new_capacity = g_vulkan.scope_descriptor_update_capacity > 0
                              ? g_vulkan.scope_descriptor_update_capacity
                              : 64u;
  while (new_capacity < needed) {
    if (new_capacity > (UINT32_MAX / 2u)) {
      new_capacity = needed;
      break;
    }
    new_capacity *= 2u;
  }
  VulkanScopeDescriptorUpdate *resized = (VulkanScopeDescriptorUpdate *)realloc(
      g_vulkan.scope_descriptor_updates,
      (size_t)new_capacity * sizeof(VulkanScopeDescriptorUpdate));
  if (resized == NULL) {
    set_errorf("Failed to grow Vulkan scope descriptor update table");
    return 0;
  }
  g_vulkan.scope_descriptor_updates = resized;
  g_vulkan.scope_descriptor_update_capacity = new_capacity;
  return 1;
}

static int scope_track_descriptor_update(VkDescriptorSet descriptor_set,
                                         const VulkanBuffer *buffers,
                                         uint32_t buffer_count) {
  if (descriptor_set == VK_NULL_HANDLE || buffers == NULL || buffer_count == 0 ||
      buffer_count > Neuron_VK_MAX_DISPATCH_BUFFERS) {
    set_errorf("Invalid Vulkan scoped descriptor update");
    return 0;
  }
  if (!scope_reserve_descriptor_update_capacity(
          g_vulkan.scope_descriptor_update_count + 1u)) {
    return 0;
  }

  VulkanScopeDescriptorUpdate *entry =
      &g_vulkan.scope_descriptor_updates[g_vulkan.scope_descriptor_update_count];
  entry->set = descriptor_set;
  entry->buffer_count = buffer_count;
  for (uint32_t i = 0; i < buffer_count; ++i) {
    entry->buffers[i] = buffers[i].buffer;
  }
  g_vulkan.scope_descriptor_update_count++;
  g_vulkan.scope_metric_descriptor_writes += (uint64_t)buffer_count;
  return 1;
}

static int scope_apply_descriptor_updates(void) {
  if (g_vulkan.scope_descriptor_update_count == 0) {
    return 1;
  }
  if (g_vulkan.vkUpdateDescriptorSets == NULL) {
    set_errorf("Missing Vulkan function: vkUpdateDescriptorSets");
    return 0;
  }

  uint32_t total_writes = 0;
  for (uint32_t i = 0; i < g_vulkan.scope_descriptor_update_count; ++i) {
    total_writes += g_vulkan.scope_descriptor_updates[i].buffer_count;
  }
  if (total_writes == 0) {
    return 1;
  }

  VkDescriptorBufferInfo *infos =
      (VkDescriptorBufferInfo *)malloc((size_t)total_writes *
                                       sizeof(VkDescriptorBufferInfo));
  VkWriteDescriptorSet *writes = (VkWriteDescriptorSet *)malloc(
      (size_t)total_writes * sizeof(VkWriteDescriptorSet));
  if (infos == NULL || writes == NULL) {
    if (infos != NULL) {
      free(infos);
    }
    if (writes != NULL) {
      free(writes);
    }
    set_errorf("Failed to allocate Vulkan scoped descriptor write tables");
    return 0;
  }

  uint32_t cursor = 0;
  for (uint32_t i = 0; i < g_vulkan.scope_descriptor_update_count; ++i) {
    const VulkanScopeDescriptorUpdate *entry =
        &g_vulkan.scope_descriptor_updates[i];
    for (uint32_t binding = 0; binding < entry->buffer_count; ++binding) {
      infos[cursor].buffer = entry->buffers[binding];
      infos[cursor].offset = 0;
      infos[cursor].range = VK_WHOLE_SIZE;

      writes[cursor].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[cursor].pNext = NULL;
      writes[cursor].dstSet = entry->set;
      writes[cursor].dstBinding = binding;
      writes[cursor].dstArrayElement = 0;
      writes[cursor].descriptorCount = 1;
      writes[cursor].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writes[cursor].pImageInfo = NULL;
      writes[cursor].pBufferInfo = &infos[cursor];
      writes[cursor].pTexelBufferView = NULL;
      cursor++;
    }
  }

  g_vulkan.vkUpdateDescriptorSets(g_vulkan.device, cursor, writes, 0, NULL);
  free(writes);
  free(infos);
  return 1;
}

static VulkanScopeBufferEntry *scope_find_entry(const void *host_ptr,
                                                VkDeviceSize size) {
  if (host_ptr == NULL || size == 0) {
    return NULL;
  }
  for (uint32_t i = 0; i < g_vulkan.scope_entry_count; ++i) {
    VulkanScopeBufferEntry *entry = &g_vulkan.scope_entries[i];
    if (entry->host_ptr == host_ptr && entry->size == size) {
      return entry;
    }
  }
  return NULL;
}

static int scope_track_transient(const VulkanBuffer *buffer, VkDeviceSize size,
                                 VkBufferUsageFlags usage) {
  if (buffer == NULL || buffer->buffer == VK_NULL_HANDLE ||
      buffer->memory == VK_NULL_HANDLE || size == 0) {
    return 0;
  }
  if (!scope_reserve_transient_capacity(g_vulkan.scope_transient_count + 1u)) {
    return 0;
  }
  VulkanTransientBufferEntry *entry =
      &g_vulkan.scope_transient_buffers[g_vulkan.scope_transient_count];
  entry->buffer = *buffer;
  entry->size = size;
  entry->usage = usage;
  g_vulkan.scope_transient_count++;
  return 1;
}

static int scope_track_entry(void *host_ptr, VkDeviceSize size,
                             const VulkanBuffer *buffer, int dirty_output,
                             int from_host_cache) {
  if (host_ptr == NULL || size == 0 || buffer == NULL ||
      buffer->buffer == VK_NULL_HANDLE || buffer->memory == VK_NULL_HANDLE) {
    return 0;
  }

  VulkanScopeBufferEntry *existing = scope_find_entry(host_ptr, size);
  if (existing != NULL) {
    existing->dirty_output = dirty_output ? 1 : existing->dirty_output;
    existing->buffer = *buffer;
    existing->from_host_cache =
        from_host_cache ? 1 : (dirty_output ? 0 : existing->from_host_cache);
    if (dirty_output) {
      existing->consumed_after_write = 0;
    }
    return 1;
  }

  if (!scope_reserve_entry_capacity(g_vulkan.scope_entry_count + 1u)) {
    return 0;
  }
  VulkanScopeBufferEntry *entry = &g_vulkan.scope_entries[g_vulkan.scope_entry_count];
  entry->host_ptr = host_ptr;
  entry->size = size;
  entry->dirty_output = dirty_output ? 1 : 0;
  entry->from_host_cache = from_host_cache ? 1 : 0;
  entry->consumed_after_write = 0;
  entry->buffer = *buffer;
  g_vulkan.scope_entry_count++;
  return 1;
}

static int scope_get_zero_buffer(VulkanBuffer *out_buffer, const char *label) {
  if (out_buffer == NULL) {
    return 0;
  }
  if (!g_vulkan.scope_has_zero_buffer) {
    VulkanBuffer zero = {0};
    uint32_t zero_value = 0;
    if ((!scope_pool_take(sizeof(uint32_t), Neuron_VK_USAGE_STORAGE,
                          Neuron_VK_MEMORY_DEVICE_LOCAL, &zero) &&
         !create_device_local_storage_buffer(sizeof(uint32_t), &zero)) ||



        !scope_upload_host_to_device(&zero_value, sizeof(uint32_t), &zero,
                                     label)) {
      set_errorf("Failed to create Vulkan scope zero buffer for %s", label);
      destroy_buffer(&zero);
      return 0;
    }
    g_vulkan.scope_zero_buffer = zero;
    g_vulkan.scope_has_zero_buffer = 1;
  }
  *out_buffer = g_vulkan.scope_zero_buffer;
  return 1;
}

static int scope_resolve_input_buffer(const void *host_ptr, VkDeviceSize size,
                                      int cacheable, VulkanBuffer *out_buffer,
                                      const char *label) {
  if (host_ptr == NULL || size == 0 || out_buffer == NULL) {
    return 0;
  }

  VulkanScopeBufferEntry *entry = scope_find_entry(host_ptr, size);
  if (entry != NULL) {
    if (entry->dirty_output) {
      entry->consumed_after_write = 1;
    }
    *out_buffer = entry->buffer;
    return 1;
  }

  uint64_t checksum = 0;
  int checksum_ready = 0;
  if (cacheable && g_vulkan.host_input_cache_enabled) {
    const int cache_index = host_input_cache_find(host_ptr, size);
    if (cache_index >= 0) {
      VulkanHostInputCacheEntry *cache_entry =
          &g_vulkan.host_input_cache_entries[(uint32_t)cache_index];
      if (g_vulkan.host_input_cache_verify) {
        checksum = compute_host_checksum(host_ptr, size);
        checksum_ready = 1;
        if (cache_entry->checksum != checksum &&
            !scope_upload_host_to_device(host_ptr, size, &cache_entry->buffer,
                                         label)) {
          set_errorf("Failed to refresh Vulkan scope cached input for %s", label);
          return 0;
        }
        cache_entry->checksum = checksum;
      }
      cache_entry->last_use_tick = ++g_vulkan.host_input_cache_tick;
      if (!scope_track_entry((void *)host_ptr, size, &cache_entry->buffer, 0, 1)) {
        return 0;
      }
      *out_buffer = cache_entry->buffer;
      return 1;
    }
  }

  VulkanBuffer uploaded = {0};
  if ((!scope_pool_take(size, Neuron_VK_USAGE_STORAGE, Neuron_VK_MEMORY_DEVICE_LOCAL,
                        &uploaded) &&
       !create_device_local_storage_buffer(size, &uploaded)) ||
      !scope_upload_host_to_device(host_ptr, size, &uploaded, label)) {
    set_errorf("Failed to stage Vulkan scope input buffer for %s", label);
    destroy_buffer(&uploaded);
    return 0;
  }
  if (cacheable) {
    int from_host_cache = 0;
    if (g_vulkan.host_input_cache_enabled) {
      if (g_vulkan.host_input_cache_verify && !checksum_ready) {
        checksum = compute_host_checksum(host_ptr, size);
        checksum_ready = 1;
      }
      if (host_input_cache_insert(
              host_ptr, size, g_vulkan.host_input_cache_verify ? checksum : 0,
              &uploaded)) {
        from_host_cache = 1;
      }
    }
    if (!scope_track_entry((void *)host_ptr, size, &uploaded, 0,
                           from_host_cache)) {
      if (from_host_cache) {
        host_input_cache_invalidate(host_ptr, size);
      } else {
        destroy_buffer(&uploaded);
      }
      return 0;
    }
  } else {
    if (!scope_track_transient(&uploaded, size, Neuron_VK_USAGE_STORAGE)) {
      destroy_buffer(&uploaded);
      return 0;
    }
  }
  *out_buffer = uploaded;
  return 1;
}

static int scope_resolve_output_buffer(void *host_ptr, VkDeviceSize size,
                                       int initialize_from_host,
                                       VulkanBuffer *out_buffer,
                                       const char *label) {
  if (host_ptr == NULL || size == 0 || out_buffer == NULL) {
    return 0;
  }

  VulkanScopeBufferEntry *entry = scope_find_entry(host_ptr, size);
  if (entry != NULL) {
    if (entry->from_host_cache) {
      host_input_cache_invalidate(host_ptr, size);
      entry = scope_find_entry(host_ptr, size);
    }
    if (entry == NULL) {
      set_errorf("Internal error: Vulkan scope output entry lost for %s", label);
      return 0;
    }
    if (initialize_from_host &&
        !scope_upload_host_to_device(host_ptr, size, &entry->buffer, label)) {
      set_errorf("Failed to refresh Vulkan scope output buffer for %s", label);
      return 0;
    }
    entry->dirty_output = 1;
    entry->from_host_cache = 0;
    entry->consumed_after_write = 0;
    *out_buffer = entry->buffer;
    return 1;
  }

  host_input_cache_invalidate(host_ptr, size);

  VulkanBuffer output = {0};
  if (!scope_pool_take(size, Neuron_VK_USAGE_STORAGE, Neuron_VK_MEMORY_DEVICE_LOCAL,
                       &output) &&
      !create_device_local_storage_buffer(size, &output)) {
    set_errorf("Failed to allocate Vulkan scope output buffer for %s", label);
    return 0;
  }
  if (initialize_from_host &&
      !scope_upload_host_to_device(host_ptr, size, &output, label)) {
    set_errorf("Failed to upload initial Vulkan scope output for %s", label);
    destroy_buffer(&output);
    return 0;
  }
  if (!scope_track_entry(host_ptr, size, &output, 1, 0)) {
    destroy_buffer(&output);
    return 0;
  }
  *out_buffer = output;
  return 1;
}

static int scope_begin_recording(void) {
  scope_upload_ring_reset();
  scope_reset_pending_fusion_state();

  VkDescriptorPoolSize pool_size = {0};
  pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_size.descriptorCount = Neuron_VK_SCOPE_MAX_DESCRIPTORS;

  VkDescriptorPoolCreateInfo pool_info = {0};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = Neuron_VK_SCOPE_MAX_SETS;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  if (g_vulkan.vkCreateDescriptorPool(g_vulkan.device, &pool_info, NULL,
                                      &g_vulkan.scope_descriptor_pool) !=
      VK_SUCCESS) {
    set_errorf("vkCreateDescriptorPool failed for Vulkan scope batching");
    return 0;
  }

  VkCommandBufferAllocateInfo command_buffer_info = {0};
  command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_info.commandPool = g_vulkan.command_pool;
  command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_info.commandBufferCount = 1;
  if (g_vulkan.vkAllocateCommandBuffers(g_vulkan.device, &command_buffer_info,
                                        &g_vulkan.scope_command_buffer) !=
      VK_SUCCESS) {
    set_errorf("vkAllocateCommandBuffers failed for Vulkan scope batching");
    scope_release_recording_resources();
    return 0;
  }

  VkCommandBufferBeginInfo begin_info = {0};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (g_vulkan.vkBeginCommandBuffer(g_vulkan.scope_command_buffer, &begin_info) !=
      VK_SUCCESS) {
    set_errorf("vkBeginCommandBuffer failed for Vulkan scope batching");
    scope_release_recording_resources();
    return 0;
  }

  g_vulkan.scope_has_recorded_work = 0;
  return 1;
}

static int submit_and_wait_with_fence(VkCommandBuffer command_buffer,
                                      const char *context_label) {
  if (command_buffer == VK_NULL_HANDLE) {
    set_errorf("Invalid Vulkan command buffer for submit");
    return 0;
  }
  if (g_vulkan.vkCreateFence == NULL || g_vulkan.vkDestroyFence == NULL ||
      g_vulkan.vkWaitForFences == NULL) {
    set_errorf("Missing Vulkan fence functions for submit synchronization");
    return 0;
  }

  VkFence fence = VK_NULL_HANDLE;
  VkFenceCreateInfo fence_info = {0};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  if (g_vulkan.vkCreateFence(g_vulkan.device, &fence_info, NULL, &fence) !=
      VK_SUCCESS) {
    set_errorf("vkCreateFence failed%s%s", context_label != NULL ? " " : "",
               context_label != NULL ? context_label : "");
    return 0;
  }

  VkSubmitInfo submit_info = {0};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  if (g_vulkan.vkQueueSubmit(g_vulkan.queue, 1, &submit_info, fence) !=
      VK_SUCCESS) {
    set_errorf("vkQueueSubmit failed%s%s", context_label != NULL ? " " : "",
               context_label != NULL ? context_label : "");
    g_vulkan.vkDestroyFence(g_vulkan.device, fence, NULL);
    return 0;
  }

  if (g_vulkan.vkWaitForFences(g_vulkan.device, 1, &fence, VK_TRUE, UINT64_MAX) !=
      VK_SUCCESS) {
    set_errorf("vkWaitForFences failed%s%s", context_label != NULL ? " " : "",
               context_label != NULL ? context_label : "");
    g_vulkan.vkDestroyFence(g_vulkan.device, fence, NULL);
    return 0;
  }

  g_vulkan.vkDestroyFence(g_vulkan.device, fence, NULL);



  return 1;
}

static int scope_flush_and_end_recording(void) {
  typedef struct {
    uint32_t entry_index;
    VkDeviceSize offset;
    VkDeviceSize size;
  } ScopeReadbackRegion;

  int ok = 1;
  ScopeReadbackRegion *regions = NULL;
  uint32_t region_count = 0;
  VkDeviceSize staging_bytes = 0;
  VulkanBuffer staging = {0};

  if (!scope_batch_active()) {
    return 1;
  }

  if (!scope_flush_pending_fusions()) {
    ok = 0;
  }

  for (uint32_t i = 0; ok && i < g_vulkan.scope_entry_count; ++i) {
    const VulkanScopeBufferEntry *entry = &g_vulkan.scope_entries[i];
    if (!entry->dirty_output || entry->size == 0) {
      continue;
    }
    if (g_vulkan.scope_readback_sink_only && entry->consumed_after_write) {
      continue;
    }
    const VkDeviceSize aligned =
        (staging_bytes + (VkDeviceSize)3u) & ~(VkDeviceSize)3u;
    if (aligned < staging_bytes) {
      set_errorf("Vulkan scope readback staging size overflow");
      ok = 0;
      break;
    }
    staging_bytes = aligned;
    if (entry->size > (VkDeviceSize)(UINT64_MAX - staging_bytes)) {
      set_errorf("Vulkan scope readback staging range overflow");
      ok = 0;
      break;
    }
    staging_bytes += entry->size;
    region_count++;
  }

  if (ok && region_count > 0) {
    if (g_vulkan.vkCmdCopyBuffer == NULL) {
      set_errorf("Missing Vulkan function: vkCmdCopyBuffer");
      ok = 0;
    } else {
      regions = (ScopeReadbackRegion *)malloc((size_t)region_count *
                                              sizeof(ScopeReadbackRegion));
      if (regions == NULL) {
        set_errorf("Failed to allocate Vulkan scope readback regions");
        ok = 0;
      }
    }
  }

  if (ok && region_count > 0 &&
      !scope_pool_take(staging_bytes, Neuron_VK_USAGE_STAGING_DST,
                       Neuron_VK_MEMORY_HOST_VISIBLE, &staging) &&
      !create_host_visible_buffer(staging_bytes, Neuron_VK_USAGE_STAGING_DST,
                                  &staging)) {
    ok = 0;
  }

  if (ok && region_count > 0) {
    VkDeviceSize cursor = 0;
    uint32_t region_cursor = 0;
    for (uint32_t i = 0; i < g_vulkan.scope_entry_count; ++i) {
      const VulkanScopeBufferEntry *entry = &g_vulkan.scope_entries[i];
      if (!entry->dirty_output || entry->size == 0) {
        continue;
      }
      if (g_vulkan.scope_readback_sink_only && entry->consumed_after_write) {
        continue;
      }
      cursor = (cursor + (VkDeviceSize)3u) & ~(VkDeviceSize)3u;
      regions[region_cursor].entry_index = i;
      regions[region_cursor].offset = cursor;
      regions[region_cursor].size = entry->size;
      cursor += entry->size;
      region_cursor++;
    }

    if (region_cursor != region_count) {
      set_errorf("Internal Vulkan scope readback region mismatch");
      ok = 0;
    }
  }

  if (ok && region_count > 0 && g_vulkan.vkCmdPipelineBarrier != NULL) {
    VkMemoryBarrier pre_copy_barrier = {0};
    pre_copy_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    pre_copy_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    pre_copy_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    g_vulkan.vkCmdPipelineBarrier(
        g_vulkan.scope_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &pre_copy_barrier, 0, NULL, 0,
        NULL);
    g_vulkan.scope_metric_barrier_count++;
  }

  if (ok && region_count > 0) {
    for (uint32_t i = 0; i < region_count; ++i) {
      const ScopeReadbackRegion *region = &regions[i];
      const VulkanScopeBufferEntry *entry =
          &g_vulkan.scope_entries[region->entry_index];
      VkBufferCopy copy = {0};
      copy.srcOffset = 0;
      copy.dstOffset = region->offset;
      copy.size = region->size;
      g_vulkan.vkCmdCopyBuffer(g_vulkan.scope_command_buffer, entry->buffer.buffer,
                               staging.buffer, 1, &copy);
    }
  }

  if (ok && region_count > 0 && g_vulkan.vkCmdPipelineBarrier != NULL) {
    VkMemoryBarrier post_copy_barrier = {0};
    post_copy_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    post_copy_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    post_copy_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    g_vulkan.vkCmdPipelineBarrier(
        g_vulkan.scope_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &post_copy_barrier, 0, NULL, 0, NULL);
    g_vulkan.scope_metric_barrier_count++;
  }

  if (ok && !scope_apply_descriptor_updates()) {
    ok = 0;
  }

  if (g_vulkan.vkEndCommandBuffer(g_vulkan.scope_command_buffer) != VK_SUCCESS) {
    set_errorf("vkEndCommandBuffer failed while closing Vulkan scope batch");
    ok = 0;
  }

  if (ok && g_vulkan.scope_has_recorded_work) {
    if (!submit_and_wait_with_fence(g_vulkan.scope_command_buffer,
                                    "while executing Vulkan scope batch")) {
      ok = 0;
    }
  }

  if (ok && region_count > 0) {
    g_vulkan.scope_metric_readback_bytes += (uint64_t)staging_bytes;
    if (staging.mapped_host == NULL) {
      set_errorf("Vulkan scope readback staging buffer is not mapped");
      ok = 0;
    } else {
      for (uint32_t i = 0; i < region_count; ++i) {
        const ScopeReadbackRegion *region = &regions[i];
        VulkanScopeBufferEntry *entry = &g_vulkan.scope_entries[region->entry_index];
        memcpy(entry->host_ptr,
               ((const uint8_t *)staging.mapped_host) + region->offset,
               (size_t)region->size);
        entry->dirty_output = 0;
        entry->consumed_after_write = 0;
      }
    }
  } else if (ok) {
    for (uint32_t i = 0; i < g_vulkan.scope_entry_count; ++i) {
      g_vulkan.scope_entries[i].dirty_output = 0;
      g_vulkan.scope_entries[i].consumed_after_write = 0;
    }
  }

  scope_pool_release(&staging, staging_bytes, Neuron_VK_USAGE_STAGING_DST,
                     Neuron_VK_MEMORY_HOST_VISIBLE);
  if (regions != NULL) {
    free(regions);
  }
  scope_release_recording_resources();
  scope_destroy_tracking_buffers();
  /* Keep scope tracking arrays and pooled buffers alive across scopes to
   * amortize repeated Vulkan allocations. They are reclaimed in shutdown. */
  return ok;
}

static void shutdown_internal(void) {
  if (g_vulkan.device != VK_NULL_HANDLE && g_vulkan.vkDeviceWaitIdle != NULL) {
    g_vulkan.vkDeviceWaitIdle(g_vulkan.device);
  }

  scope_release_recording_resources();
  scope_destroy_tracking_buffers();
  scope_pool_destroy_all();
  scope_upload_ring_destroy_all();
  host_input_cache_destroy_all();

  if (g_vulkan.scope_entries != NULL) {
    free(g_vulkan.scope_entries);
    g_vulkan.scope_entries = NULL;
  }
  g_vulkan.scope_entry_capacity = 0;
  g_vulkan.scope_entry_count = 0;

  if (g_vulkan.scope_transient_buffers != NULL) {
    free(g_vulkan.scope_transient_buffers);
    g_vulkan.scope_transient_buffers = NULL;
  }
  g_vulkan.scope_transient_capacity = 0;
  g_vulkan.scope_transient_count = 0;

  if (g_vulkan.scope_descriptor_updates != NULL) {
    free(g_vulkan.scope_descriptor_updates);
    g_vulkan.scope_descriptor_updates = NULL;
  }
  g_vulkan.scope_descriptor_update_capacity = 0;
  g_vulkan.scope_descriptor_update_count = 0;

  destroy_pipeline_bundle(&g_vulkan.binary_pipeline, &g_vulkan.binary_shader_module,
                          &g_vulkan.binary_pipeline_layout,
                          &g_vulkan.binary_descriptor_set_layout);
  destroy_pipeline_bundle(&g_vulkan.binary_chain_pipeline,
                          &g_vulkan.binary_chain_shader_module,
                          &g_vulkan.binary_chain_pipeline_layout,
                          &g_vulkan.binary_chain_descriptor_set_layout);
  destroy_pipeline_bundle(&g_vulkan.binary_then_fma_pipeline,
                          &g_vulkan.binary_then_fma_shader_module,
                          &g_vulkan.binary_then_fma_pipeline_layout,
                          &g_vulkan.binary_then_fma_descriptor_set_layout);
  destroy_pipeline_bundle(&g_vulkan.binary_chain_then_fma_pipeline,
                          &g_vulkan.binary_chain_then_fma_shader_module,
                          &g_vulkan.binary_chain_then_fma_pipeline_layout,
                          &g_vulkan.binary_chain_then_fma_descriptor_set_layout);
  destroy_pipeline_bundle(&g_vulkan.fma_pipeline, &g_vulkan.fma_shader_module,
                          &g_vulkan.fma_pipeline_layout,
                          &g_vulkan.fma_descriptor_set_layout);
  destroy_pipeline_bundle(&g_vulkan.matmul_dense_pipeline,
                          &g_vulkan.matmul_dense_shader_module,
                          &g_vulkan.matmul_dense_pipeline_layout,
                          &g_vulkan.matmul_dense_descriptor_set_layout);
  destroy_pipeline_bundle(&g_vulkan.matmul_packed_pipeline,
                          &g_vulkan.matmul_packed_shader_module,



                          &g_vulkan.matmul_packed_pipeline_layout,
                          &g_vulkan.matmul_packed_descriptor_set_layout);

  if (g_vulkan.command_pool != VK_NULL_HANDLE &&
      g_vulkan.vkDestroyCommandPool != NULL && g_vulkan.device != VK_NULL_HANDLE) {
    g_vulkan.vkDestroyCommandPool(g_vulkan.device, g_vulkan.command_pool, NULL);
    g_vulkan.command_pool = VK_NULL_HANDLE;
  }

  if (g_vulkan.common_context_acquired) {
    neuron_vk_common_release();
    g_vulkan.common_context_acquired = 0;
  }
  g_vulkan.device = VK_NULL_HANDLE;
  g_vulkan.instance = VK_NULL_HANDLE;

  close_vulkan_loader();

  g_vulkan.available = 0;
  g_vulkan.initialized = 0;
  g_vulkan.queue = VK_NULL_HANDLE;
  g_vulkan.physical_device = VK_NULL_HANDLE;
  g_vulkan.queue_family_index = 0;
  g_vulkan.scope_active = 0;
  g_vulkan.scope_batch_enabled = 0;
  g_vulkan.scope_fusion_enabled = 0;
  g_vulkan.scope_metrics_enabled = 0;
  g_vulkan.scope_has_recorded_work = 0;
  g_vulkan.scope_metric_dispatch_count = 0;
  g_vulkan.scope_metric_barrier_count = 0;
  g_vulkan.scope_metric_descriptor_writes = 0;
  g_vulkan.scope_metric_readback_bytes = 0;
  scope_reset_pending_fusion_state();
  memset(&g_vulkan.memory_properties, 0, sizeof(g_vulkan.memory_properties));
}

static uint32_t find_memory_type(uint32_t type_mask,
                                 VkMemoryPropertyFlags required_flags) {
  for (uint32_t i = 0; i < g_vulkan.memory_properties.memoryTypeCount; ++i) {
    const int bit_match = (type_mask & (1u << i)) != 0;
    const VkMemoryPropertyFlags flags =
        g_vulkan.memory_properties.memoryTypes[i].propertyFlags;
    if (bit_match && (flags & required_flags) == required_flags) {
      return i;
    }
  }
  return UINT32_MAX;
}

static int create_buffer_with_properties(VkDeviceSize byte_size,
                                         VkBufferUsageFlags usage,
                                         VkMemoryPropertyFlags memory_flags,
                                         int map_persistently,
                                         VulkanBuffer *out_buffer) {
  if (out_buffer == NULL) {
    set_errorf("Internal error: null out_buffer");
    return 0;
  }

  out_buffer->buffer = VK_NULL_HANDLE;
  out_buffer->memory = VK_NULL_HANDLE;
  out_buffer->mapped_host = NULL;

  VkBufferCreateInfo buffer_info = {0};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = byte_size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (g_vulkan.vkCreateBuffer(g_vulkan.device, &buffer_info, NULL,
                              &out_buffer->buffer) != VK_SUCCESS) {
    set_errorf("vkCreateBuffer failed");
    return 0;
  }

  VkMemoryRequirements requirements = {0};
  g_vulkan.vkGetBufferMemoryRequirements(g_vulkan.device, out_buffer->buffer,
                                         &requirements);

  const uint32_t memory_type_index =
      find_memory_type(requirements.memoryTypeBits, memory_flags);
  if (memory_type_index == UINT32_MAX) {
    set_errorf("No Vulkan memory type found for requested flags");
    destroy_buffer(out_buffer);
    return 0;
  }

  VkMemoryAllocateInfo allocate_info = {0};
  allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate_info.allocationSize = requirements.size;
  allocate_info.memoryTypeIndex = memory_type_index;

  if (g_vulkan.vkAllocateMemory(g_vulkan.device, &allocate_info, NULL,
                                &out_buffer->memory) != VK_SUCCESS) {
    set_errorf("vkAllocateMemory failed");
    destroy_buffer(out_buffer);
    return 0;
  }

  if (g_vulkan.vkBindBufferMemory(g_vulkan.device, out_buffer->buffer,
                                  out_buffer->memory, 0) != VK_SUCCESS) {
    set_errorf("vkBindBufferMemory failed");
    destroy_buffer(out_buffer);
    return 0;
  }

  if (map_persistently) {
    if (g_vulkan.vkMapMemory(g_vulkan.device, out_buffer->memory, 0, byte_size, 0,
                             &out_buffer->mapped_host) != VK_SUCCESS) {
      set_errorf("vkMapMemory failed for persistent host mapping");
      destroy_buffer(out_buffer);
      return 0;
    }
  }

  return 1;
}

static int create_host_visible_buffer(VkDeviceSize byte_size,
                                      VkBufferUsageFlags usage,
                                      VulkanBuffer *out_buffer) {
  return create_buffer_with_properties(
      byte_size, usage,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      1, out_buffer);
}

static int create_host_visible_storage_buffer(VkDeviceSize byte_size,
                                              VulkanBuffer *out_buffer) {
  return create_host_visible_buffer(byte_size, Neuron_VK_USAGE_STORAGE, out_buffer);
}

static int create_device_local_storage_buffer(VkDeviceSize byte_size,
                                              VulkanBuffer *out_buffer) {
  return create_buffer_with_properties(
      byte_size, Neuron_VK_USAGE_STORAGE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0,
      out_buffer);
}

static int map_copy_host_to_buffer(const VulkanBuffer *buffer,
                                   const void *source, VkDeviceSize size) {
  if (buffer == NULL || source == NULL || size == 0 || buffer->mapped_host == NULL) {
    set_errorf("Invalid Vulkan host->device copy arguments");
    return 0;
  }
  memcpy(buffer->mapped_host, source, (size_t)size);
  return 1;
}

static int map_copy_buffer_to_host(const VulkanBuffer *buffer,
                                   void *destination, VkDeviceSize size) {
  if (buffer == NULL || destination == NULL || size == 0 ||
      buffer->mapped_host == NULL) {
    set_errorf("Invalid Vulkan device->host copy arguments");
    return 0;
  }
  memcpy(destination, buffer->mapped_host, (size_t)size);
  return 1;
}

static int create_compute_pipeline(const uint32_t *shader_words,
                                   size_t shader_word_count,
                                   uint32_t descriptor_binding_count,
                                   uint32_t push_constant_size,
                                   VkDescriptorSetLayout *out_set_layout,
                                   VkPipelineLayout *out_pipeline_layout,
                                   VkShaderModule *out_shader_module,
                                   VkPipeline *out_pipeline) {
  enum { kMaxBindings = 8 };
  *out_set_layout = VK_NULL_HANDLE;
  *out_pipeline_layout = VK_NULL_HANDLE;
  *out_shader_module = VK_NULL_HANDLE;
  *out_pipeline = VK_NULL_HANDLE;

  if (descriptor_binding_count == 0 || descriptor_binding_count > kMaxBindings) {
    set_errorf("Invalid Vulkan descriptor binding count: %u",
               (unsigned int)descriptor_binding_count);
    return 0;
  }

  VkDescriptorSetLayoutBinding bindings[kMaxBindings];
  memset(bindings, 0, sizeof(bindings));
  for (uint32_t i = 0; i < descriptor_binding_count; ++i) {
    bindings[i].binding = i;
    bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[i].descriptorCount = 1;
    bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  }

  VkDescriptorSetLayoutCreateInfo set_layout_info = {0};
  set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  set_layout_info.bindingCount = descriptor_binding_count;
  set_layout_info.pBindings = bindings;
  if (g_vulkan.vkCreateDescriptorSetLayout(g_vulkan.device, &set_layout_info, NULL,
                                           out_set_layout) != VK_SUCCESS) {
    set_errorf("vkCreateDescriptorSetLayout failed");
    return 0;
  }

  VkPushConstantRange push_constant_range = {0};
  push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  push_constant_range.offset = 0;
  push_constant_range.size = push_constant_size;

  VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = out_set_layout;
  pipeline_layout_info.pushConstantRangeCount = push_constant_size > 0 ? 1 : 0;
  pipeline_layout_info.pPushConstantRanges =
      push_constant_size > 0 ? &push_constant_range : NULL;

  if (g_vulkan.vkCreatePipelineLayout(g_vulkan.device, &pipeline_layout_info, NULL,
                                      out_pipeline_layout) != VK_SUCCESS) {
    set_errorf("vkCreatePipelineLayout failed");
    return 0;
  }

  VkShaderModuleCreateInfo shader_module_info = {0};
  shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_module_info.codeSize = shader_word_count * sizeof(uint32_t);
  shader_module_info.pCode = shader_words;
  if (g_vulkan.vkCreateShaderModule(g_vulkan.device, &shader_module_info, NULL,
                                    out_shader_module) != VK_SUCCESS) {
    set_errorf("vkCreateShaderModule failed");
    return 0;
  }

  VkPipelineShaderStageCreateInfo stage_info = {0};
  stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage_info.module = *out_shader_module;
  stage_info.pName = "main";

  VkComputePipelineCreateInfo pipeline_info = {0};
  pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_info.stage = stage_info;



  pipeline_info.layout = *out_pipeline_layout;
  if (g_vulkan.vkCreateComputePipelines(g_vulkan.device, VK_NULL_HANDLE, 1,
                                        &pipeline_info, NULL,
                                        out_pipeline) != VK_SUCCESS) {
    set_errorf("vkCreateComputePipelines failed");
    return 0;
  }

  return 1;
}

static int initialize_instance_and_device(void) {
  NeuronVulkanCommonContext common = {0};
  char common_error[256] = {0};
  if (neuron_vk_common_acquire(g_vulkan.requested_scope_mode,
                               g_vulkan.requested_device_class, 0, &common,
                               common_error, sizeof(common_error)) != 0) {
    set_errorf("Failed to acquire shared Vulkan context: %s",
               common_error[0] != '\0' ? common_error : "unknown");
    return 0;
  }

  g_vulkan.common_context_acquired = 1;
  g_vulkan.instance = common.instance;
  g_vulkan.physical_device = common.physical_device;
  g_vulkan.device = common.device;
  g_vulkan.queue = common.compute_queue;
  g_vulkan.queue_family_index = common.compute_queue_family;
  g_vulkan.selected_device_class = common.selected_device_class;
  snprintf(g_vulkan.selected_device_name, sizeof(g_vulkan.selected_device_name),
           "%s", common.selected_device_name);

  if (!load_instance_functions() || !load_device_functions()) {
    return 0;
  }

  if (g_vulkan.queue == VK_NULL_HANDLE) {
    g_vulkan.vkGetDeviceQueue(g_vulkan.device, g_vulkan.queue_family_index, 0,
                              &g_vulkan.queue);
    if (g_vulkan.queue == VK_NULL_HANDLE) {
      set_errorf("vkGetDeviceQueue failed to return a compute queue");
      return 0;
    }
  }

  g_vulkan.vkGetPhysicalDeviceMemoryProperties(g_vulkan.physical_device,
                                               &g_vulkan.memory_properties);

  if (env_flag_enabled("NEURON_GPU_VULKAN_LOG_DEVICE")) {
    const char *mode_name = "default";
    switch (g_vulkan.requested_scope_mode) {
    case NEURON_GPU_SCOPE_MODE_FORCE:
      mode_name = "force";
      break;
    case NEURON_GPU_SCOPE_MODE_PREFER:
      mode_name = "prefer";
      break;
    case NEURON_GPU_SCOPE_MODE_DEFAULT:
    default:
      mode_name = "default";
      break;
    }
    fprintf(stderr,
            "[VULKAN_DEVICE] requested_mode=%s requested_class=%s "
            "selected_class=%s selected_name=%s queue_family=%u\n",
            mode_name, device_class_name(g_vulkan.requested_device_class),
            device_class_name(g_vulkan.selected_device_class),
            g_vulkan.selected_device_name, g_vulkan.queue_family_index);
  }

  VkCommandPoolCreateInfo command_pool_info = {0};
  command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  command_pool_info.queueFamilyIndex = g_vulkan.queue_family_index;
  if (g_vulkan.vkCreateCommandPool(g_vulkan.device, &command_pool_info, NULL,
                                   &g_vulkan.command_pool) != VK_SUCCESS) {
    set_errorf("vkCreateCommandPool failed");
    return 0;
  }

  return 1;
}

static int initialize_pipelines(void) {
  if (!create_compute_pipeline(kElementwiseBinarySpirv,
                               kElementwiseBinarySpirvWordCount, 3,
                               sizeof(uint32_t) * 2,
                               &g_vulkan.binary_descriptor_set_layout,



                               &g_vulkan.binary_pipeline_layout,
                               &g_vulkan.binary_shader_module,
                               &g_vulkan.binary_pipeline)) {
    return 0;
  }

  if (!create_compute_pipeline(kBinaryChainSpirv, kBinaryChainSpirvWordCount, 5,
                               sizeof(uint32_t) * 4,
                               &g_vulkan.binary_chain_descriptor_set_layout,
                               &g_vulkan.binary_chain_pipeline_layout,
                               &g_vulkan.binary_chain_shader_module,
                               &g_vulkan.binary_chain_pipeline)) {
    return 0;
  }

  if (!create_compute_pipeline(kBinaryThenFmaSpirv,
                               kBinaryThenFmaSpirvWordCount, 6,
                               sizeof(uint32_t) * 2,
                               &g_vulkan.binary_then_fma_descriptor_set_layout,
                               &g_vulkan.binary_then_fma_pipeline_layout,
                               &g_vulkan.binary_then_fma_shader_module,
                               &g_vulkan.binary_then_fma_pipeline)) {
    return 0;
  }

  if (!create_compute_pipeline(kBinaryChainThenFmaSpirv,
                               kBinaryChainThenFmaSpirvWordCount, 8,
                               sizeof(uint32_t) * 4,
                               &g_vulkan.binary_chain_then_fma_descriptor_set_layout,
                               &g_vulkan.binary_chain_then_fma_pipeline_layout,
                               &g_vulkan.binary_chain_then_fma_shader_module,
                               &g_vulkan.binary_chain_then_fma_pipeline)) {
    return 0;
  }

  if (!create_compute_pipeline(kFmaSpirv, kFmaSpirvWordCount, 4,
                               sizeof(uint32_t),
                               &g_vulkan.fma_descriptor_set_layout,
                               &g_vulkan.fma_pipeline_layout,
                               &g_vulkan.fma_shader_module,
                               &g_vulkan.fma_pipeline)) {
    return 0;
  }

  if (!create_compute_pipeline(kMatMulDenseSpirv, kMatMulDenseSpirvWordCount, 5,
                               sizeof(uint32_t) * 8,
                               &g_vulkan.matmul_dense_descriptor_set_layout,
                               &g_vulkan.matmul_dense_pipeline_layout,
                               &g_vulkan.matmul_dense_shader_module,
                               &g_vulkan.matmul_dense_pipeline)) {
    return 0;
  }

  if (!create_compute_pipeline(kMatMulPackedSpirv, kMatMulPackedSpirvWordCount, 6,
                               sizeof(uint32_t) * 13,
                               &g_vulkan.matmul_packed_descriptor_set_layout,
                               &g_vulkan.matmul_packed_pipeline_layout,
                               &g_vulkan.matmul_packed_shader_module,
                               &g_vulkan.matmul_packed_pipeline)) {
    return 0;
  }

  return 1;
}

static int ensure_initialized(void) {
  if (g_vulkan.initialized) {
    return g_vulkan.available;
  }

  g_vulkan.initialized = 1;
  g_vulkan.available = 0;
  clear_error();

  if (!open_vulkan_loader()) {
    goto fail;
  }
  if (!load_global_functions()) {
    goto fail;
  }
  if (!initialize_instance_and_device()) {
    goto fail;
  }
  if (!initialize_pipelines()) {
    goto fail;
  }

  g_vulkan.available = 1;
  clear_error();
  return 1;

fail:
  shutdown_internal();
  g_vulkan.initialized = 1;
  g_vulkan.available = 0;
  return 0;
}

static int dispatch_compute(VkPipeline pipeline, VkPipelineLayout pipeline_layout,
                            VkDescriptorSetLayout descriptor_set_layout,
                            const VulkanBuffer *buffers, uint32_t buffer_count,
                            const void *push_constants,
                            uint32_t push_constants_size,
                            uint32_t dispatch_x, uint32_t dispatch_y,
                            uint32_t dispatch_z) {
  VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  int ok = 0;
  const int scoped_batch = scope_batch_active();

  if (buffers == NULL || buffer_count == 0 ||
      buffer_count > Neuron_VK_MAX_DISPATCH_BUFFERS) {
    set_errorf("Invalid Vulkan dispatch buffer configuration");
    return 0;
  }
  if (dispatch_x == 0 || dispatch_y == 0 || dispatch_z == 0) {
    set_errorf("Invalid Vulkan dispatch workgroup dimensions");
    return 0;
  }

  if (scoped_batch) {
    descriptor_pool = g_vulkan.scope_descriptor_pool;
    command_buffer = g_vulkan.scope_command_buffer;
  } else {
    VkDescriptorPoolSize pool_size = {0};
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = buffer_count;

    VkDescriptorPoolCreateInfo descriptor_pool_info = {0};
    descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.maxSets = 1;
    descriptor_pool_info.poolSizeCount = 1;
    descriptor_pool_info.pPoolSizes = &pool_size;
    if (g_vulkan.vkCreateDescriptorPool(g_vulkan.device, &descriptor_pool_info, NULL,
                                        &descriptor_pool) != VK_SUCCESS) {
      set_errorf("vkCreateDescriptorPool failed");
      goto cleanup;
    }
  }

  VkDescriptorSetAllocateInfo descriptor_set_info = {0};
  descriptor_set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptor_set_info.descriptorPool = descriptor_pool;
  descriptor_set_info.descriptorSetCount = 1;
  descriptor_set_info.pSetLayouts = &descriptor_set_layout;
  if (g_vulkan.vkAllocateDescriptorSets(g_vulkan.device, &descriptor_set_info,
                                        &descriptor_set) != VK_SUCCESS) {
    set_errorf("vkAllocateDescriptorSets failed");
    goto cleanup;
  }

  if (scoped_batch) {
    if (!scope_track_descriptor_update(descriptor_set, buffers, buffer_count)) {
      goto cleanup;
    }
  } else {
    VkDescriptorBufferInfo descriptor_buffer_info[Neuron_VK_MAX_DISPATCH_BUFFERS];
    VkWriteDescriptorSet descriptor_writes[Neuron_VK_MAX_DISPATCH_BUFFERS];
    memset(descriptor_buffer_info, 0, sizeof(descriptor_buffer_info));
    memset(descriptor_writes, 0, sizeof(descriptor_writes));
    for (uint32_t i = 0; i < buffer_count; ++i) {
      descriptor_buffer_info[i].buffer = buffers[i].buffer;
      descriptor_buffer_info[i].offset = 0;
      descriptor_buffer_info[i].range = VK_WHOLE_SIZE;

      descriptor_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_writes[i].dstSet = descriptor_set;
      descriptor_writes[i].dstBinding = i;
      descriptor_writes[i].descriptorCount = 1;
      descriptor_writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      descriptor_writes[i].pBufferInfo = &descriptor_buffer_info[i];
    }
    g_vulkan.vkUpdateDescriptorSets(g_vulkan.device, buffer_count,
                                    descriptor_writes, 0, NULL);
  }

  if (!scoped_batch) {
    VkCommandBufferAllocateInfo command_buffer_info = {0};
    command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_info.commandPool = g_vulkan.command_pool;
    command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_info.commandBufferCount = 1;
    if (g_vulkan.vkAllocateCommandBuffers(g_vulkan.device, &command_buffer_info,
                                          &command_buffer) != VK_SUCCESS) {
      set_errorf("vkAllocateCommandBuffers failed");
      goto cleanup;
    }

    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (g_vulkan.vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
      set_errorf("vkBeginCommandBuffer failed");
      goto cleanup;
    }
  }

  g_vulkan.vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                             pipeline);
  g_vulkan.vkCmdBindDescriptorSets(
      command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1,
      &descriptor_set, 0, NULL);
  if (push_constants != NULL && push_constants_size > 0) {
    g_vulkan.vkCmdPushConstants(command_buffer, pipeline_layout,
                                VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                push_constants_size, push_constants);
  }
  g_vulkan.vkCmdDispatch(command_buffer, dispatch_x, dispatch_y, dispatch_z);
  if (scoped_batch) {
    g_vulkan.scope_metric_dispatch_count++;
  }

  if (g_vulkan.vkCmdPipelineBarrier != NULL) {
    VkMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    if (scoped_batch) {
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
      g_vulkan.vkCmdPipelineBarrier(
          command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, NULL, 0, NULL);
      g_vulkan.scope_metric_barrier_count++;
    } else {
      barrier.dstAccessMask =
          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
          VK_ACCESS_HOST_READ_BIT;
      g_vulkan.vkCmdPipelineBarrier(
          command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
          &barrier, 0, NULL, 0, NULL);
    }
  }

  if (scoped_batch) {
    g_vulkan.scope_has_recorded_work = 1;
    ok = 1;
    goto cleanup;
  }




  if (g_vulkan.vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
    set_errorf("vkEndCommandBuffer failed");
    goto cleanup;
  }

  if (!submit_and_wait_with_fence(command_buffer, "for Vulkan dispatch")) {
    goto cleanup;
  }

  ok = 1;

cleanup:
  if (!scoped_batch && command_buffer != VK_NULL_HANDLE &&
      g_vulkan.vkFreeCommandBuffers != NULL) {
    g_vulkan.vkFreeCommandBuffers(g_vulkan.device, g_vulkan.command_pool, 1,
                                  &command_buffer);
  }
  if (!scoped_batch && descriptor_pool != VK_NULL_HANDLE &&
      g_vulkan.vkDestroyDescriptorPool != NULL) {
    g_vulkan.vkDestroyDescriptorPool(g_vulkan.device, descriptor_pool, NULL);
  }
  return ok;
}

int npp_gpu_vulkan_engine_try_initialize(char *error_buffer, size_t error_size) {
  const int ok = ensure_initialized();
  copy_error_out(error_buffer, error_size);
  return ok ? 1 : 0;
}

void npp_gpu_vulkan_engine_set_scope_preference(NeuronGpuScopeMode mode,
                                            NeuronGpuDeviceClass device_class) {
  switch (mode) {
  case NEURON_GPU_SCOPE_MODE_DEFAULT:
  case NEURON_GPU_SCOPE_MODE_PREFER:
  case NEURON_GPU_SCOPE_MODE_FORCE:
    g_vulkan.requested_scope_mode = mode;
    break;
  default:
    g_vulkan.requested_scope_mode = NEURON_GPU_SCOPE_MODE_DEFAULT;
    break;
  }

  switch (device_class) {
  case NEURON_GPU_DEVICE_CLASS_ANY:
  case NEURON_GPU_DEVICE_CLASS_DISCRETE:
  case NEURON_GPU_DEVICE_CLASS_INTEGRATED:
    g_vulkan.requested_device_class = device_class;
    break;
  default:
    g_vulkan.requested_device_class = NEURON_GPU_DEVICE_CLASS_ANY;
    break;
  }
}

void npp_gpu_vulkan_engine_shutdown(void) {
  shutdown_internal();
  clear_error();
}

int npp_gpu_vulkan_engine_supports_op(NeuronGpuOpKind op) {
  if (!g_vulkan.initialized || !g_vulkan.available) {
    return 0;
  }
  switch (op) {
  case NEURON_GPU_OP_TENSOR_ADD:
  case NEURON_GPU_OP_TENSOR_SUB:
  case NEURON_GPU_OP_TENSOR_MUL:
  case NEURON_GPU_OP_TENSOR_DIV:
  case NEURON_GPU_OP_TENSOR_FMA:
  case NEURON_GPU_OP_TENSOR_MATMUL:
    return 1;
  default:
    return 0;
  }
}

int npp_gpu_vulkan_engine_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                      const float *b, float *out,
                                      int32_t element_count, char *error_buffer,
                                      size_t error_size) {
  if (a == NULL || b == NULL || out == NULL || element_count <= 0) {
    set_errorf("Invalid Vulkan tensor binary arguments");
    copy_error_out(error_buffer, error_size);
    return -1;
  }
  if (!ensure_initialized()) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  uint32_t opcode = 0;
  switch (op) {
  case NEURON_GPU_OP_TENSOR_ADD:
    opcode = 0;
    break;
  case NEURON_GPU_OP_TENSOR_SUB:
    opcode = 1;
    break;
  case NEURON_GPU_OP_TENSOR_MUL:
    opcode = 2;
    break;
  case NEURON_GPU_OP_TENSOR_DIV:
    opcode = 3;
    break;
  default:
    set_errorf("Unsupported Vulkan binary op kind: %d", (int)op);
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  const VkDeviceSize bytes = (VkDeviceSize)element_count * sizeof(float);
  VulkanBuffer buffers[3] = {0};
  const int scoped_batch = scope_batch_active();
  int result = -1;

  if (scoped_batch) {
    if (!scope_resolve_input_buffer(a, bytes, 1, &buffers[0], "binary lhs") ||
        !scope_resolve_input_buffer(b, bytes, 1, &buffers[1], "binary rhs") ||
        !scope_resolve_output_buffer(out, bytes, 0, &buffers[2], "binary out")) {
      goto cleanup;
    }
  } else {
    if (!create_host_visible_storage_buffer(bytes, &buffers[0]) ||
        !create_host_visible_storage_buffer(bytes, &buffers[1]) ||
        !create_host_visible_storage_buffer(bytes, &buffers[2])) {
      goto cleanup;
    }
    if (!map_copy_host_to_buffer(&buffers[0], a, bytes) ||
        !map_copy_host_to_buffer(&buffers[1], b, bytes)) {
      goto cleanup;
    }
  }

  if (scoped_batch) {
    if (g_vulkan.scope_fusion_enabled) {
      if (g_vulkan.scope_pending_binary_chain.active) {
        if (!scope_flush_pending_binary_chain()) {
          goto cleanup;
        }
      }

      if (g_vulkan.scope_pending_binary.active) {
        const VulkanPendingBinaryOp pending = g_vulkan.scope_pending_binary;
        const int uses_pending_lhs = (a == pending.out_host);
        const int uses_pending_rhs = (b == pending.out_host);

        if ((uses_pending_lhs ^ uses_pending_rhs) != 0 &&
            pending.element_count == (uint32_t)element_count) {
          g_vulkan.scope_pending_binary_chain.active = 1;
          g_vulkan.scope_pending_binary_chain.first_opcode = pending.opcode;
          g_vulkan.scope_pending_binary_chain.second_opcode = opcode;
          g_vulkan.scope_pending_binary_chain.temp_is_lhs =
              uses_pending_lhs ? 1u : 0u;
          g_vulkan.scope_pending_binary_chain.element_count =
              (uint32_t)element_count;
          g_vulkan.scope_pending_binary_chain.tmp_host = pending.out_host;
          g_vulkan.scope_pending_binary_chain.mid_host = out;
          g_vulkan.scope_pending_binary_chain.first_lhs_buffer =
              pending.lhs_buffer;
          g_vulkan.scope_pending_binary_chain.first_rhs_buffer =
              pending.rhs_buffer;
          g_vulkan.scope_pending_binary_chain.second_other_buffer =
              uses_pending_lhs ? buffers[1] : buffers[0];
          g_vulkan.scope_pending_binary_chain.tmp_buffer = pending.out_buffer;
          g_vulkan.scope_pending_binary_chain.mid_buffer = buffers[2];
          scope_reset_pending_binary();
          clear_error();
          result = 0;
          goto cleanup;
        }

        if (!scope_flush_pending_fusions()) {
          goto cleanup;
        }
      }

      g_vulkan.scope_pending_binary.active = 1;
      g_vulkan.scope_pending_binary.opcode = opcode;
      g_vulkan.scope_pending_binary.element_count = (uint32_t)element_count;
      g_vulkan.scope_pending_binary.lhs_host = a;
      g_vulkan.scope_pending_binary.rhs_host = b;
      g_vulkan.scope_pending_binary.out_host = out;
      g_vulkan.scope_pending_binary.lhs_buffer = buffers[0];
      g_vulkan.scope_pending_binary.rhs_buffer = buffers[1];
      g_vulkan.scope_pending_binary.out_buffer = buffers[2];
      clear_error();
      result = 0;
      goto cleanup;
    }

    if (!scope_flush_pending_fusions()) {
      goto cleanup;
    }
  }

  {
    struct {
      uint32_t n;
      uint32_t op;
    } push = {(uint32_t)element_count, opcode};

    const uint32_t group_x = ((uint32_t)element_count + 255u) / 256u;
    if (!dispatch_compute(g_vulkan.binary_pipeline, g_vulkan.binary_pipeline_layout,
                          g_vulkan.binary_descriptor_set_layout, buffers, 3,
                          &push, sizeof(push), group_x, 1u, 1u)) {
      goto cleanup;
    }
  }

  if (!scoped_batch && !map_copy_buffer_to_host(&buffers[2], out, bytes)) {
    goto cleanup;
  }

  clear_error();
  result = 0;

cleanup:
  if (!scoped_batch) {
    destroy_buffer(&buffers[2]);
    destroy_buffer(&buffers[1]);
    destroy_buffer(&buffers[0]);
  }
  copy_error_out(error_buffer, error_size);
  return result;
}

int npp_gpu_vulkan_engine_dispatch_fma(const float *a, const float *b,
                                   const float *c, float *out,
                                   int32_t element_count, char *error_buffer,
                                   size_t error_size) {
  if (a == NULL || b == NULL || c == NULL || out == NULL || element_count <= 0) {
    set_errorf("Invalid Vulkan tensor fma arguments");
    copy_error_out(error_buffer, error_size);
    return -1;
  }
  if (!ensure_initialized()) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }




  const VkDeviceSize bytes = (VkDeviceSize)element_count * sizeof(float);
  VulkanBuffer buffers[4] = {0};
  const int scoped_batch = scope_batch_active();
  int result = -1;

  if (scoped_batch) {
    if (!scope_resolve_input_buffer(a, bytes, 1, &buffers[0], "fma a") ||
        !scope_resolve_input_buffer(b, bytes, 1, &buffers[1], "fma b") ||
        !scope_resolve_input_buffer(c, bytes, 1, &buffers[2], "fma c") ||
        !scope_resolve_output_buffer(out, bytes, 0, &buffers[3], "fma out")) {
      goto cleanup;
    }

    if (g_vulkan.scope_fusion_enabled) {
      if (g_vulkan.scope_pending_binary_chain.active) {
        const VulkanPendingBinaryChainOp pending_chain =
            g_vulkan.scope_pending_binary_chain;
        if (a == pending_chain.mid_host &&
            pending_chain.element_count == (uint32_t)element_count) {
          const int fused = scope_dispatch_fused_binary_chain_then_fma(
              &pending_chain, &buffers[1], &buffers[2], &buffers[3]);
          if (fused < 0) {
            goto cleanup;
          }
          if (fused > 0) {
            scope_reset_pending_binary_chain();
            clear_error();
            result = 0;
            goto cleanup;
          }
        }
      }

      if (g_vulkan.scope_pending_binary.active) {
        const VulkanPendingBinaryOp pending = g_vulkan.scope_pending_binary;
        if (a == pending.out_host &&
            pending.element_count == (uint32_t)element_count) {
          const int fused = scope_dispatch_fused_binary_then_fma(
              &pending, &buffers[1], &buffers[2], &buffers[3]);
          if (fused < 0) {
            goto cleanup;
          }
          if (fused > 0) {
            scope_reset_pending_binary();
            clear_error();
            result = 0;
            goto cleanup;
          }
        }
      }
    }

    if (!scope_flush_pending_fusions()) {
      goto cleanup;
    }
  } else {
    if (!create_host_visible_storage_buffer(bytes, &buffers[0]) ||
        !create_host_visible_storage_buffer(bytes, &buffers[1]) ||
        !create_host_visible_storage_buffer(bytes, &buffers[2]) ||
        !create_host_visible_storage_buffer(bytes, &buffers[3])) {
      goto cleanup;
    }
    if (!map_copy_host_to_buffer(&buffers[0], a, bytes) ||
        !map_copy_host_to_buffer(&buffers[1], b, bytes) ||
        !map_copy_host_to_buffer(&buffers[2], c, bytes)) {
      goto cleanup;
    }
  }

  const uint32_t push_n = (uint32_t)element_count;
  const uint32_t group_x = ((uint32_t)element_count + 255u) / 256u;
  if (!dispatch_compute(g_vulkan.fma_pipeline, g_vulkan.fma_pipeline_layout,
                        g_vulkan.fma_descriptor_set_layout, buffers, 4, &push_n,
                        sizeof(push_n), group_x, 1u, 1u)) {
    goto cleanup;
  }
  if (!scoped_batch && !map_copy_buffer_to_host(&buffers[3], out, bytes)) {
    goto cleanup;
  }

  clear_error();
  result = 0;

cleanup:
  if (!scoped_batch) {
    destroy_buffer(&buffers[3]);
    destroy_buffer(&buffers[2]);
    destroy_buffer(&buffers[1]);
    destroy_buffer(&buffers[0]);
  }
  copy_error_out(error_buffer, error_size);
  return result;
}

static int validate_matmul_descriptor(const NeuronGpuMatMulDispatchDesc *desc) {
  if (desc == NULL || desc->a == NULL || desc->out == NULL || desc->m <= 0 ||
      desc->n <= 0 || desc->k <= 0) {
    set_errorf("Invalid Vulkan tensor matmul descriptor");
    return 0;
  }
  if (desc->b == NULL && desc->packed_b == NULL) {
    set_errorf("Vulkan tensor matmul requires dense or packed B");
    return 0;
  }
  if (desc->packed_b != NULL) {
    if (desc->packed_b->rows != desc->k || desc->packed_b->cols != desc->n ||
        desc->packed_b->kc <= 0 || desc->packed_b->nc <= 0 ||
        desc->packed_b->kBlocks <= 0 || desc->packed_b->nBlocks <= 0 ||
        desc->packed_b->offsets == NULL || desc->packed_b->data == NULL ||
        desc->packed_b->panelCount == 0) {
      set_errorf("Vulkan tensor matmul packed-B metadata is invalid");
      return 0;
    }
    if (desc->packed_b->panelCount >
        (size_t)desc->packed_b->kBlocks * (size_t)desc->packed_b->nBlocks) {
      set_errorf("Vulkan tensor matmul packed-B panel count is out of range");
      return 0;
    }
  }
  if (desc->bias != NULL) {
    if (desc->bias_cols != desc->n ||
        (desc->bias_rows != 1 && desc->bias_rows != desc->m)) {
      set_errorf("Vulkan tensor matmul bias shape mismatch");
      return 0;
    }
  }
  if (desc->residual != NULL && desc->residual_cols != desc->n) {
    set_errorf("Vulkan tensor matmul residual shape mismatch");
    return 0;
  }
  if (desc->activation != 0 && desc->activation != 1 && desc->activation != 2) {
    set_errorf("Vulkan tensor matmul activation is unsupported: %d",
               desc->activation);
    return 0;
  }
  return 1;
}

static int create_and_fill_buffer(const void *data, VkDeviceSize bytes,
                                  VulkanBuffer *buffer, const char *label) {
  if (bytes == 0 || buffer == NULL) {
    set_errorf("Invalid Vulkan buffer request for %s", label);
    return 0;
  }
  if (!create_host_visible_storage_buffer(bytes, buffer)) {
    return 0;
  }
  if (data != NULL && !map_copy_host_to_buffer(buffer, data, bytes)) {
    set_errorf("Failed to upload %s", label);
    return 0;
  }
  return 1;
}

static int create_zero_buffer(VulkanBuffer *buffer, const char *label) {
  uint32_t zero = 0;
  return create_and_fill_buffer(&zero, sizeof(zero), buffer, label);
}

static size_t compute_packed_data_float_count(const NeuronPackedMatrix *packed) {
  size_t max_end = 0;
  const size_t panels = packed->panelCount;
  for (size_t panel_index = 0; panel_index < panels; ++panel_index) {
    const int32_t n_block = (int32_t)(panel_index / (size_t)packed->kBlocks);
    const int32_t k_block = (int32_t)(panel_index % (size_t)packed->kBlocks);
    if (n_block < 0 || n_block >= packed->nBlocks || k_block < 0 ||
        k_block >= packed->kBlocks) {
      return 0;
    }

    const int32_t nc0 = n_block * packed->nc;
    const int32_t kc0 = k_block * packed->kc;
    const int32_t nc_cur =
        (packed->cols - nc0) < packed->nc ? (packed->cols - nc0) : packed->nc;
    const int32_t kc_cur =
        (packed->rows - kc0) < packed->kc ? (packed->rows - kc0) : packed->kc;
    if (nc_cur <= 0 || kc_cur <= 0) {
      return 0;
    }

    const size_t panel_size = (size_t)nc_cur * (size_t)kc_cur;
    const size_t panel_offset = packed->offsets[panel_index];
    if (panel_offset > SIZE_MAX - panel_size) {
      return 0;
    }
    const size_t panel_end = panel_offset + panel_size;
    if (panel_end > max_end) {
      max_end = panel_end;
    }
  }
  return max_end;
}

static int dispatch_matmul_dense(const NeuronGpuMatMulDispatchDesc *desc) {
  VulkanBuffer buffers[5] = {0};
  const int scoped_batch = scope_batch_active();
  int ok = 0;

  if (scoped_batch && !scope_flush_pending_fusions()) {
    goto cleanup;
  }

  const VkDeviceSize a_bytes =
      (VkDeviceSize)((size_t)desc->m * (size_t)desc->k * sizeof(float));
  const VkDeviceSize b_bytes =
      (VkDeviceSize)((size_t)desc->k * (size_t)desc->n * sizeof(float));
  const VkDeviceSize out_bytes =
      (VkDeviceSize)((size_t)desc->m * (size_t)desc->n * sizeof(float));
  const VkDeviceSize bias_bytes = (desc->bias != NULL && desc->bias_rows > 0 &&
                                   desc->bias_cols > 0)
                                      ? (VkDeviceSize)((size_t)desc->bias_rows *
                                                       (size_t)desc->bias_cols *
                                                       sizeof(float))
                                      : (VkDeviceSize)sizeof(uint32_t);
  const VkDeviceSize residual_bytes =
      (desc->residual != NULL && desc->m > 0 && desc->residual_cols > 0)
          ? (VkDeviceSize)((size_t)desc->m * (size_t)desc->residual_cols *
                           sizeof(float))
          : (VkDeviceSize)sizeof(uint32_t);

  if (scoped_batch) {
    if (!scope_resolve_input_buffer(desc->a, a_bytes, 1, &buffers[0], "matmul A") ||
        !scope_resolve_input_buffer(desc->b, b_bytes, 1, &buffers[1],
                                    "matmul B")) {
      goto cleanup;
    }
    if (desc->bias != NULL) {
      if (!scope_resolve_input_buffer(desc->bias, bias_bytes, 1, &buffers[2],
                                      "matmul bias")) {
        goto cleanup;
      }
    } else if (!scope_get_zero_buffer(&buffers[2], "matmul bias placeholder")) {
      goto cleanup;
    }
    if (desc->residual != NULL) {
      if (!scope_resolve_input_buffer(desc->residual, residual_bytes, 1,
                                      &buffers[3], "matmul residual")) {
        goto cleanup;
      }



    } else if (!scope_get_zero_buffer(&buffers[3],
                                      "matmul residual placeholder")) {
      goto cleanup;
    }
    if (!scope_resolve_output_buffer(desc->out, out_bytes, desc->accumulate,
                                     &buffers[4], "matmul out")) {
      goto cleanup;
    }
  } else {
    if (!create_and_fill_buffer(desc->a, a_bytes, &buffers[0], "matmul A") ||
        !create_and_fill_buffer(desc->b, b_bytes, &buffers[1], "matmul B")) {
      goto cleanup;
    }
    if (desc->bias != NULL) {
      if (!create_and_fill_buffer(desc->bias, bias_bytes, &buffers[2],
                                  "matmul bias")) {
        goto cleanup;
      }
    } else if (!create_zero_buffer(&buffers[2], "matmul bias placeholder")) {
      goto cleanup;
    }
    if (desc->residual != NULL) {
      if (!create_and_fill_buffer(desc->residual, residual_bytes, &buffers[3],
                                  "matmul residual")) {
        goto cleanup;
      }
    } else if (!create_zero_buffer(&buffers[3], "matmul residual placeholder")) {
      goto cleanup;
    }
    if (!create_host_visible_storage_buffer(out_bytes, &buffers[4])) {
      goto cleanup;
    }
    if (desc->accumulate &&
        !map_copy_host_to_buffer(&buffers[4], desc->out, out_bytes)) {
      set_errorf("Failed to upload accumulated matmul output");
      goto cleanup;
    }
  }

  struct {
    uint32_t m;
    uint32_t n;
    uint32_t k;
    uint32_t bias_rows;
    uint32_t bias_cols;
    uint32_t residual_cols;
    uint32_t activation;
    uint32_t accumulate;
  } push = {(uint32_t)desc->m,
            (uint32_t)desc->n,
            (uint32_t)desc->k,
            (uint32_t)(desc->bias != NULL ? desc->bias_rows : 0),
            (uint32_t)(desc->bias != NULL ? desc->bias_cols : 0),
            (uint32_t)(desc->residual != NULL ? desc->residual_cols : 0),
            (uint32_t)desc->activation,
            (uint32_t)(desc->accumulate ? 1 : 0)};

  const uint32_t group_x = ((uint32_t)desc->n + 15u) / 16u;
  const uint32_t group_y = ((uint32_t)desc->m + 15u) / 16u;
  if (!dispatch_compute(g_vulkan.matmul_dense_pipeline,
                        g_vulkan.matmul_dense_pipeline_layout,
                        g_vulkan.matmul_dense_descriptor_set_layout, buffers, 5,
                        &push, sizeof(push), group_x, group_y, 1u)) {
    goto cleanup;
  }
  if (!scoped_batch &&
      !map_copy_buffer_to_host(&buffers[4], desc->out, out_bytes)) {
    goto cleanup;
  }

  ok = 1;

cleanup:
  if (!scoped_batch) {
    destroy_buffer(&buffers[4]);
    destroy_buffer(&buffers[3]);
    destroy_buffer(&buffers[2]);
    destroy_buffer(&buffers[1]);
    destroy_buffer(&buffers[0]);
  }
  return ok;
}

static int dispatch_matmul_packed(const NeuronGpuMatMulDispatchDesc *desc) {
  VulkanBuffer buffers[6] = {0};
  uint32_t *offsets32 = NULL;
  const int scoped_batch = scope_batch_active();
  int ok = 0;

  if (scoped_batch && !scope_flush_pending_fusions()) {
    goto cleanup;
  }

  const NeuronPackedMatrix *packed = desc->packed_b;
  const size_t packed_data_floats = compute_packed_data_float_count(packed);
  if (packed_data_floats == 0) {
    set_errorf("Vulkan packed matmul failed to compute packed data footprint");
    goto cleanup;
  }
  if (packed_data_floats > SIZE_MAX / sizeof(float)) {
    set_errorf("Vulkan packed matmul packed data footprint overflow");
    goto cleanup;
  }
  if (packed->panelCount > (size_t)UINT32_MAX) {
    set_errorf("Vulkan packed matmul panel count exceeds uint32 range");
    goto cleanup;
  }
  if (packed->panelCount > SIZE_MAX / sizeof(uint32_t)) {
    set_errorf("Vulkan packed matmul offsets footprint overflow");
    goto cleanup;
  }

  const size_t offsets_count = packed->panelCount;
  offsets32 = (uint32_t *)malloc(offsets_count * sizeof(uint32_t));
  if (offsets32 == NULL) {
    set_errorf("Failed to allocate temporary packed offsets buffer");
    goto cleanup;
  }
  for (size_t i = 0; i < offsets_count; ++i) {
    if (packed->offsets[i] > (size_t)UINT32_MAX) {
      set_errorf("Vulkan packed matmul offset exceeds uint32 range");
      goto cleanup;
    }
    offsets32[i] = (uint32_t)packed->offsets[i];
  }

  const VkDeviceSize a_bytes =
      (VkDeviceSize)((size_t)desc->m * (size_t)desc->k * sizeof(float));
  const VkDeviceSize packed_data_bytes =
      (VkDeviceSize)(packed_data_floats * sizeof(float));
  const VkDeviceSize offsets_bytes =
      (VkDeviceSize)(offsets_count * sizeof(uint32_t));
  const VkDeviceSize out_bytes =
      (VkDeviceSize)((size_t)desc->m * (size_t)desc->n * sizeof(float));
  const VkDeviceSize bias_bytes = (desc->bias != NULL && desc->bias_rows > 0 &&
                                   desc->bias_cols > 0)
                                      ? (VkDeviceSize)((size_t)desc->bias_rows *
                                                       (size_t)desc->bias_cols *
                                                       sizeof(float))
                                      : (VkDeviceSize)sizeof(uint32_t);
  const VkDeviceSize residual_bytes =
      (desc->residual != NULL && desc->m > 0 && desc->residual_cols > 0)
          ? (VkDeviceSize)((size_t)desc->m * (size_t)desc->residual_cols *
                           sizeof(float))
          : (VkDeviceSize)sizeof(uint32_t);

  if (scoped_batch) {
    if (!scope_resolve_input_buffer(desc->a, a_bytes, 1, &buffers[0],
                                    "packed matmul A") ||
        !scope_resolve_input_buffer(packed->data, packed_data_bytes, 1,
                                    &buffers[1], "packed matmul data") ||
        !scope_resolve_input_buffer(offsets32, offsets_bytes, 0, &buffers[2],
                                    "packed matmul offsets")) {
      goto cleanup;
    }
    if (desc->bias != NULL) {
      if (!scope_resolve_input_buffer(desc->bias, bias_bytes, 1, &buffers[3],
                                      "packed matmul bias")) {
        goto cleanup;
      }
    } else if (!scope_get_zero_buffer(&buffers[3],
                                      "packed matmul bias placeholder")) {
      goto cleanup;
    }
    if (desc->residual != NULL) {
      if (!scope_resolve_input_buffer(desc->residual, residual_bytes, 1,
                                      &buffers[4], "packed matmul residual")) {
        goto cleanup;
      }
    } else if (!scope_get_zero_buffer(&buffers[4],
                                      "packed matmul residual placeholder")) {
      goto cleanup;
    }
    if (!scope_resolve_output_buffer(desc->out, out_bytes, desc->accumulate,
                                     &buffers[5], "packed matmul out")) {
      goto cleanup;
    }
  } else {
    if (!create_and_fill_buffer(desc->a, a_bytes, &buffers[0], "packed matmul A") ||
        !create_and_fill_buffer(packed->data, packed_data_bytes, &buffers[1],
                                "packed matmul data") ||
        !create_and_fill_buffer(offsets32, offsets_bytes, &buffers[2],
                                "packed matmul offsets")) {
      goto cleanup;
    }
    if (desc->bias != NULL) {
      if (!create_and_fill_buffer(desc->bias, bias_bytes, &buffers[3],
                                  "packed matmul bias")) {
        goto cleanup;
      }
    } else if (!create_zero_buffer(&buffers[3],
                                   "packed matmul bias placeholder")) {
      goto cleanup;
    }
    if (desc->residual != NULL) {
      if (!create_and_fill_buffer(desc->residual, residual_bytes, &buffers[4],
                                  "packed matmul residual")) {
        goto cleanup;
      }
    } else if (!create_zero_buffer(&buffers[4],
                                   "packed matmul residual placeholder")) {
      goto cleanup;
    }
    if (!create_host_visible_storage_buffer(out_bytes, &buffers[5])) {
      goto cleanup;
    }
    if (desc->accumulate &&
        !map_copy_host_to_buffer(&buffers[5], desc->out, out_bytes)) {
      set_errorf("Failed to upload accumulated packed matmul output");
      goto cleanup;
    }
  }

  struct {
    uint32_t m;
    uint32_t n;
    uint32_t k;
    uint32_t kc;
    uint32_t nc;
    uint32_t k_blocks;
    uint32_t n_blocks;
    uint32_t panel_count;
    uint32_t bias_rows;
    uint32_t bias_cols;
    uint32_t residual_cols;
    uint32_t activation;
    uint32_t accumulate;
  } push = {(uint32_t)desc->m,
            (uint32_t)desc->n,
            (uint32_t)desc->k,
            (uint32_t)packed->kc,
            (uint32_t)packed->nc,
            (uint32_t)packed->kBlocks,
            (uint32_t)packed->nBlocks,
            (uint32_t)packed->panelCount,
            (uint32_t)(desc->bias != NULL ? desc->bias_rows : 0),
            (uint32_t)(desc->bias != NULL ? desc->bias_cols : 0),
            (uint32_t)(desc->residual != NULL ? desc->residual_cols : 0),
            (uint32_t)desc->activation,
            (uint32_t)(desc->accumulate ? 1 : 0)};




  const uint32_t group_x = ((uint32_t)desc->n + 15u) / 16u;
  const uint32_t group_y = ((uint32_t)desc->m + 15u) / 16u;
  if (!dispatch_compute(g_vulkan.matmul_packed_pipeline,
                        g_vulkan.matmul_packed_pipeline_layout,
                        g_vulkan.matmul_packed_descriptor_set_layout, buffers, 6,
                        &push, sizeof(push), group_x, group_y, 1u)) {
    goto cleanup;
  }
  if (!scoped_batch &&
      !map_copy_buffer_to_host(&buffers[5], desc->out, out_bytes)) {
    goto cleanup;
  }

  ok = 1;

cleanup:
  if (offsets32 != NULL) {
    free(offsets32);
  }
  if (!scoped_batch) {
    destroy_buffer(&buffers[5]);
    destroy_buffer(&buffers[4]);
    destroy_buffer(&buffers[3]);
    destroy_buffer(&buffers[2]);
    destroy_buffer(&buffers[1]);
    destroy_buffer(&buffers[0]);
  }
  return ok;
}

int npp_gpu_vulkan_engine_dispatch_matmul(const NeuronGpuMatMulDispatchDesc *desc,
                                      char *error_buffer, size_t error_size) {
  if (!validate_matmul_descriptor(desc)) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }
  if (!ensure_initialized()) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  if ((desc->packed_b != NULL && !dispatch_matmul_packed(desc)) ||
      (desc->packed_b == NULL && !dispatch_matmul_dense(desc))) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  clear_error();
  copy_error_out(error_buffer, error_size);
  return 0;
}

int npp_gpu_vulkan_engine_scope_begin(char *error_buffer, size_t error_size) {
  if (!ensure_initialized()) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }
  if (g_vulkan.scope_active) {
    clear_error();
    copy_error_out(error_buffer, error_size);
    return 0;
  }

  scope_release_recording_resources();
  scope_destroy_tracking_buffers();

  g_vulkan.scope_active = 1;
  g_vulkan.scope_batch_enabled = env_flag_enabled("NEURON_GPU_SCOPE_BATCH");
  g_vulkan.scope_fusion_enabled =
      g_vulkan.scope_batch_enabled &&
      env_flag_enabled("NEURON_GPU_SCOPE_FUSION");
  g_vulkan.scope_metrics_enabled = env_flag_enabled("NEURON_GPU_SCOPE_METRICS");
  g_vulkan.scope_metric_dispatch_count = 0;
  g_vulkan.scope_metric_barrier_count = 0;
  g_vulkan.scope_metric_descriptor_writes = 0;
  g_vulkan.scope_metric_readback_bytes = 0;
  g_vulkan.host_input_cache_enabled =
      env_flag_enabled("NEURON_GPU_SCOPE_INPUT_CACHE");
  g_vulkan.host_input_cache_verify =
      env_flag_enabled("NEURON_GPU_SCOPE_INPUT_CACHE_VERIFY");
  g_vulkan.scope_readback_sink_only =
      env_flag_enabled("NEURON_GPU_SCOPE_READBACK_SINK_ONLY");
  if (!g_vulkan.host_input_cache_enabled) {
    host_input_cache_destroy_all();
  }
  g_vulkan.scope_has_recorded_work = 0;
  scope_reset_pending_fusion_state();
  if (g_vulkan.scope_batch_enabled && !scope_begin_recording()) {
    g_vulkan.scope_active = 0;
    g_vulkan.scope_batch_enabled = 0;
    g_vulkan.scope_fusion_enabled = 0;
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  clear_error();
  copy_error_out(error_buffer, error_size);
  return 0;
}

int npp_gpu_vulkan_engine_scope_end(char *error_buffer, size_t error_size) {
  if (!g_vulkan.initialized || !g_vulkan.available) {
    set_errorf("Vulkan backend is not initialized");
    copy_error_out(error_buffer, error_size);
    return -1;
  }
  if (!g_vulkan.scope_active) {
    set_errorf("Vulkan scope_end called without matching scope_begin");
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  int ok = 1;
  if (g_vulkan.scope_batch_enabled) {
    ok = scope_flush_and_end_recording();
  } else {
    scope_release_recording_resources();
    scope_destroy_tracking_buffers();
  }

  g_vulkan.scope_active = 0;
  g_vulkan.scope_batch_enabled = 0;
  g_vulkan.scope_fusion_enabled = 0;
  g_vulkan.scope_readback_sink_only = 0;
  g_vulkan.scope_has_recorded_work = 0;
  scope_reset_pending_fusion_state();
  if (g_vulkan.scope_metrics_enabled) {
    fprintf(stderr,
            "[VULKAN_SCOPE_METRICS] dispatch=%llu barrier=%llu descriptor_writes=%llu readback_bytes=%llu\n",
            (unsigned long long)g_vulkan.scope_metric_dispatch_count,
            (unsigned long long)g_vulkan.scope_metric_barrier_count,
            (unsigned long long)g_vulkan.scope_metric_descriptor_writes,
            (unsigned long long)g_vulkan.scope_metric_readback_bytes);
  }
  g_vulkan.scope_metrics_enabled = 0;
  if (ok) {
    clear_error();
  }
  copy_error_out(error_buffer, error_size);
  return ok ? 0 : -1;
}

int npp_gpu_vulkan_engine_materialize(const void *host_ptr, size_t byte_size,
                                  char *error_buffer, size_t error_size) {
  if (host_ptr == NULL || byte_size == 0) {
    clear_error();
    copy_error_out(error_buffer, error_size);
    return 0;
  }
  if (!g_vulkan.initialized || !g_vulkan.available) {
    set_errorf("Vulkan backend is not initialized");
    copy_error_out(error_buffer, error_size);
    return -1;
  }
  if (!scope_batch_active()) {
    clear_error();
    copy_error_out(error_buffer, error_size);
    return 0;
  }

  VulkanScopeBufferEntry *entry =
      scope_find_entry(host_ptr, (VkDeviceSize)byte_size);
  if (entry == NULL || !entry->dirty_output) {
    clear_error();
    copy_error_out(error_buffer, error_size);
    return 0;
  }

  if (!scope_flush_and_end_recording()) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }
  if (!scope_begin_recording()) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  clear_error();
  copy_error_out(error_buffer, error_size);
  return 0;
}

#else

static void write_header_missing_error(char *error_buffer, size_t error_size) {
  if (error_buffer != NULL && error_size > 0) {
    snprintf(error_buffer, error_size,
             "Vulkan headers were not available at build time");
  }
}

int npp_gpu_vulkan_engine_try_initialize(char *error_buffer, size_t error_size) {
  write_header_missing_error(error_buffer, error_size);
  return 0;
}

void npp_gpu_vulkan_engine_set_scope_preference(NeuronGpuScopeMode mode,
                                            NeuronGpuDeviceClass device_class) {
  (void)mode;
  (void)device_class;
}

void npp_gpu_vulkan_engine_shutdown(void) {}

int npp_gpu_vulkan_engine_supports_op(NeuronGpuOpKind op) {
  (void)op;
  return 0;
}

int npp_gpu_vulkan_engine_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                      const float *b, float *out,
                                      int32_t element_count, char *error_buffer,
                                      size_t error_size) {
  (void)op;
  (void)a;
  (void)b;
  (void)out;
  (void)element_count;
  write_header_missing_error(error_buffer, error_size);
  return -1;
}

int npp_gpu_vulkan_engine_dispatch_fma(const float *a, const float *b,
                                   const float *c, float *out,
                                   int32_t element_count, char *error_buffer,
                                   size_t error_size) {
  (void)a;
  (void)b;
  (void)c;
  (void)out;
  (void)element_count;
  write_header_missing_error(error_buffer, error_size);
  return -1;
}

int npp_gpu_vulkan_engine_dispatch_matmul(const NeuronGpuMatMulDispatchDesc *desc,
                                      char *error_buffer, size_t error_size) {
  (void)desc;
  write_header_missing_error(error_buffer, error_size);



  return -1;
}

int npp_gpu_vulkan_engine_scope_begin(char *error_buffer, size_t error_size) {
  write_header_missing_error(error_buffer, error_size);
  return -1;
}

int npp_gpu_vulkan_engine_scope_end(char *error_buffer, size_t error_size) {
  write_header_missing_error(error_buffer, error_size);
  return -1;
}

int npp_gpu_vulkan_engine_materialize(const void *host_ptr, size_t byte_size,
                                  char *error_buffer, size_t error_size) {
  (void)host_ptr;
  (void)byte_size;
  write_header_missing_error(error_buffer, error_size);
  return -1;
}

#endif




#if Neuron_VK_COMMON_HAS_HEADERS

static int scope_sync_for_external_interop(void) {
  if (!scope_batch_active()) {
    set_errorf("Vulkan tensor interop requires an active batched gpu scope");
    return 0;
  }

  if (!scope_flush_pending_fusions()) {
    return 0;
  }
  if (!scope_apply_descriptor_updates()) {
    return 0;
  }

  if (g_vulkan.vkEndCommandBuffer(g_vulkan.scope_command_buffer) != VK_SUCCESS) {
    set_errorf("vkEndCommandBuffer failed while synchronizing tensor interop");
    return 0;
  }

  if (g_vulkan.scope_has_recorded_work &&
      !submit_and_wait_with_fence(g_vulkan.scope_command_buffer,
                                  "while synchronizing tensor interop")) {
    return 0;
  }

  for (uint32_t i = 0; i < g_vulkan.scope_entry_count; ++i) {
    g_vulkan.scope_entries[i].dirty_output = 0;
    g_vulkan.scope_entries[i].consumed_after_write = 0;
  }

  scope_release_recording_resources();
  if (!scope_begin_recording()) {
    return 0;
  }

  return 1;
}

int npp_gpu_vulkan_engine_export_buffer(const void *host_ptr, size_t byte_size,
                                    void **out_buffer_handle,
                                    size_t *out_buffer_size,
                                    uint32_t *out_queue_family_index,
                                    char *error_buffer, size_t error_size) {
  if (host_ptr == NULL || byte_size == 0 || out_buffer_handle == NULL ||
      out_buffer_size == NULL || out_queue_family_index == NULL) {
    set_errorf("Invalid Vulkan export buffer request");
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  if (!scope_batch_active()) {
    set_errorf("Vulkan tensor interop requires an active batched gpu scope");
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  VulkanScopeBufferEntry *entry =
      scope_find_entry(host_ptr, (VkDeviceSize)byte_size);
  if (entry == NULL || entry->buffer.buffer == VK_NULL_HANDLE) {
    set_errorf("Tensor buffer was not found in the current gpu scope");
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  if ((entry->dirty_output || g_vulkan.scope_has_recorded_work) &&
      !scope_sync_for_external_interop()) {
    copy_error_out(error_buffer, error_size);
    return -1;
  }

  *out_buffer_handle = (void *)entry->buffer.buffer;
  *out_buffer_size = (size_t)entry->size;
  *out_queue_family_index = g_vulkan.queue_family_index;
  clear_error();
  copy_error_out(error_buffer, error_size);
  return 0;
}

#else

int npp_gpu_vulkan_engine_export_buffer(const void *host_ptr, size_t byte_size,
                                    void **out_buffer_handle,
                                    size_t *out_buffer_size,
                                    uint32_t *out_queue_family_index,
                                    char *error_buffer, size_t error_size) {
  (void)host_ptr;
  (void)byte_size;
  (void)out_buffer_handle;
  (void)out_buffer_size;
  (void)out_queue_family_index;
  write_header_missing_error(error_buffer, error_size);
  return -1;
}

#endif






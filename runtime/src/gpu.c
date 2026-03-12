#include "neuron_gpu.h"
#include "neuron_platform.h"
#include "gpu_internal.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NPP_ENABLE_CUDA_BACKEND
#define NPP_ENABLE_CUDA_BACKEND 1
#endif

#ifndef NPP_ENABLE_VULKAN_BACKEND
#define NPP_ENABLE_VULKAN_BACKEND 0
#endif

#ifndef NPP_ENABLE_WEBGPU_BACKEND
#define NPP_ENABLE_WEBGPU_BACKEND 0
#endif

#if NPP_ENABLE_CUDA_BACKEND
typedef int CUresult;
typedef int CUdevice;
typedef struct CUctx_st *CUcontext;
typedef unsigned long long CUdeviceptr;

#define CUDA_SUCCESS 0

typedef CUresult (*PFN_cuInit)(unsigned int flags);
typedef CUresult (*PFN_cuDeviceGetCount)(int *count);
typedef CUresult (*PFN_cuDeviceGet)(CUdevice *device, int ordinal);
typedef CUresult (*PFN_cuCtxCreate)(CUcontext *pctx, unsigned int flags,
                                    CUdevice dev);
typedef CUresult (*PFN_cuCtxDestroy)(CUcontext ctx);
typedef CUresult (*PFN_cuMemAlloc)(CUdeviceptr *dptr, size_t bytesize);
typedef CUresult (*PFN_cuMemFree)(CUdeviceptr dptr);
typedef CUresult (*PFN_cuMemcpyHtoD)(CUdeviceptr dstDevice,
                                     const void *srcHost, size_t ByteCount);
typedef CUresult (*PFN_cuMemcpyDtoH)(void *dstHost, CUdeviceptr srcDevice,
                                     size_t ByteCount);
typedef CUresult (*PFN_cuMemcpyDtoD)(CUdeviceptr dstDevice,
                                     CUdeviceptr srcDevice, size_t ByteCount);
#endif

typedef enum {
  ALLOC_KIND_UNKNOWN = 0,
  ALLOC_KIND_HOST_FALLBACK = 1,
  ALLOC_KIND_DEVICE = 2,
} AllocationKind;

typedef struct AllocationNode {
  void *ptr;
  size_t bytes;
  AllocationKind kind;
  struct AllocationNode *next;
} AllocationNode;

typedef enum {
  FORCED_BACKEND_AUTO = 0,
  FORCED_BACKEND_CPU = 1,
  FORCED_BACKEND_VULKAN = 2,
  FORCED_BACKEND_CUDA = 3,
  FORCED_BACKEND_WEBGPU = 4,
} ForcedBackend;

typedef struct {
  int initialized;
  NeuronGpuBackend backend;
#if NPP_ENABLE_CUDA_BACKEND
  NeuronPlatformLibraryHandle handle;
  CUcontext context;
  PFN_cuInit cuInit;
  PFN_cuDeviceGetCount cuDeviceGetCount;
  PFN_cuDeviceGet cuDeviceGet;
  PFN_cuCtxCreate cuCtxCreate;
  PFN_cuCtxDestroy cuCtxDestroy;
  PFN_cuMemAlloc cuMemAlloc;
  PFN_cuMemFree cuMemFree;
  PFN_cuMemcpyHtoD cuMemcpyHtoD;
  PFN_cuMemcpyDtoH cuMemcpyDtoH;
  PFN_cuMemcpyDtoD cuMemcpyDtoD;
#endif
} GpuRuntimeState;

static GpuRuntimeState g_gpu = {0};
static AllocationNode *g_allocations = NULL;
static NeuronGpuMemoryStats g_memory_stats = {0};
static char g_last_error[512] = {0};
static int g_scope_depth = 0;
static NeuronGpuBackend g_scope_backend = NEURON_GPU_BACKEND_NONE;
static NeuronGpuScopeMode g_pending_scope_mode = NEURON_GPU_SCOPE_MODE_DEFAULT;
static NeuronGpuDeviceClass g_pending_device_class = NEURON_GPU_DEVICE_CLASS_ANY;

static void clear_last_error(void) { g_last_error[0] = '\0'; }

static void set_last_error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
  va_end(args);
}

static int string_equals_ignore_case(const char *lhs, const char *rhs) {
  if (lhs == NULL || rhs == NULL) {
    return 0;
  }
  while (*lhs != '\0' && *rhs != '\0') {
    if (tolower((unsigned char)*lhs) != tolower((unsigned char)*rhs)) {
      return 0;
    }
    ++lhs;
    ++rhs;
  }
  return *lhs == '\0' && *rhs == '\0';
}

static ForcedBackend parse_forced_backend(const char *value, int *invalid_out) {
  if (invalid_out != NULL) {
    *invalid_out = 0;
  }

  if (value == NULL || value[0] == '\0' || string_equals_ignore_case(value, "auto")) {
    return FORCED_BACKEND_AUTO;
  }
  if (string_equals_ignore_case(value, "cpu")) {
    return FORCED_BACKEND_CPU;
  }
  if (string_equals_ignore_case(value, "vulkan")) {
    return FORCED_BACKEND_VULKAN;
  }
  if (string_equals_ignore_case(value, "cuda")) {
    return FORCED_BACKEND_CUDA;
  }
  if (string_equals_ignore_case(value, "webgpu")) {
    return FORCED_BACKEND_WEBGPU;
  }

  if (invalid_out != NULL) {
    *invalid_out = 1;
  }
  return FORCED_BACKEND_AUTO;
}

static int is_valid_scope_mode(NeuronGpuScopeMode mode) {
  return mode == NEURON_GPU_SCOPE_MODE_DEFAULT ||
         mode == NEURON_GPU_SCOPE_MODE_PREFER ||
         mode == NEURON_GPU_SCOPE_MODE_FORCE;
}

static int is_valid_device_class(NeuronGpuDeviceClass device_class) {
  return device_class == NEURON_GPU_DEVICE_CLASS_ANY ||
         device_class == NEURON_GPU_DEVICE_CLASS_DISCRETE ||
         device_class == NEURON_GPU_DEVICE_CLASS_INTEGRATED;
}

#if NPP_ENABLE_CUDA_BACKEND
static void *load_cuda_symbol(const char *name) {
  if (g_gpu.handle == NULL || name == NULL) {
    return NULL;
  }
  return neuron_platform_load_symbol(g_gpu.handle, name);
}

static void *load_cuda_symbol_any(const char *first, const char *second) {
  void *symbol = load_cuda_symbol(first);
  if (symbol == NULL && second != NULL) {
    symbol = load_cuda_symbol(second);
  }
  return symbol;
}

static int open_cuda_driver_library(void) {
  g_gpu.handle = neuron_platform_open_library("nvcuda.dll");
  if (g_gpu.handle == NULL) {
    g_gpu.handle = neuron_platform_open_library("libcuda.so.1");
  }
  if (g_gpu.handle == NULL) {
    g_gpu.handle = neuron_platform_open_library("libcuda.so");
  }
  return g_gpu.handle != NULL;
}

static void close_cuda_driver_library(void) {
  if (g_gpu.handle == NULL) {
    return;
  }
  neuron_platform_close_library(g_gpu.handle);
  g_gpu.handle = NULL;
}

static int load_cuda_driver_symbols(void) {
  g_gpu.cuInit = (PFN_cuInit)load_cuda_symbol("cuInit");
  g_gpu.cuDeviceGetCount =
      (PFN_cuDeviceGetCount)load_cuda_symbol("cuDeviceGetCount");
  g_gpu.cuDeviceGet = (PFN_cuDeviceGet)load_cuda_symbol("cuDeviceGet");
  g_gpu.cuCtxCreate =
      (PFN_cuCtxCreate)load_cuda_symbol_any("cuCtxCreate_v2", "cuCtxCreate");
  g_gpu.cuCtxDestroy =
      (PFN_cuCtxDestroy)load_cuda_symbol_any("cuCtxDestroy_v2", "cuCtxDestroy");
  g_gpu.cuMemAlloc =
      (PFN_cuMemAlloc)load_cuda_symbol_any("cuMemAlloc_v2", "cuMemAlloc");
  g_gpu.cuMemFree =
      (PFN_cuMemFree)load_cuda_symbol_any("cuMemFree_v2", "cuMemFree");
  g_gpu.cuMemcpyHtoD =
      (PFN_cuMemcpyHtoD)load_cuda_symbol_any("cuMemcpyHtoD_v2", "cuMemcpyHtoD");
  g_gpu.cuMemcpyDtoH =
      (PFN_cuMemcpyDtoH)load_cuda_symbol_any("cuMemcpyDtoH_v2", "cuMemcpyDtoH");
  g_gpu.cuMemcpyDtoD =
      (PFN_cuMemcpyDtoD)load_cuda_symbol_any("cuMemcpyDtoD_v2", "cuMemcpyDtoD");

  return g_gpu.cuInit != NULL && g_gpu.cuDeviceGetCount != NULL &&
         g_gpu.cuDeviceGet != NULL && g_gpu.cuCtxCreate != NULL &&
         g_gpu.cuCtxDestroy != NULL && g_gpu.cuMemAlloc != NULL &&
         g_gpu.cuMemFree != NULL && g_gpu.cuMemcpyHtoD != NULL &&
         g_gpu.cuMemcpyDtoH != NULL;
}

static int initialize_cuda_backend(char *error_buffer, size_t error_size) {
  if (!open_cuda_driver_library()) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size,
               "CUDA driver library was not found at runtime");
    }
    return 0;
  }

  if (!load_cuda_driver_symbols()) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size,
               "CUDA driver loaded but required symbols are missing");
    }
    close_cuda_driver_library();
    return 0;
  }

  if (g_gpu.cuInit(0) != CUDA_SUCCESS) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size, "cuInit failed");
    }
    close_cuda_driver_library();
    return 0;
  }

  int device_count = 0;
  CUresult device_count_result = g_gpu.cuDeviceGetCount(&device_count);
  if (device_count_result != CUDA_SUCCESS || device_count <= 0) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size, "No CUDA devices were detected");
    }
    close_cuda_driver_library();
    return 0;
  }

  CUdevice device = 0;
  if (g_gpu.cuDeviceGet(&device, 0) != CUDA_SUCCESS) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size, "cuDeviceGet failed");
    }
    close_cuda_driver_library();
    return 0;
  }

  if (g_gpu.cuCtxCreate(&g_gpu.context, 0, device) != CUDA_SUCCESS) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size, "cuCtxCreate failed");
    }
    close_cuda_driver_library();
    return 0;
  }

  if (error_buffer != NULL && error_size > 0) {
    error_buffer[0] = '\0';
  }
  return 1;
}
#else
static void close_cuda_driver_library(void) {}
#endif

static int try_enable_vulkan_backend(int forced) {
#if NPP_ENABLE_VULKAN_BACKEND
  char error_message[512] = {0};
  if (neuron_gpu_vulkan_try_initialize(error_message, sizeof(error_message)) != 0) {
    g_gpu.backend = NEURON_GPU_BACKEND_VULKAN_COMPUTE;
    clear_last_error();
    return 1;
  }

  if (forced) {
    if (error_message[0] != '\0') {
      set_last_error("%s", error_message);
    } else {
      set_last_error("Vulkan backend initialization failed");
    }
  }
#else
  if (forced) {
    set_last_error("Vulkan backend is disabled at build time");
  }
#endif
  return 0;
}

static int try_enable_webgpu_backend(int forced) {
#if NPP_ENABLE_WEBGPU_BACKEND
  char error_message[512] = {0};
  if (neuron_gpu_webgpu_try_initialize(error_message, sizeof(error_message)) !=
      0) {
    g_gpu.backend = NEURON_GPU_BACKEND_WEBGPU;
    clear_last_error();
    return 1;
  }

  if (forced) {
    if (error_message[0] != '\0') {
      set_last_error("%s", error_message);
    } else {
      set_last_error("WebGPU backend initialization failed");
    }
  }
#else
  if (forced) {
    set_last_error("WebGPU backend is disabled at build time");
  }
#endif
  return 0;
}

static int try_enable_cuda_backend(int forced) {
#if NPP_ENABLE_CUDA_BACKEND
  char error_message[512] = {0};
  if (initialize_cuda_backend(error_message, sizeof(error_message)) != 0) {
    g_gpu.backend = NEURON_GPU_BACKEND_CUDA_DRIVER;
    clear_last_error();
    return 1;
  }

  if (forced) {
    if (error_message[0] != '\0') {
      set_last_error("%s", error_message);
    } else {
      set_last_error("CUDA backend initialization failed");
    }
  }
#else
  if (forced) {
    set_last_error("CUDA backend is disabled at build time");
  }
#endif
  return 0;
}

static void initialize_backend_once(void) {
  if (g_gpu.initialized) {
    return;
  }

  g_gpu.initialized = 1;
  g_gpu.backend = NEURON_GPU_BACKEND_CPU_FALLBACK;
  clear_last_error();

  const char *forced_value = getenv("NEURON_GPU_FORCE_BACKEND");
  int invalid_force = 0;
  ForcedBackend forced = parse_forced_backend(forced_value, &invalid_force);
  if (invalid_force && forced_value != NULL) {
    set_last_error(
        "Unknown NEURON_GPU_FORCE_BACKEND='%s'; using auto policy",
        forced_value);
  }

  switch (forced) {
  case FORCED_BACKEND_CPU:
    clear_last_error();
    return;
  case FORCED_BACKEND_WEBGPU:
    (void)try_enable_webgpu_backend(1);
    return;
  case FORCED_BACKEND_VULKAN:
#if NPP_ENABLE_VULKAN_BACKEND
    neuron_gpu_vulkan_set_scope_preference(g_pending_scope_mode,
                                           g_pending_device_class);
#endif
    (void)try_enable_vulkan_backend(1);
    return;
  case FORCED_BACKEND_CUDA:
    (void)try_enable_cuda_backend(1);
    return;
  case FORCED_BACKEND_AUTO:
  default:
    break;
  }

  if (try_enable_webgpu_backend(0)) {
    return;
  }

  if (g_pending_device_class != NEURON_GPU_DEVICE_CLASS_ANY &&
      (g_pending_scope_mode == NEURON_GPU_SCOPE_MODE_PREFER ||
       g_pending_scope_mode == NEURON_GPU_SCOPE_MODE_FORCE)) {
    if (g_pending_device_class == NEURON_GPU_DEVICE_CLASS_DISCRETE) {
#if NPP_ENABLE_CUDA_BACKEND
      if (try_enable_cuda_backend(
              g_pending_scope_mode == NEURON_GPU_SCOPE_MODE_FORCE ? 1 : 0)) {
        return;
      }
#endif
#if NPP_ENABLE_VULKAN_BACKEND
      neuron_gpu_vulkan_set_scope_preference(g_pending_scope_mode,
                                             g_pending_device_class);
      if (try_enable_vulkan_backend(
              g_pending_scope_mode == NEURON_GPU_SCOPE_MODE_FORCE ? 1 : 0)) {
        return;
      }
#endif
      if (g_pending_scope_mode == NEURON_GPU_SCOPE_MODE_FORCE) {
        if (g_last_error[0] == '\0') {
          set_last_error("Failed to select a discrete GPU backend");
        }
        return;
      }
    } else if (g_pending_device_class == NEURON_GPU_DEVICE_CLASS_INTEGRATED) {
#if NPP_ENABLE_VULKAN_BACKEND
      neuron_gpu_vulkan_set_scope_preference(g_pending_scope_mode,
                                             g_pending_device_class);
      if (try_enable_vulkan_backend(
              g_pending_scope_mode == NEURON_GPU_SCOPE_MODE_FORCE ? 1 : 0)) {
        return;
      }
#endif
      if (g_pending_scope_mode == NEURON_GPU_SCOPE_MODE_FORCE) {
        if (g_last_error[0] == '\0') {
          set_last_error("Failed to select an integrated GPU backend");
        }
        return;
      }
    }
  }

#if NPP_ENABLE_VULKAN_BACKEND
  neuron_gpu_vulkan_set_scope_preference(NEURON_GPU_SCOPE_MODE_DEFAULT,
                                         NEURON_GPU_DEVICE_CLASS_ANY);
#endif

#if NPP_ENABLE_CUDA_BACKEND
  if (try_enable_cuda_backend(0)) {
    return;
  }
#endif

#if NPP_ENABLE_VULKAN_BACKEND
  if (try_enable_vulkan_backend(0)) {
    return;
  }
#endif

  if (!invalid_force) {
    clear_last_error();
  }
}

static AllocationNode *find_allocation_node(const void *ptr,
                                            AllocationNode **prev_out) {
  AllocationNode *prev = NULL;
  AllocationNode *cur = g_allocations;
  while (cur != NULL) {
    if (cur->ptr == ptr) {
      if (prev_out != NULL) {
        *prev_out = prev;
      }
      return cur;
    }
    prev = cur;
    cur = cur->next;
  }
  if (prev_out != NULL) {
    *prev_out = NULL;
  }
  return NULL;
}

static void update_stats_on_register(size_t bytes, AllocationKind kind) {
  g_memory_stats.total_allocations++;
  if (kind == ALLOC_KIND_DEVICE) {
    g_memory_stats.device_allocations++;
    g_memory_stats.device_allocated_bytes += bytes;
  } else if (kind == ALLOC_KIND_HOST_FALLBACK) {
    g_memory_stats.host_fallback_allocations++;
    g_memory_stats.host_fallback_allocated_bytes += bytes;
  }
}

static void update_stats_on_unregister(size_t bytes, AllocationKind kind) {
  if (g_memory_stats.total_allocations > 0) {
    g_memory_stats.total_allocations--;
  }
  if (kind == ALLOC_KIND_DEVICE) {
    if (g_memory_stats.device_allocations > 0) {
      g_memory_stats.device_allocations--;
    }
    if (g_memory_stats.device_allocated_bytes >= bytes) {
      g_memory_stats.device_allocated_bytes -= bytes;
    } else {
      g_memory_stats.device_allocated_bytes = 0;
    }
  } else if (kind == ALLOC_KIND_HOST_FALLBACK) {
    if (g_memory_stats.host_fallback_allocations > 0) {
      g_memory_stats.host_fallback_allocations--;
    }
    if (g_memory_stats.host_fallback_allocated_bytes >= bytes) {
      g_memory_stats.host_fallback_allocated_bytes -= bytes;
    } else {
      g_memory_stats.host_fallback_allocated_bytes = 0;
    }
  }
}

static void register_allocation(void *ptr, size_t bytes, AllocationKind kind) {
  if (ptr == NULL || bytes == 0) {
    return;
  }
  AllocationNode *node = (AllocationNode *)malloc(sizeof(AllocationNode));
  if (node == NULL) {
    return;
  }
  node->ptr = ptr;
  node->bytes = bytes;
  node->kind = kind;
  node->next = g_allocations;
  g_allocations = node;
  update_stats_on_register(bytes, kind);
}

static AllocationKind unregister_allocation(void *ptr, size_t *bytes_out) {
  AllocationNode *prev = NULL;
  AllocationNode *node = find_allocation_node(ptr, &prev);
  if (node == NULL) {
    if (bytes_out != NULL) {
      *bytes_out = 0;
    }
    return ALLOC_KIND_UNKNOWN;
  }

  if (prev == NULL) {
    g_allocations = node->next;
  } else {
    prev->next = node->next;
  }

  if (bytes_out != NULL) {
    *bytes_out = node->bytes;
  }
  AllocationKind kind = node->kind;
  update_stats_on_unregister(node->bytes, kind);
  free(node);
  return kind;
}

static int is_device_pointer(const void *ptr) {
  AllocationNode *node = find_allocation_node(ptr, NULL);
  return node != NULL && node->kind == ALLOC_KIND_DEVICE;
}

static int is_host_fallback_pointer(const void *ptr) {
  AllocationNode *node = find_allocation_node(ptr, NULL);
  return node != NULL && node->kind == ALLOC_KIND_HOST_FALLBACK;
}

static NeuronGpuCopyKind infer_copy_kind(const void *dst, const void *src) {
  int dst_device = is_device_pointer(dst);
  int src_device = is_device_pointer(src);

  if (dst_device && src_device) {
    return NEURON_GPU_MEMCPY_DEVICE_TO_DEVICE;
  }
  if (dst_device) {
    return NEURON_GPU_MEMCPY_HOST_TO_DEVICE;
  }
  if (src_device) {
    return NEURON_GPU_MEMCPY_DEVICE_TO_HOST;
  }
  return NEURON_GPU_MEMCPY_HOST_TO_HOST;
}

NeuronGpuBackend neuron_gpu_backend(void) {
  initialize_backend_once();
  return g_gpu.backend;
}

const char *neuron_gpu_backend_name(void) {
  switch (neuron_gpu_backend()) {
  case NEURON_GPU_BACKEND_WEBGPU:
    return "webgpu";
  case NEURON_GPU_BACKEND_CUDA_DRIVER:
    return "cuda-driver";
  case NEURON_GPU_BACKEND_VULKAN_COMPUTE:
    return "vulkan-compute";
  case NEURON_GPU_BACKEND_CPU_FALLBACK:
    return "cpu-fallback";
  default:
    return "none";
  }
}

const char *neuron_gpu_last_error(void) {
  return (g_last_error[0] == '\0') ? NULL : g_last_error;
}

int neuron_gpu_is_available(void) {
  NeuronGpuBackend backend = neuron_gpu_backend();
  return backend == NEURON_GPU_BACKEND_CUDA_DRIVER ||
         backend == NEURON_GPU_BACKEND_VULKAN_COMPUTE ||
         backend == NEURON_GPU_BACKEND_WEBGPU;
}

int neuron_gpu_supports_op(NeuronGpuOpKind op) {
  initialize_backend_once();
  if (g_gpu.backend == NEURON_GPU_BACKEND_WEBGPU) {
#if NPP_ENABLE_WEBGPU_BACKEND
    return neuron_gpu_webgpu_supports_op(op);
#else
    return 0;
#endif
  }

  if (g_gpu.backend == NEURON_GPU_BACKEND_VULKAN_COMPUTE) {
#if NPP_ENABLE_VULKAN_BACKEND
    return neuron_gpu_vulkan_supports_op(op);
#else
    return 0;
#endif
  }

  if (g_gpu.backend == NEURON_GPU_BACKEND_CUDA_DRIVER) {
#if NPP_ENABLE_CUDA_BACKEND
    return neuron_gpu_cuda_supports_op(op);
#else
    return 0;
#endif
  }

  return 0;
}

int neuron_gpu_dispatch_tensor_binary(NeuronGpuOpKind op, const float *a,
                                      const float *b, float *out,
                                      int32_t element_count) {
  if (a == NULL || b == NULL || out == NULL || element_count <= 0) {
    set_last_error("Invalid tensor binary dispatch arguments");
    return -1;
  }

  initialize_backend_once();
  if (g_gpu.backend == NEURON_GPU_BACKEND_CUDA_DRIVER) {
#if NPP_ENABLE_CUDA_BACKEND
    char cuda_error[512] = {0};
    if (neuron_gpu_cuda_dispatch_binary(op, a, b, out, element_count,
                                        cuda_error, sizeof(cuda_error)) == 0) {
      clear_last_error();
      return 0;
    }
#if NPP_ENABLE_VULKAN_BACKEND
    char vulkan_error[512] = {0};
    if (neuron_gpu_vulkan_try_initialize(vulkan_error, sizeof(vulkan_error)) !=
            0 &&
        neuron_gpu_vulkan_dispatch_binary(op, a, b, out, element_count,
                                          vulkan_error,
                                          sizeof(vulkan_error)) == 0) {
      clear_last_error();
      return 0;
    }
    if (cuda_error[0] != '\0' && vulkan_error[0] != '\0') {
      set_last_error("CUDA dispatch failed (%s); Vulkan fallback failed (%s)",
                     cuda_error, vulkan_error);
    } else if (cuda_error[0] != '\0') {
      set_last_error("%s", cuda_error);
    } else if (vulkan_error[0] != '\0') {
      set_last_error("%s", vulkan_error);
    } else {
      set_last_error(
          "CUDA dispatch failed and Vulkan fallback was unavailable");
    }
    return -1;
#else
    if (cuda_error[0] != '\0') {
      set_last_error("%s", cuda_error);
    } else {
      set_last_error("CUDA tensor binary dispatch failed");
    }
    return -1;
#endif
#else
    set_last_error("CUDA backend is disabled at build time");
    return -1;
#endif
  }

  if (g_gpu.backend == NEURON_GPU_BACKEND_WEBGPU) {
#if NPP_ENABLE_WEBGPU_BACKEND
    if (neuron_gpu_webgpu_dispatch_binary(op, a, b, out, element_count,
                                          g_last_error,
                                          sizeof(g_last_error)) == 0) {
      clear_last_error();
      return 0;
    }
    return -1;
#else
    set_last_error("WebGPU backend is disabled at build time");
    return -1;
#endif
  }

  if (g_gpu.backend == NEURON_GPU_BACKEND_VULKAN_COMPUTE) {
#if NPP_ENABLE_VULKAN_BACKEND
    if (neuron_gpu_vulkan_dispatch_binary(op, a, b, out, element_count,
                                          g_last_error,
                                          sizeof(g_last_error)) == 0) {
      clear_last_error();
      return 0;
    }
    return -1;
#else
    set_last_error("Vulkan backend is disabled at build time");
    return -1;
#endif
  }

  set_last_error("No active GPU backend for tensor binary dispatch");
  return -1;
}

int neuron_gpu_dispatch_tensor_fma(const float *a, const float *b,
                                   const float *c, float *out,
                                   int32_t element_count) {
  if (a == NULL || b == NULL || c == NULL || out == NULL || element_count <= 0) {
    set_last_error("Invalid tensor fma dispatch arguments");
    return -1;
  }

  initialize_backend_once();
  if (g_gpu.backend == NEURON_GPU_BACKEND_CUDA_DRIVER) {
#if NPP_ENABLE_CUDA_BACKEND
    char cuda_error[512] = {0};
    if (neuron_gpu_cuda_dispatch_fma(a, b, c, out, element_count, cuda_error,
                                     sizeof(cuda_error)) == 0) {
      clear_last_error();
      return 0;
    }
#if NPP_ENABLE_VULKAN_BACKEND
    char vulkan_error[512] = {0};
    if (neuron_gpu_vulkan_try_initialize(vulkan_error, sizeof(vulkan_error)) !=
            0 &&
        neuron_gpu_vulkan_dispatch_fma(a, b, c, out, element_count,
                                       vulkan_error,
                                       sizeof(vulkan_error)) == 0) {
      clear_last_error();
      return 0;
    }
    if (cuda_error[0] != '\0' && vulkan_error[0] != '\0') {
      set_last_error("CUDA dispatch failed (%s); Vulkan fallback failed (%s)",
                     cuda_error, vulkan_error);
    } else if (cuda_error[0] != '\0') {
      set_last_error("%s", cuda_error);
    } else if (vulkan_error[0] != '\0') {
      set_last_error("%s", vulkan_error);
    } else {
      set_last_error("CUDA dispatch failed and Vulkan fallback was unavailable");
    }
    return -1;
#else
    if (cuda_error[0] != '\0') {
      set_last_error("%s", cuda_error);
    } else {
      set_last_error("CUDA tensor fma dispatch failed");
    }
    return -1;
#endif
#else
    set_last_error("CUDA backend is disabled at build time");
    return -1;
#endif
  }

  if (g_gpu.backend == NEURON_GPU_BACKEND_WEBGPU) {
#if NPP_ENABLE_WEBGPU_BACKEND
    if (neuron_gpu_webgpu_dispatch_fma(a, b, c, out, element_count, g_last_error,
                                       sizeof(g_last_error)) == 0) {
      clear_last_error();
      return 0;
    }
    return -1;
#else
    set_last_error("WebGPU backend is disabled at build time");
    return -1;
#endif
  }

  if (g_gpu.backend == NEURON_GPU_BACKEND_VULKAN_COMPUTE) {
#if NPP_ENABLE_VULKAN_BACKEND
    if (neuron_gpu_vulkan_dispatch_fma(a, b, c, out, element_count, g_last_error,
                                       sizeof(g_last_error)) == 0) {
      clear_last_error();
      return 0;
    }
    return -1;
#else
    set_last_error("Vulkan backend is disabled at build time");
    return -1;
#endif
  }

  set_last_error("No active GPU backend for tensor fma dispatch");
  return -1;
}

int neuron_gpu_dispatch_tensor_matmul(const NeuronGpuMatMulDispatchDesc *desc) {
  if (desc == NULL || desc->a == NULL || desc->out == NULL || desc->m <= 0 ||
      desc->n <= 0 || desc->k <= 0) {
    set_last_error("Invalid tensor matmul dispatch arguments");
    return -1;
  }
  if (desc->b == NULL && desc->packed_b == NULL) {
    set_last_error("Tensor matmul dispatch requires dense or packed B input");
    return -1;
  }

  initialize_backend_once();
  if (g_gpu.backend == NEURON_GPU_BACKEND_CUDA_DRIVER) {
#if NPP_ENABLE_CUDA_BACKEND
    char cuda_error[512] = {0};
    if (neuron_gpu_cuda_dispatch_matmul(desc, cuda_error, sizeof(cuda_error)) == 0) {
      clear_last_error();
      return 0;
    }
#if NPP_ENABLE_VULKAN_BACKEND
    char vulkan_error[512] = {0};
    if (neuron_gpu_vulkan_try_initialize(vulkan_error, sizeof(vulkan_error)) != 0 &&
        neuron_gpu_vulkan_dispatch_matmul(desc, vulkan_error,
                                          sizeof(vulkan_error)) == 0) {
      clear_last_error();
      return 0;
    }
    if (cuda_error[0] != '\0' && vulkan_error[0] != '\0') {
      set_last_error("CUDA matmul dispatch failed (%s); Vulkan fallback failed (%s)",
                     cuda_error, vulkan_error);
    } else if (cuda_error[0] != '\0') {
      set_last_error("%s", cuda_error);
    } else if (vulkan_error[0] != '\0') {
      set_last_error("%s", vulkan_error);
    } else {
      set_last_error("CUDA matmul dispatch failed and Vulkan fallback was unavailable");
    }
    return -1;
#else
    if (cuda_error[0] != '\0') {
      set_last_error("%s", cuda_error);
    } else {
      set_last_error("CUDA tensor matmul dispatch failed");
    }
    return -1;
#endif
#else
    set_last_error("CUDA backend is disabled at build time");
    return -1;
#endif
  }

  if (g_gpu.backend == NEURON_GPU_BACKEND_WEBGPU) {
#if NPP_ENABLE_WEBGPU_BACKEND
    if (neuron_gpu_webgpu_dispatch_matmul(desc, g_last_error,
                                          sizeof(g_last_error)) == 0) {
      clear_last_error();
      return 0;
    }
    return -1;
#else
    set_last_error("WebGPU backend is disabled at build time");
    return -1;
#endif
  }

  if (g_gpu.backend == NEURON_GPU_BACKEND_VULKAN_COMPUTE) {
#if NPP_ENABLE_VULKAN_BACKEND
    if (neuron_gpu_vulkan_dispatch_matmul(desc, g_last_error,
                                          sizeof(g_last_error)) == 0) {
      clear_last_error();
      return 0;
    }
    return -1;
#else
    set_last_error("Vulkan backend is disabled at build time");
    return -1;
#endif
  }

  set_last_error("No active GPU backend for tensor matmul dispatch");
  return -1;
}

int neuron_gpu_prepare_cpu_tensor(const float *host_data, int32_t element_count) {
  if (host_data == NULL || element_count <= 0) {
    return 0;
  }

  initialize_backend_once();
  if (g_scope_depth <= 0) {
    return 0;
  }

  if (g_scope_backend == NEURON_GPU_BACKEND_VULKAN_COMPUTE) {
#if NPP_ENABLE_VULKAN_BACKEND
    if (neuron_gpu_vulkan_materialize(
            host_data, (size_t)element_count * sizeof(float), g_last_error,
            sizeof(g_last_error)) == 0) {
      clear_last_error();
      return 0;
    }
    return -1;
#else
    set_last_error("Vulkan backend is disabled at build time");
    return -1;
#endif
  }

  return 0;
}

int neuron_gpu_scope_begin_ex(NeuronGpuScopeMode mode,
                              NeuronGpuDeviceClass device_class) {
  if (!is_valid_scope_mode(mode)) {
    set_last_error("Invalid gpu scope mode: %d", (int)mode);
    return -1;
  }
  if (!is_valid_device_class(device_class)) {
    set_last_error("Invalid gpu device class: %d", (int)device_class);
    return -1;
  }

  g_pending_scope_mode = mode;
  g_pending_device_class = device_class;
  initialize_backend_once();
  if (g_scope_depth == 0) {
    if (mode == NEURON_GPU_SCOPE_MODE_FORCE &&
        device_class != NEURON_GPU_DEVICE_CLASS_ANY &&
        g_gpu.backend == NEURON_GPU_BACKEND_CPU_FALLBACK) {
      if (g_last_error[0] == '\0') {
        set_last_error("Forced GPU scope could not select requested device class");
      }
      return -1;
    }
    g_scope_backend = g_gpu.backend;
  }

  if (g_scope_depth == 0 &&
      g_scope_backend == NEURON_GPU_BACKEND_VULKAN_COMPUTE) {
#if NPP_ENABLE_VULKAN_BACKEND
    if (neuron_gpu_vulkan_scope_begin(g_last_error, sizeof(g_last_error)) == 0) {
      g_scope_depth = 1;
      clear_last_error();
      return 0;
    }
    return -1;
#else
    set_last_error("Vulkan backend is disabled at build time");
    return -1;
#endif
  }

  g_scope_depth++;
  clear_last_error();
  return 0;
}

int neuron_gpu_scope_begin(void) {
  return neuron_gpu_scope_begin_ex(NEURON_GPU_SCOPE_MODE_DEFAULT,
                                   NEURON_GPU_DEVICE_CLASS_ANY);
}

int neuron_gpu_scope_end(void) {
  initialize_backend_once();
  if (g_scope_depth <= 0) {
    set_last_error("neuron_gpu_scope_end called without matching scope_begin");
    return -1;
  }

  g_scope_depth--;
  if (g_scope_depth > 0) {
    clear_last_error();
    return 0;
  }

  if (g_scope_backend == NEURON_GPU_BACKEND_VULKAN_COMPUTE) {
#if NPP_ENABLE_VULKAN_BACKEND
    if (neuron_gpu_vulkan_scope_end(g_last_error, sizeof(g_last_error)) == 0) {
      g_scope_backend = NEURON_GPU_BACKEND_NONE;
      clear_last_error();
      return 0;
    }
    return -1;
#else
    set_last_error("Vulkan backend is disabled at build time");
    return -1;
#endif
  }

  g_scope_backend = NEURON_GPU_BACKEND_NONE;
  clear_last_error();
  return 0;
}

int neuron_gpu_scope_is_active(void) { return g_scope_depth > 0 ? 1 : 0; }

void *neuron_gpu_malloc(size_t bytes) {
  if (bytes == 0) {
    set_last_error("neuron_gpu_malloc: bytes must be > 0");
    return NULL;
  }

  initialize_backend_once();

#if NPP_ENABLE_CUDA_BACKEND
  if (g_gpu.backend == NEURON_GPU_BACKEND_CUDA_DRIVER) {
    CUdeviceptr device_ptr = 0;
    CUresult result = g_gpu.cuMemAlloc(&device_ptr, bytes);
    if (result == CUDA_SUCCESS) {
      void *opaque_ptr = (void *)(uintptr_t)device_ptr;
      register_allocation(opaque_ptr, bytes, ALLOC_KIND_DEVICE);
      clear_last_error();
      return opaque_ptr;
    }
    set_last_error("cuMemAlloc failed with error code %d", result);
  }
#endif

  void *host_ptr = malloc(bytes);
  if (host_ptr == NULL) {
    set_last_error("Host fallback allocation failed for %zu bytes", bytes);
    return NULL;
  }

  register_allocation(host_ptr, bytes, ALLOC_KIND_HOST_FALLBACK);
  clear_last_error();
  return host_ptr;
}

void neuron_gpu_free(void *ptr) {
  if (ptr == NULL) {
    return;
  }

  initialize_backend_once();

  size_t bytes = 0;
  AllocationKind kind = unregister_allocation(ptr, &bytes);
  (void)bytes;

#if NPP_ENABLE_CUDA_BACKEND
  if (kind == ALLOC_KIND_DEVICE &&
      g_gpu.backend == NEURON_GPU_BACKEND_CUDA_DRIVER && g_gpu.cuMemFree) {
    CUresult result = g_gpu.cuMemFree((CUdeviceptr)(uintptr_t)ptr);
    if (result != CUDA_SUCCESS) {
      set_last_error("cuMemFree failed with error code %d", result);
    } else {
      clear_last_error();
    }
    return;
  }

  if (kind == ALLOC_KIND_UNKNOWN &&
      g_gpu.backend == NEURON_GPU_BACKEND_CUDA_DRIVER && g_gpu.cuMemFree) {
    CUresult result = g_gpu.cuMemFree((CUdeviceptr)(uintptr_t)ptr);
    if (result == CUDA_SUCCESS) {
      clear_last_error();
      return;
    }
  }
#endif

  free(ptr);
  clear_last_error();
}

int neuron_gpu_memcpy(void *dst, const void *src, size_t bytes,
                      NeuronGpuCopyKind kind) {
  if (dst == NULL || src == NULL) {
    set_last_error("neuron_gpu_memcpy: dst and src must be non-null");
    return -1;
  }
  if (bytes == 0) {
    clear_last_error();
    return 0;
  }

  initialize_backend_once();

  NeuronGpuCopyKind effective_kind = kind;
  if (effective_kind == NEURON_GPU_MEMCPY_AUTO) {
    effective_kind = infer_copy_kind(dst, src);
  }

#if NPP_ENABLE_CUDA_BACKEND
  if (g_gpu.backend == NEURON_GPU_BACKEND_CUDA_DRIVER) {
    CUresult result = CUDA_SUCCESS;
    switch (effective_kind) {
    case NEURON_GPU_MEMCPY_HOST_TO_DEVICE:
      if (is_host_fallback_pointer(dst)) {
        memcpy(dst, src, bytes);
        clear_last_error();
        return 0;
      }
      result = g_gpu.cuMemcpyHtoD((CUdeviceptr)(uintptr_t)dst, src, bytes);
      break;
    case NEURON_GPU_MEMCPY_DEVICE_TO_HOST:
      if (is_host_fallback_pointer(src)) {
        memcpy(dst, src, bytes);
        clear_last_error();
        return 0;
      }
      result = g_gpu.cuMemcpyDtoH(dst, (CUdeviceptr)(uintptr_t)src, bytes);
      break;
    case NEURON_GPU_MEMCPY_DEVICE_TO_DEVICE:
      if (is_host_fallback_pointer(dst) || is_host_fallback_pointer(src)) {
        memcpy(dst, src, bytes);
        clear_last_error();
        return 0;
      }
      if (g_gpu.cuMemcpyDtoD == NULL) {
        set_last_error("cuMemcpyDtoD symbol missing");
        return -1;
      }
      result = g_gpu.cuMemcpyDtoD((CUdeviceptr)(uintptr_t)dst,
                                  (CUdeviceptr)(uintptr_t)src, bytes);
      break;
    case NEURON_GPU_MEMCPY_HOST_TO_HOST:
      memcpy(dst, src, bytes);
      clear_last_error();
      return 0;
    default:
      set_last_error("Unsupported memcpy kind: %d", (int)effective_kind);
      return -1;
    }

    if (result != CUDA_SUCCESS) {
      set_last_error("CUDA memcpy failed (kind=%d, code=%d)",
                     (int)effective_kind, result);
      return -1;
    }
    clear_last_error();
    return 0;
  }
#endif

  memcpy(dst, src, bytes);
  clear_last_error();
  return 0;
}

int neuron_gpu_memcpy_to_device(void *dst_device, const void *src_host,
                                size_t bytes) {
  return neuron_gpu_memcpy(dst_device, src_host, bytes,
                           NEURON_GPU_MEMCPY_HOST_TO_DEVICE);
}

int neuron_gpu_memcpy_to_host(void *dst_host, const void *src_device,
                              size_t bytes) {
  return neuron_gpu_memcpy(dst_host, src_device, bytes,
                           NEURON_GPU_MEMCPY_DEVICE_TO_HOST);
}

void neuron_gpu_get_memory_stats(NeuronGpuMemoryStats *out_stats) {
  if (out_stats == NULL) {
    return;
  }
  *out_stats = g_memory_stats;
}

void neuron_gpu_reset(void) {
  AllocationNode *node = g_allocations;
  while (node != NULL) {
    AllocationNode *next = node->next;
#if NPP_ENABLE_CUDA_BACKEND
    if (node->kind == ALLOC_KIND_DEVICE &&
        g_gpu.backend == NEURON_GPU_BACKEND_CUDA_DRIVER && g_gpu.cuMemFree) {
      g_gpu.cuMemFree((CUdeviceptr)(uintptr_t)node->ptr);
    } else
#endif
        if (node->kind == ALLOC_KIND_HOST_FALLBACK) {
      free(node->ptr);
    }
    free(node);
    node = next;
  }
  g_allocations = NULL;
  memset(&g_memory_stats, 0, sizeof(g_memory_stats));

#if NPP_ENABLE_VULKAN_BACKEND
  if (g_gpu.backend == NEURON_GPU_BACKEND_VULKAN_COMPUTE) {
    neuron_gpu_vulkan_shutdown();
  }
#endif

#if NPP_ENABLE_WEBGPU_BACKEND
  if (g_gpu.backend == NEURON_GPU_BACKEND_WEBGPU) {
    neuron_gpu_webgpu_shutdown();
  }
#endif

#if NPP_ENABLE_CUDA_BACKEND
  neuron_gpu_cuda_shutdown();
#endif

#if NPP_ENABLE_CUDA_BACKEND
  if (g_gpu.backend == NEURON_GPU_BACKEND_CUDA_DRIVER && g_gpu.context &&
      g_gpu.cuCtxDestroy) {
    g_gpu.cuCtxDestroy(g_gpu.context);
  }
#endif

  close_cuda_driver_library();
  memset(&g_gpu, 0, sizeof(g_gpu));
  g_scope_depth = 0;
  g_scope_backend = NEURON_GPU_BACKEND_NONE;
  g_pending_scope_mode = NEURON_GPU_SCOPE_MODE_DEFAULT;
  g_pending_device_class = NEURON_GPU_DEVICE_CLASS_ANY;
  clear_last_error();
}

static int write_kernel(char *buffer, size_t buffer_size, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int written = vsnprintf(buffer, buffer_size, fmt, args);
  va_end(args);

  if (written < 0 || (size_t)written >= buffer_size) {
    set_last_error("Kernel buffer is too small");
    return -1;
  }
  clear_last_error();
  return 0;
}

int neuron_gpu_generate_kernel_into(const char *op_name, int rank, char *buffer,
                                    size_t buffer_size) {
  if (buffer == NULL || buffer_size == 0) {
    set_last_error("Kernel output buffer is null or empty");
    return -1;
  }

  const char *name = (op_name == NULL || op_name[0] == '\0') ? "tensor_op"
                                                              : op_name;
  int normalized_rank = rank < 1 ? 1 : rank;
  const char *backend_name = neuron_gpu_backend_name();

  if (strcmp(name, "tensor_add") == 0 || strcmp(name, "TensorAdd") == 0) {
    return write_kernel(
        buffer, buffer_size,
        "// backend=%s rank=%d\n"
        "extern \"C\" __global__ void tensor_add(float* out, const float* a, "
        "const float* b, int n) {\n"
        "  int i = blockIdx.x * blockDim.x + threadIdx.x;\n"
        "  if (i < n) out[i] = a[i] + b[i];\n"
        "}\n",
        backend_name, normalized_rank);
  }

  if (strcmp(name, "tensor_mul") == 0 || strcmp(name, "TensorMul") == 0) {
    return write_kernel(
        buffer, buffer_size,
        "// backend=%s rank=%d\n"
        "extern \"C\" __global__ void tensor_mul(float* out, const float* a, "
        "const float* b, int n) {\n"
        "  int i = blockIdx.x * blockDim.x + threadIdx.x;\n"
        "  if (i < n) out[i] = a[i] * b[i];\n"
        "}\n",
        backend_name, normalized_rank);
  }

  if (strcmp(name, "tensor_fma") == 0 || strcmp(name, "TensorFMA") == 0) {
    return write_kernel(
        buffer, buffer_size,
        "// backend=%s rank=%d\n"
        "extern \"C\" __global__ void tensor_fma(float* out, const float* a, "
        "const float* b, const float* c, int n) {\n"
        "  int i = blockIdx.x * blockDim.x + threadIdx.x;\n"
        "  if (i < n) out[i] = a[i] * b[i] + c[i];\n"
        "}\n",
        backend_name, normalized_rank);
  }

  if (strcmp(name, "tensor_matmul") == 0 || strcmp(name, "TensorMatMul") == 0) {
    return write_kernel(
        buffer, buffer_size,
        "// backend=%s rank=%d\n"
        "extern \"C\" __global__ void tensor_matmul(float* out, const float* a, "
        "const float* b, int m, int k, int n) {\n"
        "  int row = blockIdx.y * blockDim.y + threadIdx.y;\n"
        "  int col = blockIdx.x * blockDim.x + threadIdx.x;\n"
        "  if (row < m && col < n) {\n"
        "    float sum = 0.0f;\n"
        "    for (int i = 0; i < k; ++i) sum += a[row * k + i] * b[i * n + "
        "col];\n"
        "    out[row * n + col] = sum;\n"
        "  }\n"
        "}\n",
        backend_name, normalized_rank);
  }

  return write_kernel(
      buffer, buffer_size,
      "// backend=%s rank=%d\n"
      "// No specialized kernel template for op '%s'.\n"
      "extern \"C\" __global__ void tensor_generic(float* out, const float* in, "
      "int n) {\n"
      "  int i = blockIdx.x * blockDim.x + threadIdx.x;\n"
      "  if (i < n) out[i] = in[i];\n"
      "}\n",
      backend_name, normalized_rank, name);
}

const char *neuron_gpu_generate_kernel(const char *op_name, int rank) {
  static char kernel_text[2048];
  if (neuron_gpu_generate_kernel_into(op_name, rank, kernel_text,
                                      sizeof(kernel_text)) != 0) {
    snprintf(kernel_text, sizeof(kernel_text),
             "// kernel generation failed: %s",
             g_last_error[0] == '\0' ? "unknown error" : g_last_error);
  }
  return kernel_text;
}

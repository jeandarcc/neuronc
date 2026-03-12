#ifndef NEURON_GPU_H
#define NEURON_GPU_H

#include "neuron_runtime_export.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  NEURON_GPU_BACKEND_NONE = 0,
  NEURON_GPU_BACKEND_CPU_FALLBACK = 1,
  NEURON_GPU_BACKEND_CUDA_DRIVER = 2,
  NEURON_GPU_BACKEND_VULKAN_COMPUTE = 3,
  NEURON_GPU_BACKEND_WEBGPU = 4,
} NeuronGpuBackend;

typedef enum {
  NEURON_GPU_MEMCPY_AUTO = 0,
  NEURON_GPU_MEMCPY_HOST_TO_DEVICE = 1,
  NEURON_GPU_MEMCPY_DEVICE_TO_HOST = 2,
  NEURON_GPU_MEMCPY_DEVICE_TO_DEVICE = 3,
  NEURON_GPU_MEMCPY_HOST_TO_HOST = 4,
} NeuronGpuCopyKind;

typedef enum {
  NEURON_GPU_OP_TENSOR_ADD = 0,
  NEURON_GPU_OP_TENSOR_SUB = 1,
  NEURON_GPU_OP_TENSOR_MUL = 2,
  NEURON_GPU_OP_TENSOR_DIV = 3,
  NEURON_GPU_OP_TENSOR_FMA = 4,
  NEURON_GPU_OP_TENSOR_MATMUL = 5,
} NeuronGpuOpKind;

typedef struct {
  size_t total_allocations;
  size_t device_allocations;
  size_t host_fallback_allocations;
  size_t device_allocated_bytes;
  size_t host_fallback_allocated_bytes;
} NeuronGpuMemoryStats;

typedef enum {
  NEURON_GPU_SCOPE_MODE_DEFAULT = 0,
  NEURON_GPU_SCOPE_MODE_PREFER = 1,
  NEURON_GPU_SCOPE_MODE_FORCE = 2,
} NeuronGpuScopeMode;

typedef enum {
  NEURON_GPU_DEVICE_CLASS_ANY = 0,
  NEURON_GPU_DEVICE_CLASS_DISCRETE = 1,
  NEURON_GPU_DEVICE_CLASS_INTEGRATED = 2,
} NeuronGpuDeviceClass;

NEURON_RUNTIME_API NeuronGpuBackend neuron_gpu_backend(void);
NEURON_RUNTIME_API const char *neuron_gpu_backend_name(void);
NEURON_RUNTIME_API const char *neuron_gpu_last_error(void);
NEURON_RUNTIME_API int neuron_gpu_is_available(void);
NEURON_RUNTIME_API void *neuron_gpu_malloc(size_t bytes);
NEURON_RUNTIME_API void neuron_gpu_free(void *ptr);
NEURON_RUNTIME_API int
neuron_gpu_memcpy(void *dst, const void *src, size_t bytes,
                  NeuronGpuCopyKind kind);
NEURON_RUNTIME_API int neuron_gpu_memcpy_to_device(void *dst_device,
                                                   const void *src_host,
                                                   size_t bytes);
NEURON_RUNTIME_API int neuron_gpu_memcpy_to_host(void *dst_host,
                                                 const void *src_device,
                                                 size_t bytes);
NEURON_RUNTIME_API int neuron_gpu_supports_op(NeuronGpuOpKind op);
NEURON_RUNTIME_API void
neuron_gpu_get_memory_stats(NeuronGpuMemoryStats *out_stats);
NEURON_RUNTIME_API void neuron_gpu_reset(void);
NEURON_RUNTIME_API int neuron_gpu_scope_begin(void);
NEURON_RUNTIME_API int
neuron_gpu_scope_begin_ex(NeuronGpuScopeMode mode,
                          NeuronGpuDeviceClass device_class);
NEURON_RUNTIME_API int neuron_gpu_scope_end(void);
NEURON_RUNTIME_API int neuron_gpu_scope_is_active(void);

// Kernel generation helper: emits backend-specific kernel source text.
NEURON_RUNTIME_API int
neuron_gpu_generate_kernel_into(const char *op_name, int rank, char *buffer,
                                size_t buffer_size);
NEURON_RUNTIME_API const char *neuron_gpu_generate_kernel(const char *op_name,
                                                          int rank);

#ifdef __cplusplus
}
#endif

#endif // NEURON_GPU_H

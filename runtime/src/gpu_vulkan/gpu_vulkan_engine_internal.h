#ifndef NPP_RUNTIME_GPU_VULKAN_ENGINE_INTERNAL_H
#define NPP_RUNTIME_GPU_VULKAN_ENGINE_INTERNAL_H

#include "gpu_internal.h"

int npp_gpu_vulkan_engine_try_initialize(char *error_buffer, size_t error_size);
void npp_gpu_vulkan_engine_set_scope_preference(
    NeuronGpuScopeMode mode, NeuronGpuDeviceClass device_class);
void npp_gpu_vulkan_engine_shutdown(void);
int npp_gpu_vulkan_engine_supports_op(NeuronGpuOpKind op);
int npp_gpu_vulkan_engine_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                          const float *b, float *out,
                                          int32_t element_count,
                                          char *error_buffer,
                                          size_t error_size);
int npp_gpu_vulkan_engine_dispatch_fma(const float *a, const float *b,
                                       const float *c, float *out,
                                       int32_t element_count,
                                       char *error_buffer,
                                       size_t error_size);
int npp_gpu_vulkan_engine_dispatch_matmul(
    const NeuronGpuMatMulDispatchDesc *desc, char *error_buffer,
    size_t error_size);
int npp_gpu_vulkan_engine_scope_begin(char *error_buffer, size_t error_size);
int npp_gpu_vulkan_engine_scope_end(char *error_buffer, size_t error_size);
int npp_gpu_vulkan_engine_materialize(const void *host_ptr, size_t byte_size,
                                      char *error_buffer, size_t error_size);
int npp_gpu_vulkan_engine_export_buffer(const void *host_ptr, size_t byte_size,
                                        void **out_buffer_handle,
                                        size_t *out_buffer_size,
                                        uint32_t *out_queue_family_index,
                                        char *error_buffer,
                                        size_t error_size);

#endif

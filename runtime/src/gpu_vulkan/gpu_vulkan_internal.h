#ifndef Neuron_RUNTIME_GPU_VULKAN_INTERNAL_H
#define Neuron_RUNTIME_GPU_VULKAN_INTERNAL_H

#include "gpu_internal.h"

int npp_gpu_vulkan_try_initialize_impl(char *error_buffer, size_t error_size);
void npp_gpu_vulkan_set_scope_preference_impl(NeuronGpuScopeMode mode,
                                              NeuronGpuDeviceClass device_class);
void npp_gpu_vulkan_shutdown_impl(void);
int npp_gpu_vulkan_supports_op_impl(NeuronGpuOpKind op);
int npp_gpu_vulkan_dispatch_binary_impl(NeuronGpuOpKind op, const float *a,
                                        const float *b, float *out,
                                        int32_t element_count,
                                        char *error_buffer,
                                        size_t error_size);
int npp_gpu_vulkan_dispatch_fma_impl(const float *a, const float *b,
                                     const float *c, float *out,
                                     int32_t element_count,
                                     char *error_buffer,
                                     size_t error_size);
int npp_gpu_vulkan_dispatch_matmul_impl(
    const NeuronGpuMatMulDispatchDesc *desc, char *error_buffer,
    size_t error_size);
int npp_gpu_vulkan_scope_begin_impl(char *error_buffer, size_t error_size);
int npp_gpu_vulkan_scope_end_impl(char *error_buffer, size_t error_size);
int npp_gpu_vulkan_materialize_impl(const void *host_ptr, size_t byte_size,
                                    char *error_buffer, size_t error_size);
int npp_gpu_vulkan_export_buffer_impl(const void *host_ptr, size_t byte_size,
                                      void **out_buffer_handle,
                                      size_t *out_buffer_size,
                                      uint32_t *out_queue_family_index,
                                      char *error_buffer, size_t error_size);

#endif

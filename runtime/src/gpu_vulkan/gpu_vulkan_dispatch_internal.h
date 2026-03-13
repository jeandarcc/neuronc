#ifndef Neuron_RUNTIME_GPU_VULKAN_DISPATCH_INTERNAL_H
#define Neuron_RUNTIME_GPU_VULKAN_DISPATCH_INTERNAL_H

#include "gpu_internal.h"

int npp_gpu_vulkan_core_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                        const float *b, float *out,
                                        int32_t element_count,
                                        char *error_buffer,
                                        size_t error_size);
int npp_gpu_vulkan_core_dispatch_fma(const float *a, const float *b,
                                     const float *c, float *out,
                                     int32_t element_count,
                                     char *error_buffer,
                                     size_t error_size);
int npp_gpu_vulkan_core_dispatch_matmul(
    const NeuronGpuMatMulDispatchDesc *desc, char *error_buffer,
    size_t error_size);

#endif

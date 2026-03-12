#ifndef NPP_RUNTIME_GPU_VULKAN_INIT_INTERNAL_H
#define NPP_RUNTIME_GPU_VULKAN_INIT_INTERNAL_H

#include "gpu_internal.h"

int npp_gpu_vulkan_core_try_initialize(char *error_buffer, size_t error_size);
void npp_gpu_vulkan_core_set_scope_preference(
    NeuronGpuScopeMode mode, NeuronGpuDeviceClass device_class);
void npp_gpu_vulkan_core_shutdown(void);
int npp_gpu_vulkan_core_supports_op(NeuronGpuOpKind op);

#endif

#ifndef Neuron_RUNTIME_GPU_VULKAN_SCOPE_INTERNAL_H
#define Neuron_RUNTIME_GPU_VULKAN_SCOPE_INTERNAL_H

#include <stddef.h>

int npp_gpu_vulkan_core_scope_begin(char *error_buffer, size_t error_size);
int npp_gpu_vulkan_core_scope_end(char *error_buffer, size_t error_size);
int npp_gpu_vulkan_core_materialize(const void *host_ptr, size_t byte_size,
                                    char *error_buffer, size_t error_size);

#endif

#ifndef NPP_RUNTIME_GPU_VULKAN_INTEROP_INTERNAL_H
#define NPP_RUNTIME_GPU_VULKAN_INTEROP_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

int npp_gpu_vulkan_core_export_buffer(const void *host_ptr, size_t byte_size,
                                      void **out_buffer_handle,
                                      size_t *out_buffer_size,
                                      uint32_t *out_queue_family_index,
                                      char *error_buffer, size_t error_size);

#endif

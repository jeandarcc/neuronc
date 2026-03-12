#include "gpu_vulkan_scope_internal.h"
#include "gpu_vulkan_internal.h"

int npp_gpu_vulkan_scope_begin_impl(char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_core_scope_begin(error_buffer, error_size);
}

int npp_gpu_vulkan_scope_end_impl(char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_core_scope_end(error_buffer, error_size);
}

int npp_gpu_vulkan_materialize_impl(const void *host_ptr, size_t byte_size,
                                    char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_core_materialize(host_ptr, byte_size, error_buffer,
                                         error_size);
}

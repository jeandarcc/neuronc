#include "gpu_vulkan_interop_internal.h"
#include "gpu_vulkan_internal.h"

int npp_gpu_vulkan_export_buffer_impl(const void *host_ptr, size_t byte_size,
                                      void **out_buffer_handle,
                                      size_t *out_buffer_size,
                                      uint32_t *out_queue_family_index,
                                      char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_core_export_buffer(
      host_ptr, byte_size, out_buffer_handle, out_buffer_size,
      out_queue_family_index, error_buffer, error_size);
}

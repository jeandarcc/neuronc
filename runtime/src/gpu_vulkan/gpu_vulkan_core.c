#include "gpu_vulkan_dispatch_internal.h"
#include "gpu_vulkan_engine_internal.h"
#include "gpu_vulkan_init_internal.h"
#include "gpu_vulkan_interop_internal.h"
#include "gpu_vulkan_scope_internal.h"

int npp_gpu_vulkan_core_try_initialize(char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_engine_try_initialize(error_buffer, error_size);
}

void npp_gpu_vulkan_core_set_scope_preference(NeuronGpuScopeMode mode,
                                              NeuronGpuDeviceClass device_class) {
  npp_gpu_vulkan_engine_set_scope_preference(mode, device_class);
}

void npp_gpu_vulkan_core_shutdown(void) { npp_gpu_vulkan_engine_shutdown(); }

int npp_gpu_vulkan_core_supports_op(NeuronGpuOpKind op) {
  return npp_gpu_vulkan_engine_supports_op(op);
}

int npp_gpu_vulkan_core_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                        const float *b, float *out,
                                        int32_t element_count,
                                        char *error_buffer,
                                        size_t error_size) {
  return npp_gpu_vulkan_engine_dispatch_binary(op, a, b, out, element_count,
                                               error_buffer, error_size);
}

int npp_gpu_vulkan_core_dispatch_fma(const float *a, const float *b,
                                     const float *c, float *out,
                                     int32_t element_count,
                                     char *error_buffer,
                                     size_t error_size) {
  return npp_gpu_vulkan_engine_dispatch_fma(a, b, c, out, element_count,
                                            error_buffer, error_size);
}

int npp_gpu_vulkan_core_dispatch_matmul(
    const NeuronGpuMatMulDispatchDesc *desc, char *error_buffer,
    size_t error_size) {
  return npp_gpu_vulkan_engine_dispatch_matmul(desc, error_buffer, error_size);
}

int npp_gpu_vulkan_core_scope_begin(char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_engine_scope_begin(error_buffer, error_size);
}

int npp_gpu_vulkan_core_scope_end(char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_engine_scope_end(error_buffer, error_size);
}

int npp_gpu_vulkan_core_materialize(const void *host_ptr, size_t byte_size,
                                    char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_engine_materialize(host_ptr, byte_size, error_buffer,
                                           error_size);
}

int npp_gpu_vulkan_core_export_buffer(const void *host_ptr, size_t byte_size,
                                      void **out_buffer_handle,
                                      size_t *out_buffer_size,
                                      uint32_t *out_queue_family_index,
                                      char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_engine_export_buffer(
      host_ptr, byte_size, out_buffer_handle, out_buffer_size,
      out_queue_family_index, error_buffer, error_size);
}

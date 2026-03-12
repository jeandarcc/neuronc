#include "gpu_vulkan_internal.h"

int neuron_gpu_vulkan_try_initialize(char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_try_initialize_impl(error_buffer, error_size);
}

void neuron_gpu_vulkan_set_scope_preference(NeuronGpuScopeMode mode,
                                            NeuronGpuDeviceClass device_class) {
  npp_gpu_vulkan_set_scope_preference_impl(mode, device_class);
}

void neuron_gpu_vulkan_shutdown(void) { npp_gpu_vulkan_shutdown_impl(); }

int neuron_gpu_vulkan_supports_op(NeuronGpuOpKind op) {
  return npp_gpu_vulkan_supports_op_impl(op);
}

int neuron_gpu_vulkan_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                      const float *b, float *out,
                                      int32_t element_count, char *error_buffer,
                                      size_t error_size) {
  return npp_gpu_vulkan_dispatch_binary_impl(op, a, b, out, element_count,
                                             error_buffer, error_size);
}

int neuron_gpu_vulkan_dispatch_fma(const float *a, const float *b,
                                   const float *c, float *out,
                                   int32_t element_count, char *error_buffer,
                                   size_t error_size) {
  return npp_gpu_vulkan_dispatch_fma_impl(a, b, c, out, element_count,
                                          error_buffer, error_size);
}

int neuron_gpu_vulkan_dispatch_matmul(const NeuronGpuMatMulDispatchDesc *desc,
                                      char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_dispatch_matmul_impl(desc, error_buffer, error_size);
}

int neuron_gpu_vulkan_scope_begin(char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_scope_begin_impl(error_buffer, error_size);
}

int neuron_gpu_vulkan_scope_end(char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_scope_end_impl(error_buffer, error_size);
}

int neuron_gpu_vulkan_materialize(const void *host_ptr, size_t byte_size,
                                  char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_materialize_impl(host_ptr, byte_size, error_buffer,
                                         error_size);
}

int neuron_gpu_vulkan_export_buffer(const void *host_ptr, size_t byte_size,
                                    void **out_buffer_handle,
                                    size_t *out_buffer_size,
                                    uint32_t *out_queue_family_index,
                                    char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_export_buffer_impl(
      host_ptr, byte_size, out_buffer_handle, out_buffer_size,
      out_queue_family_index, error_buffer, error_size);
}

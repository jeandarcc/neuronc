#include "gpu_vulkan_dispatch_internal.h"
#include "gpu_vulkan_internal.h"

int npp_gpu_vulkan_dispatch_binary_impl(NeuronGpuOpKind op, const float *a,
                                        const float *b, float *out,
                                        int32_t element_count,
                                        char *error_buffer,
                                        size_t error_size) {
  return npp_gpu_vulkan_core_dispatch_binary(op, a, b, out, element_count,
                                             error_buffer, error_size);
}

int npp_gpu_vulkan_dispatch_fma_impl(const float *a, const float *b,
                                     const float *c, float *out,
                                     int32_t element_count,
                                     char *error_buffer,
                                     size_t error_size) {
  return npp_gpu_vulkan_core_dispatch_fma(a, b, c, out, element_count,
                                          error_buffer, error_size);
}

int npp_gpu_vulkan_dispatch_matmul_impl(
    const NeuronGpuMatMulDispatchDesc *desc, char *error_buffer,
    size_t error_size) {
  return npp_gpu_vulkan_core_dispatch_matmul(desc, error_buffer, error_size);
}

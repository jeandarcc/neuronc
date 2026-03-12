#include "gpu_vulkan_init_internal.h"
#include "gpu_vulkan_internal.h"

int npp_gpu_vulkan_try_initialize_impl(char *error_buffer, size_t error_size) {
  return npp_gpu_vulkan_core_try_initialize(error_buffer, error_size);
}

void npp_gpu_vulkan_set_scope_preference_impl(NeuronGpuScopeMode mode,
                                              NeuronGpuDeviceClass device_class) {
  npp_gpu_vulkan_core_set_scope_preference(mode, device_class);
}

void npp_gpu_vulkan_shutdown_impl(void) { npp_gpu_vulkan_core_shutdown(); }

int npp_gpu_vulkan_supports_op_impl(NeuronGpuOpKind op) {
  return npp_gpu_vulkan_core_supports_op(op);
}

#include "graphics/backend/vulkan/graphics_vk_internal.h"

#if NPP_GRAPHICS_VK_ENABLED
int neuron_graphics_backend_set_tensor_interop(NeuronGraphicsBackend *backend,
                                               NeuronTensor *tensor) {
  if (backend == NULL || tensor == NULL || tensor->data == NULL ||
      tensor->size <= 0 || tensor->element_size <= 0) {
    return 0;
  }

  const size_t byte_size = (size_t)tensor->size * (size_t)tensor->element_size;
  void *buffer_handle = NULL;
  size_t buffer_size = 0;
  uint32_t queue_family_index = 0;
  char error_buffer[256] = {0};
  if (neuron_gpu_vulkan_export_buffer(tensor->data, byte_size, &buffer_handle,
                                      &buffer_size, &queue_family_index,
                                      error_buffer,
                                      sizeof(error_buffer)) != 0) {
    neuron_graphics_set_error(
        "Failed to export tensor buffer for graphics interop: %s",
        error_buffer[0] != '\0' ? error_buffer : "unknown");
    return 0;
  }

  backend->tensor_interop_buffer = (VkBuffer)buffer_handle;
  backend->tensor_interop_size = (VkDeviceSize)buffer_size;
  backend->tensor_interop_source_queue_family = queue_family_index;
  backend->tensor_interop_requires_ownership_transfer =
      queue_family_index != backend->graphics_queue_family;
  backend->tensor_interop_ready = 1;
  return 1;
}
#endif

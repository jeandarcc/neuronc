#include "graphics/backend/graphics_backend_internal.h"
#include "graphics/graphics_core_internal.h"
#include "vulkan_common.h"

#ifndef Neuron_ENABLE_VULKAN_BACKEND
#define Neuron_ENABLE_VULKAN_BACKEND 0
#endif

#if !defined(__EMSCRIPTEN__) && (!defined(_WIN32) || !Neuron_VK_COMMON_HAS_HEADERS || !Neuron_ENABLE_VULKAN_BACKEND)
#include <stdlib.h>

struct NeuronGraphicsBackend {
  int unsupported;
};

NeuronGraphicsBackend *neuron_graphics_backend_create(NeuronGraphicsWindow *window) {
  (void)window;
#if defined(_WIN32)
  neuron_graphics_set_error(
      "Vulkan headers not available at build time; graphics backend disabled");
#else
  neuron_graphics_set_error(
      "Graphics backend is available only on Windows for this MVP");
#endif
  return NULL;
}

void neuron_graphics_backend_destroy(NeuronGraphicsBackend *backend) {
  free(backend);
}

void neuron_graphics_backend_mark_resize(NeuronGraphicsBackend *backend) {
  (void)backend;
}

int neuron_graphics_backend_begin_frame(NeuronGraphicsBackend *backend) {
  (void)backend;
  return 0;
}

int neuron_graphics_backend_present(NeuronGraphicsBackend *backend) {
  (void)backend;
  return 0;
}

int neuron_graphics_backend_set_tensor_interop(NeuronGraphicsBackend *backend,
                                               NeuronTensor *tensor) {
  (void)backend;
  (void)tensor;
  neuron_graphics_set_error(
      "Tensor interop draw is unavailable without Vulkan graphics backend");
  return 0;
}
#endif

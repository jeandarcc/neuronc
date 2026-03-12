#include "platform/platform_manager_internal.h"

#include <stdint.h>

#if defined(__EMSCRIPTEN__)
int neuron_platform_create_window_impl(const char *title, int32_t width,
                                       int32_t height,
                                       NeuronPlatformWindowHandle *out_window) {
  (void)title;
  (void)width;
  (void)height;
  if (out_window == NULL) {
    neuron_platform_set_last_error("create_window: out_window is null");
    return 0;
  }
  out_window->native_handle = (void *)(uintptr_t)1u;
  return 1;
}

void neuron_platform_destroy_window_impl(NeuronPlatformWindowHandle window) {
  (void)window;
}

int32_t neuron_platform_pump_events_impl(NeuronPlatformWindowHandle window) {
  (void)window;
  return 1;
}

int neuron_platform_request_surface_impl(NeuronPlatformWindowHandle window,
                                         void **out_surface_handle) {
  if (out_surface_handle == NULL) {
    neuron_platform_set_last_error(
        "request_surface: out_surface_handle is null");
    return 0;
  }
  *out_surface_handle = window.native_handle;
  return 1;
}

int neuron_platform_window_should_close_impl(NeuronPlatformWindowHandle window) {
  (void)window;
  return 0;
}
#endif
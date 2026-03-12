#include "platform/platform_manager_internal.h"

#if defined(__linux__)
int neuron_platform_create_window_impl(const char *title, int32_t width,
                                       int32_t height,
                                       NeuronPlatformWindowHandle *out_window) {
  (void)title;
  (void)width;
  (void)height;
  if (out_window != NULL) {
    out_window->native_handle = NULL;
  }
  neuron_platform_set_last_error(
      "window capability is not implemented for Linux/X11 in this iteration");
  return 0;
}

void neuron_platform_destroy_window_impl(NeuronPlatformWindowHandle window) {
  (void)window;
}

int32_t neuron_platform_pump_events_impl(NeuronPlatformWindowHandle window) {
  (void)window;
  return 0;
}

int neuron_platform_request_surface_impl(NeuronPlatformWindowHandle window,
                                         void **out_surface_handle) {
  (void)window;
  if (out_surface_handle != NULL) {
    *out_surface_handle = NULL;
  }
  neuron_platform_set_last_error(
      "surface capability is not implemented for Linux/X11 in this iteration");
  return 0;
}

int neuron_platform_window_should_close_impl(NeuronPlatformWindowHandle window) {
  (void)window;
  return 0;
}
#endif


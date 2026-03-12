#include "platform/platform_manager_internal.h"

#if defined(__EMSCRIPTEN__)
NeuronPlatformLibraryHandle neuron_platform_open_library_impl(const char *path) {
  (void)path;
  neuron_platform_set_last_error(
      "dynamic library loading is unsupported on web target");
  return NULL;
}

void neuron_platform_close_library_impl(NeuronPlatformLibraryHandle handle) {
  (void)handle;
}

void *neuron_platform_load_symbol_impl(NeuronPlatformLibraryHandle handle,
                                       const char *symbol_name) {
  (void)handle;
  (void)symbol_name;
  neuron_platform_set_last_error(
      "symbol lookup is unsupported on web target");
  return NULL;
}
#endif
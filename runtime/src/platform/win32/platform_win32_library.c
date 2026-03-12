#include "platform/platform_manager_internal.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

NeuronPlatformLibraryHandle neuron_platform_open_library_impl(const char *path) {
  if (path == NULL || path[0] == '\0') {
    neuron_platform_set_last_error("library path is empty");
    return NULL;
  }
  HMODULE handle = LoadLibraryA(path);
  if (handle == NULL) {
    neuron_platform_set_last_error("failed to load library: %s", path);
    return NULL;
  }
  return (NeuronPlatformLibraryHandle)handle;
}

void neuron_platform_close_library_impl(NeuronPlatformLibraryHandle handle) {
  if (handle != NULL) {
    FreeLibrary((HMODULE)handle);
  }
}

void *neuron_platform_load_symbol_impl(NeuronPlatformLibraryHandle handle,
                                       const char *symbol_name) {
  if (handle == NULL || symbol_name == NULL || symbol_name[0] == '\0') {
    neuron_platform_set_last_error("invalid library handle or symbol name");
    return NULL;
  }
  void *symbol = (void *)GetProcAddress((HMODULE)handle, symbol_name);
  if (symbol == NULL) {
    neuron_platform_set_last_error("failed to load symbol: %s", symbol_name);
  }
  return symbol;
}
#endif


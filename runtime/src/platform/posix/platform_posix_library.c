#include "platform/platform_manager_internal.h"

#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>

NeuronPlatformLibraryHandle neuron_platform_open_library_impl(const char *path) {
  if (path == NULL || path[0] == '\0') {
    neuron_platform_set_last_error("library path is empty");
    return NULL;
  }
  void *handle = dlopen(path, RTLD_LAZY);
  if (handle == NULL) {
    const char *error = dlerror();
    neuron_platform_set_last_error("failed to load library '%s': %s", path,
                                   error != NULL ? error : "unknown error");
    return NULL;
  }
  return handle;
}

void neuron_platform_close_library_impl(NeuronPlatformLibraryHandle handle) {
  if (handle != NULL) {
    dlclose(handle);
  }
}

void *neuron_platform_load_symbol_impl(NeuronPlatformLibraryHandle handle,
                                       const char *symbol_name) {
  if (handle == NULL || symbol_name == NULL || symbol_name[0] == '\0') {
    neuron_platform_set_last_error("invalid library handle or symbol name");
    return NULL;
  }
  dlerror();
  void *symbol = dlsym(handle, symbol_name);
  const char *error = dlerror();
  if (error != NULL) {
    neuron_platform_set_last_error("failed to load symbol '%s': %s",
                                   symbol_name, error);
    return NULL;
  }
  return symbol;
}
#endif


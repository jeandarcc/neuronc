#include "platform/platform_manager_internal.h"

#include <stdlib.h>

#if defined(__EMSCRIPTEN__)
const char *neuron_platform_get_env_impl(const char *name) {
  if (name == NULL || name[0] == '\0') {
    return NULL;
  }
  return getenv(name);
}
#endif
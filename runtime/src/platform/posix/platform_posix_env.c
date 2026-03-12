#include "platform/platform_manager_internal.h"

#if defined(__linux__) || defined(__APPLE__)
#include <stdlib.h>

const char *neuron_platform_get_env_impl(const char *name) {
  return name != NULL ? getenv(name) : NULL;
}
#endif


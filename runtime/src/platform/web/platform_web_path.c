#include "platform/platform_manager_internal.h"

#if defined(__EMSCRIPTEN__)
#include <string.h>

char neuron_platform_path_separator_impl(void) { return '/'; }

int neuron_platform_current_working_directory_impl(char *buffer,
                                                   size_t buffer_size) {
  if (buffer == NULL || buffer_size == 0) {
    neuron_platform_set_last_error("cwd buffer is invalid");
    return 0;
  }
  if (buffer_size < 2) {
    neuron_platform_set_last_error("cwd buffer is too small");
    return 0;
  }
  buffer[0] = '/';
  buffer[1] = '\0';
  return 1;
}

int neuron_platform_current_executable_path_impl(char *buffer,
                                                 size_t buffer_size) {
  if (buffer == NULL || buffer_size == 0) {
    neuron_platform_set_last_error("executable path buffer is invalid");
    return 0;
  }
  const char *virtualPath = "/neuron_web_app";
  const size_t required = strlen(virtualPath) + 1;
  if (buffer_size < required) {
    neuron_platform_set_last_error(
        "executable path buffer too small; required %u bytes",
        (unsigned)required);
    return 0;
  }
  memcpy(buffer, virtualPath, required);
  return 1;
}
#endif
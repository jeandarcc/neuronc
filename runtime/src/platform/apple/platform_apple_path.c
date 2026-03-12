#include "platform/platform_manager_internal.h"

#if defined(__APPLE__)
#include <limits.h>
#include <unistd.h>
#include <mach-o/dyld.h>
#include <stdint.h>

char neuron_platform_path_separator_impl(void) { return '/'; }

int neuron_platform_current_working_directory_impl(char *buffer,
                                                   size_t buffer_size) {
  if (buffer == NULL || buffer_size == 0) {
    neuron_platform_set_last_error("cwd buffer is invalid");
    return 0;
  }
  if (getcwd(buffer, buffer_size) == NULL) {
    neuron_platform_set_last_error("failed to query current working directory");
    return 0;
  }
  return 1;
}

int neuron_platform_current_executable_path_impl(char *buffer,
                                                 size_t buffer_size) {
  if (buffer == NULL || buffer_size == 0) {
    neuron_platform_set_last_error("executable path buffer is invalid");
    return 0;
  }
  uint32_t size = (uint32_t)buffer_size;
  if (_NSGetExecutablePath(buffer, &size) != 0) {
    neuron_platform_set_last_error(
        "executable path buffer too small; required %u bytes", (unsigned)size);
    return 0;
  }
  return 1;
}
#endif


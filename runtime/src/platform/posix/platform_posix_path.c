#include "platform/platform_manager_internal.h"

#if defined(__linux__) || defined(__APPLE__)
#include <limits.h>
#include <unistd.h>

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
#if defined(__linux__)
  ssize_t written = readlink("/proc/self/exe", buffer, buffer_size - 1);
  if (written < 0 || (size_t)written >= buffer_size - 1) {
    neuron_platform_set_last_error("failed to query executable path");
    return 0;
  }
  buffer[written] = '\0';
  return 1;
#else
  neuron_platform_set_last_error(
      "executable path query is not implemented for this POSIX target yet");
  return 0;
#endif
}
#endif


#include "platform/platform_manager_internal.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <direct.h>
#include <stdlib.h>

char neuron_platform_path_separator_impl(void) { return '\\'; }

int neuron_platform_current_working_directory_impl(char *buffer,
                                                   size_t buffer_size) {
  if (buffer == NULL || buffer_size == 0) {
    neuron_platform_set_last_error("cwd buffer is invalid");
    return 0;
  }
  if (_getcwd(buffer, (int)buffer_size) == NULL) {
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

  DWORD written = GetModuleFileNameA(NULL, buffer, (DWORD)buffer_size);
  if (written == 0 || written >= (DWORD)buffer_size) {
    neuron_platform_set_last_error("failed to query executable path");
    return 0;
  }
  return 1;
}
#endif


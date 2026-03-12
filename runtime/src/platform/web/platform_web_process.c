#include "platform/platform_manager_internal.h"

#if defined(__EMSCRIPTEN__)
int neuron_platform_spawn_process_impl(const char *command_line,
                                       NeuronPlatformProcessHandle *out_process) {
  (void)command_line;
  (void)out_process;
  neuron_platform_set_last_error(
      "process spawning is unsupported on web target");
  return 0;
}

int neuron_platform_wait_process_impl(NeuronPlatformProcessHandle process,
                                      int32_t *out_exit_code) {
  (void)process;
  (void)out_exit_code;
  neuron_platform_set_last_error("process wait is unsupported on web target");
  return 0;
}

int neuron_platform_terminate_process_impl(NeuronPlatformProcessHandle process) {
  (void)process;
  neuron_platform_set_last_error(
      "process termination is unsupported on web target");
  return 0;
}
#endif
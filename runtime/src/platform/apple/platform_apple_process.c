#include "platform/platform_manager_internal.h"

#if defined(__APPLE__)
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* macOS: identical POSIX semantics — /bin/sh dispatch */
int neuron_platform_spawn_process_impl(const char *command_line,
                                       NeuronPlatformProcessHandle *out_process) {
  if (command_line == NULL || command_line[0] == '\0') {
    neuron_platform_set_last_error("spawn_process: command_line is empty");
    return 0;
  }

  char *const argv[] = {
      (char *)"sh",
      (char *)"-c",
      (char *)command_line,
      NULL
  };

  pid_t pid = 0;
  int rc = posix_spawn(&pid, "/bin/sh", NULL, NULL, argv, NULL);
  if (rc != 0) {
    neuron_platform_set_last_error("spawn_process: posix_spawn failed: %s",
                                   strerror(rc));
    return 0;
  }

  if (out_process != NULL) {
    out_process->process_id = (int64_t)pid;
    out_process->native_handle = (void *)(intptr_t)pid;
  }
  return 1;
}

int neuron_platform_wait_process_impl(NeuronPlatformProcessHandle process,
                                      int32_t *out_exit_code) {
  pid_t pid = (pid_t)(intptr_t)process.native_handle;
  if (pid <= 0) {
    neuron_platform_set_last_error("wait_process: invalid pid");
    return 0;
  }

  int status = 0;
  pid_t result;
  do {
    result = waitpid(pid, &status, 0);
  } while (result == -1 && errno == EINTR);

  if (result == -1) {
    neuron_platform_set_last_error("wait_process: waitpid failed: %s",
                                   strerror(errno));
    return 0;
  }

  if (out_exit_code != NULL) {
    if (WIFEXITED(status)) {
      *out_exit_code = (int32_t)WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      *out_exit_code = -(int32_t)WTERMSIG(status);
    } else {
      *out_exit_code = -1;
    }
  }
  return 1;
}

int neuron_platform_terminate_process_impl(NeuronPlatformProcessHandle process) {
  pid_t pid = (pid_t)(intptr_t)process.native_handle;
  if (pid <= 0) {
    neuron_platform_set_last_error("terminate_process: invalid pid");
    return 0;
  }
  if (kill(pid, SIGTERM) != 0) {
    neuron_platform_set_last_error("terminate_process: kill(SIGTERM) failed: %s",
                                   strerror(errno));
    return 0;
  }
  return 1;
}
#endif


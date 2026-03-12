#include "platform/platform_manager_internal.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

int neuron_platform_spawn_process_impl(const char *command_line,
                                       NeuronPlatformProcessHandle *out_process) {
  if (command_line == NULL || command_line[0] == '\0') {
    neuron_platform_set_last_error("spawn_process: command_line is empty");
    return 0;
  }

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));

  /* CreateProcessA modifies lpCommandLine in-place; we need a writable copy */
  char buffer[32768];
  int written = snprintf(buffer, sizeof(buffer), "%s", command_line);
  if (written < 0 || (size_t)written >= sizeof(buffer)) {
    neuron_platform_set_last_error("spawn_process: command line too long");
    return 0;
  }

  if (!CreateProcessA(NULL, buffer, NULL, NULL, FALSE,
                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
    neuron_platform_set_last_error("spawn_process: CreateProcessA failed (error %lu)",
                                   (unsigned long)GetLastError());
    return 0;
  }

  /* Close the thread handle immediately; we only track the process handle */
  CloseHandle(pi.hThread);

  if (out_process != NULL) {
    out_process->process_id = (int64_t)pi.dwProcessId;
    out_process->native_handle = (void *)pi.hProcess;
  } else {
    CloseHandle(pi.hProcess);
  }
  return 1;
}

int neuron_platform_wait_process_impl(NeuronPlatformProcessHandle process,
                                      int32_t *out_exit_code) {
  HANDLE hProcess = (HANDLE)process.native_handle;
  if (hProcess == NULL) {
    neuron_platform_set_last_error("wait_process: invalid process handle");
    return 0;
  }

  DWORD wait_result = WaitForSingleObject(hProcess, INFINITE);
  if (wait_result != WAIT_OBJECT_0) {
    neuron_platform_set_last_error("wait_process: WaitForSingleObject failed (error %lu)",
                                   (unsigned long)GetLastError());
    return 0;
  }

  if (out_exit_code != NULL) {
    DWORD exit_code = 0;
    if (GetExitCodeProcess(hProcess, &exit_code)) {
      *out_exit_code = (int32_t)exit_code;
    } else {
      *out_exit_code = -1;
    }
  }

  CloseHandle(hProcess);
  return 1;
}

int neuron_platform_terminate_process_impl(NeuronPlatformProcessHandle process) {
  HANDLE hProcess = (HANDLE)process.native_handle;
  if (hProcess == NULL) {
    neuron_platform_set_last_error("terminate_process: invalid process handle");
    return 0;
  }
  if (!TerminateProcess(hProcess, 1)) {
    neuron_platform_set_last_error("terminate_process: TerminateProcess failed (error %lu)",
                                   (unsigned long)GetLastError());
    return 0;
  }
  CloseHandle(hProcess);
  return 1;
}
#endif



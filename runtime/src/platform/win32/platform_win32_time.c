#include "platform/platform_manager_internal.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

int64_t neuron_platform_now_ms_impl(void) {
  return (int64_t)GetTickCount64();
}

void neuron_platform_sleep_ms_impl(int64_t duration_ms) {
  if (duration_ms <= 0) {
    return;
  }
  Sleep((DWORD)duration_ms);
}
#endif


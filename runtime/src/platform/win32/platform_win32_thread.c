#include "platform/platform_manager_internal.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

void neuron_platform_pin_current_thread_impl(int32_t thread_index) {
  if (thread_index < 0) {
    return;
  }
  const int bits = (int)(sizeof(DWORD_PTR) * 8u);
  if (bits <= 0) {
    return;
  }
  const DWORD_PTR mask = (DWORD_PTR)1u << (unsigned)(thread_index % bits);
  (void)SetThreadAffinityMask(GetCurrentThread(), mask);
}
#endif


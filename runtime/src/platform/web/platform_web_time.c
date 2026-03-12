#include "platform/platform_manager_internal.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

int64_t neuron_platform_now_ms_impl(void) {
  return (int64_t)emscripten_get_now();
}

void neuron_platform_sleep_ms_impl(int64_t duration_ms) {
  (void)duration_ms;
}
#endif
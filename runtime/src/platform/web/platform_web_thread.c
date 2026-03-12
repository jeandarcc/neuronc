#include "platform/platform_manager_internal.h"

#if defined(__EMSCRIPTEN__)
void neuron_platform_pin_current_thread_impl(int32_t thread_index) {
  (void)thread_index;
}
#endif
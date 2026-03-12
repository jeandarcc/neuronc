#include "platform/platform_manager_internal.h"

#if defined(__linux__) || defined(__APPLE__)
#include <time.h>
#include <unistd.h>

int64_t neuron_platform_now_ms_impl(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    neuron_platform_set_last_error("clock_gettime failed");
    return 0;
  }
  return (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000);
}

void neuron_platform_sleep_ms_impl(int64_t duration_ms) {
  if (duration_ms <= 0) {
    return;
  }
  usleep((useconds_t)(duration_ms * 1000));
}
#endif


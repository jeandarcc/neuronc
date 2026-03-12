#include "platform/platform_manager_internal.h"

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

void neuron_platform_pin_current_thread_impl(int32_t thread_index) {
  if (thread_index < 0) {
    return;
  }
  const long cpuCount = sysconf(_SC_NPROCESSORS_ONLN);
  if (cpuCount <= 0) {
    return;
  }
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(thread_index % (int)cpuCount, &set);
  (void)pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
}
#endif


#include "platform/platform_manager_internal.h"

#if defined(_WIN32)
const char *neuron_platform_name_impl(void) { return "windows"; }

const char *neuron_platform_arch_name_impl(void) {
#if defined(_M_X64) || defined(__x86_64__)
  return "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
  return "arm64";
#elif defined(_M_IX86) || defined(__i386__)
  return "x86";
#else
  return "unknown";
#endif
}
#endif


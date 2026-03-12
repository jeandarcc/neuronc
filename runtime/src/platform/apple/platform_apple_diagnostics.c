#include "platform/platform_manager_internal.h"

#if defined(__APPLE__)

const char *neuron_platform_name_impl(void) {
  return "macos";
}

const char *neuron_platform_arch_name_impl(void) {
#if defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
  return "arm64";
#elif defined(__i386__) || defined(_M_IX86)
  return "x86";
#else
  return "unknown";
#endif
}
#endif


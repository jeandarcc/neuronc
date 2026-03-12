#include "platform/platform_manager_internal.h"

#if defined(__EMSCRIPTEN__)
const char *neuron_platform_name_impl(void) { return "web"; }

const char *neuron_platform_arch_name_impl(void) { return "wasm32"; }
#endif
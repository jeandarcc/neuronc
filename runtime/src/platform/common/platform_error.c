#include "platform/platform_manager_internal.h"

#include <stdarg.h>
#include <stdio.h>

static char g_neuron_platform_last_error[512] = {0};

void neuron_platform_set_last_error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_neuron_platform_last_error, sizeof(g_neuron_platform_last_error),
            fmt, args);
  va_end(args);
}

void neuron_platform_clear_last_error(void) {
  g_neuron_platform_last_error[0] = '\0';
}

const char *neuron_platform_last_error(void) {
  return g_neuron_platform_last_error;
}


#include "graphics_core_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

NeuronGraphicsCanvas *g_active_canvas = NULL;
static char g_last_graphics_error[512];

void neuron_graphics_set_error(const char *fmt, ...) {
  if (fmt == NULL) {
    g_last_graphics_error[0] = '\0';
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(g_last_graphics_error, sizeof(g_last_graphics_error), fmt, args);
  va_end(args);
  g_last_graphics_error[sizeof(g_last_graphics_error) - 1] = '\0';
}

const char *npp_graphics_core_last_error(void) { return g_last_graphics_error; }

char *neuron_graphics_copy_string(const char *text) {
  if (text == NULL) {
    return NULL;
  }

  const size_t len = strlen(text);
  char *copy = (char *)malloc(len + 1u);
  if (copy == NULL) {
    return NULL;
  }

  memcpy(copy, text, len + 1u);
  return copy;
}

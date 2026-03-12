#ifndef NPP_RUNTIME_GRAPHICS_CORE_INTERNAL_H
#define NPP_RUNTIME_GRAPHICS_CORE_INTERNAL_H

#include "graphics_types_internal.h"

extern NeuronGraphicsCanvas *g_active_canvas;

void neuron_graphics_set_error(const char *fmt, ...);
char *neuron_graphics_copy_string(const char *text);

#endif

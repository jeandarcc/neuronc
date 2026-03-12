#ifndef NPP_RUNTIME_GRAPHICS_PLATFORM_INTERNAL_H
#define NPP_RUNTIME_GRAPHICS_PLATFORM_INTERNAL_H

#include "graphics/graphics_types_internal.h"

#if defined(_WIN32)
#include <stddef.h>

const wchar_t *neuron_graphics_window_class_name(void);
wchar_t *neuron_graphics_utf8_to_wide(const char *text);
int neuron_graphics_register_window_class(void);
int32_t neuron_graphics_window_pump_messages(NeuronGraphicsWindow *window);
#endif

#endif

#ifndef NPP_RUNTIME_GRAPHICS_WINDOW_CANVAS_INTERNAL_H
#define NPP_RUNTIME_GRAPHICS_WINDOW_CANVAS_INTERNAL_H

#include "neuron_graphics.h"

const char *npp_graphics_core_last_error(void);
NeuronGraphicsWindow *npp_graphics_core_create_window(int32_t width,
                                                       int32_t height,
                                                       const char *title);
void npp_graphics_core_window_free(NeuronGraphicsWindow *window);
NeuronGraphicsCanvas *
npp_graphics_core_create_canvas(NeuronGraphicsWindow *window);
void npp_graphics_core_canvas_free(NeuronGraphicsCanvas *canvas);
int32_t npp_graphics_core_canvas_pump(NeuronGraphicsCanvas *canvas);
int32_t npp_graphics_core_canvas_should_close(NeuronGraphicsCanvas *canvas);
int32_t npp_graphics_core_canvas_take_resize(NeuronGraphicsCanvas *canvas);
void npp_graphics_core_canvas_begin_frame(NeuronGraphicsCanvas *canvas);
void npp_graphics_core_canvas_end_frame(NeuronGraphicsCanvas *canvas);
void npp_graphics_core_present(void);
int32_t npp_graphics_core_window_get_width(NeuronGraphicsWindow *window);
int32_t npp_graphics_core_window_get_height(NeuronGraphicsWindow *window);

#endif

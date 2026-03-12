#include "graphics_window_canvas_internal.h"

const char *neuron_graphics_last_error(void) {
  return npp_graphics_core_last_error();
}

NeuronGraphicsWindow *neuron_graphics_create_window(int32_t width,
                                                    int32_t height,
                                                    const char *title) {
  return npp_graphics_core_create_window(width, height, title);
}

void neuron_graphics_window_free(NeuronGraphicsWindow *window) {
  npp_graphics_core_window_free(window);
}

NeuronGraphicsCanvas *
neuron_graphics_create_canvas(NeuronGraphicsWindow *window) {
  return npp_graphics_core_create_canvas(window);
}

void neuron_graphics_canvas_free(NeuronGraphicsCanvas *canvas) {
  npp_graphics_core_canvas_free(canvas);
}

int32_t neuron_graphics_canvas_pump(NeuronGraphicsCanvas *canvas) {
  return npp_graphics_core_canvas_pump(canvas);
}

int32_t neuron_graphics_canvas_should_close(NeuronGraphicsCanvas *canvas) {
  return npp_graphics_core_canvas_should_close(canvas);
}

int32_t neuron_graphics_canvas_take_resize(NeuronGraphicsCanvas *canvas) {
  return npp_graphics_core_canvas_take_resize(canvas);
}

void neuron_graphics_canvas_begin_frame(NeuronGraphicsCanvas *canvas) {
  npp_graphics_core_canvas_begin_frame(canvas);
}

void neuron_graphics_canvas_end_frame(NeuronGraphicsCanvas *canvas) {
  npp_graphics_core_canvas_end_frame(canvas);
}

void neuron_graphics_present(void) { npp_graphics_core_present(); }

int32_t neuron_graphics_window_get_width(NeuronGraphicsWindow *window) {
  return npp_graphics_core_window_get_width(window);
}

int32_t neuron_graphics_window_get_height(NeuronGraphicsWindow *window) {
  return npp_graphics_core_window_get_height(window);
}

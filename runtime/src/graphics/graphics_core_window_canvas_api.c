#include "graphics/backend/graphics_backend_internal.h"
#include "graphics/graphics_core_internal.h"
#include "graphics/platform/graphics_platform_internal.h"
#include "graphics_window_canvas_internal.h"

#include <stdlib.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

NeuronGraphicsWindow *npp_graphics_core_create_window(int32_t width,
                                                      int32_t height,
                                                      const char *title) {
  neuron_graphics_set_error(NULL);

  if (width <= 0) {
    width = 1280;
  }
  if (height <= 0) {
    height = 720;
  }

  NeuronGraphicsWindow *window =
      (NeuronGraphicsWindow *)calloc(1, sizeof(NeuronGraphicsWindow));
  if (window == NULL) {
    neuron_graphics_set_error("Out of memory creating graphics window");
    return NULL;
  }

  window->width = width;
  window->height = height;
  window->title = neuron_graphics_copy_string(title);
  window->should_close = 0;
  window->resized = 0;
  window->hwnd = NULL;

#if defined(_WIN32)
  if (!neuron_graphics_register_window_class()) {
    npp_graphics_core_window_free(window);
    return NULL;
  }

  wchar_t *wide_title = neuron_graphics_utf8_to_wide(title);
  if (wide_title == NULL) {
    wide_title = neuron_graphics_utf8_to_wide("Neuron Graphics");
  }
  if (wide_title == NULL) {
    neuron_graphics_set_error("Failed to allocate UTF-16 window title");
    npp_graphics_core_window_free(window);
    return NULL;
  }

  RECT rect;
  rect.left = 0;
  rect.top = 0;
  rect.right = width;
  rect.bottom = height;
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

  window->hwnd = (NeuronGraphicsNativeWindowHandle)CreateWindowExW(
      0, neuron_graphics_window_class_name(), wide_title, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
      rect.bottom - rect.top, NULL, NULL, GetModuleHandleW(NULL), window);
  free(wide_title);

  if (window->hwnd == NULL) {
    const DWORD err = GetLastError();
    neuron_graphics_set_error("CreateWindowExW failed (%lu)",
                              (unsigned long)err);
    npp_graphics_core_window_free(window);
    return NULL;
  }

  ShowWindow((HWND)window->hwnd, SW_SHOW);
  UpdateWindow((HWND)window->hwnd);
#else
  window->should_close = 1;
  neuron_graphics_set_error(
      "Graphics.CreateWindow is supported only on Windows in this MVP");
#endif

  return window;
}

void npp_graphics_core_window_free(NeuronGraphicsWindow *window) {
  if (window == NULL) {
    return;
  }
#if defined(_WIN32)
  if (window->hwnd != NULL) {
    DestroyWindow((HWND)window->hwnd);
    window->hwnd = NULL;
  }
#endif
  free(window->title);
  free(window);
}

NeuronGraphicsCanvas *npp_graphics_core_create_canvas(NeuronGraphicsWindow *window) {
  if (window == NULL) {
    neuron_graphics_set_error("Cannot create canvas for null window");
    return NULL;
  }

  NeuronGraphicsCanvas *canvas =
      (NeuronGraphicsCanvas *)calloc(1, sizeof(NeuronGraphicsCanvas));
  if (canvas == NULL) {
    neuron_graphics_set_error("Out of memory creating graphics canvas");
    return NULL;
  }

  canvas->window = window;
  canvas->backend = neuron_graphics_backend_create(window);
  canvas->frame_active = 0;
  canvas->frame_presented = 0;
  canvas->has_clear_color = 0;
  canvas->draw_commands = NULL;
  canvas->draw_command_count = 0;
  canvas->draw_command_capacity = 0;

  if (canvas->backend == NULL) {
    window->should_close = 1;
  }
  return canvas;
}

void npp_graphics_core_canvas_free(NeuronGraphicsCanvas *canvas) {
  if (canvas == NULL) {
    return;
  }
  if (g_active_canvas == canvas) {
    g_active_canvas = NULL;
  }
  if (canvas->backend != NULL) {
    neuron_graphics_backend_destroy(canvas->backend);
    canvas->backend = NULL;
  }
  free(canvas->draw_commands);
  free(canvas);
}

int32_t npp_graphics_core_canvas_pump(NeuronGraphicsCanvas *canvas) {
  if (canvas == NULL || canvas->window == NULL) {
    return 0;
  }
#if defined(_WIN32)
  return neuron_graphics_window_pump_messages(canvas->window);
#else
  return 0;
#endif
}

int32_t npp_graphics_core_canvas_should_close(NeuronGraphicsCanvas *canvas) {
  if (canvas == NULL || canvas->window == NULL) {
    return 1;
  }
  return canvas->window->should_close != 0 ? 1 : 0;
}

int32_t npp_graphics_core_canvas_take_resize(NeuronGraphicsCanvas *canvas) {
  if (canvas == NULL || canvas->window == NULL) {
    return 0;
  }
  if (canvas->window->resized == 0) {
    return 0;
  }
  canvas->window->resized = 0;
  neuron_graphics_backend_mark_resize(canvas->backend);
  return 1;
}

void npp_graphics_core_canvas_begin_frame(NeuronGraphicsCanvas *canvas) {
  if (canvas == NULL || canvas->window == NULL) {
    return;
  }
  g_active_canvas = canvas;
  canvas->frame_active = 0;
  canvas->frame_presented = 0;
  canvas->has_clear_color = 0;
  canvas->clear_color.red = 0.0f;
  canvas->clear_color.green = 0.0f;
  canvas->clear_color.blue = 0.0f;
  canvas->clear_color.alpha = 1.0f;
  canvas->draw_command_count = 0;
  if (!neuron_graphics_backend_begin_frame(canvas->backend)) {
    g_active_canvas = NULL;
    return;
  }
  canvas->frame_active = 1;
}

void npp_graphics_core_canvas_end_frame(NeuronGraphicsCanvas *canvas) {
  if (canvas == NULL) {
    return;
  }
  if (!canvas->frame_presented) {
    npp_graphics_core_present();
  }
  canvas->frame_active = 0;
  if (g_active_canvas == canvas) {
    g_active_canvas = NULL;
  }
}

void npp_graphics_core_present(void) {
  if (g_active_canvas == NULL || !g_active_canvas->frame_active) {
    return;
  }
  if (g_active_canvas->frame_presented) {
    return;
  }
  neuron_graphics_backend_present(g_active_canvas->backend);
  g_active_canvas->frame_presented = 1;
}

int32_t npp_graphics_core_window_get_width(NeuronGraphicsWindow *window) {
  if (window == NULL) {
    return 0;
  }
  return window->width;
}

int32_t npp_graphics_core_window_get_height(NeuronGraphicsWindow *window) {
  if (window == NULL) {
    return 0;
  }
  return window->height;
}

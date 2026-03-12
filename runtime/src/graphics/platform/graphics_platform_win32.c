#include "graphics/graphics_core_internal.h"
#include "graphics/platform/graphics_platform_internal.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static LRESULT CALLBACK neuron_graphics_window_proc(HWND hwnd, UINT msg,
                                                    WPARAM wparam,
                                                    LPARAM lparam) {
  NeuronGraphicsWindow *window =
      (NeuronGraphicsWindow *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

  if (msg == WM_NCCREATE) {
    CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
    window = (NeuronGraphicsWindow *)create->lpCreateParams;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)window);
  }

  switch (msg) {
  case WM_SIZE:
    if (window != NULL) {
      window->width = (int32_t)LOWORD(lparam);
      window->height = (int32_t)HIWORD(lparam);
      window->resized = 1;
    }
    return 0;
  case WM_CLOSE:
    if (window != NULL) {
      window->should_close = 1;
    }
    DestroyWindow(hwnd);
    return 0;
  case WM_DESTROY:
    if (window != NULL) {
      window->should_close = 1;
    }
    return 0;
  default:
    break;
  }

  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

const wchar_t *neuron_graphics_window_class_name(void) {
  return L"NeuronGraphicsCanvasWindow";
}

wchar_t *neuron_graphics_utf8_to_wide(const char *text) {
  if (text == NULL || *text == '\0') {
    text = "Neuron Graphics";
  }

  int required = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
  if (required <= 0) {
    return NULL;
  }

  wchar_t *wide = (wchar_t *)calloc((size_t)required, sizeof(wchar_t));
  if (wide == NULL) {
    return NULL;
  }

  int converted = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, required);
  if (converted <= 0) {
    free(wide);
    return NULL;
  }

  return wide;
}

int neuron_graphics_register_window_class(void) {
  static ATOM g_neuron_graphics_window_class = 0;

  if (g_neuron_graphics_window_class != 0) {
    return 1;
  }

  WNDCLASSEXW cls;
  memset(&cls, 0, sizeof(cls));
  cls.cbSize = sizeof(cls);
  cls.style = CS_HREDRAW | CS_VREDRAW;
  cls.lpfnWndProc = neuron_graphics_window_proc;
  cls.hInstance = GetModuleHandleW(NULL);
  cls.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
  cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  cls.lpszClassName = neuron_graphics_window_class_name();

  g_neuron_graphics_window_class = RegisterClassExW(&cls);
  if (g_neuron_graphics_window_class == 0) {
    const DWORD err = GetLastError();
    if (err == ERROR_CLASS_ALREADY_EXISTS) {
      return 1;
    }

    neuron_graphics_set_error("Failed to register graphics window class (%lu)",
                              (unsigned long)err);
    return 0;
  }

  return 1;
}

int32_t neuron_graphics_window_pump_messages(NeuronGraphicsWindow *window) {
  HWND hwnd = window != NULL ? (HWND)window->hwnd : NULL;
  if (hwnd == NULL) {
    return 0;
  }

  MSG msg;
  int32_t pumped = 0;
  while (PeekMessageW(&msg, hwnd, 0, 0, PM_REMOVE)) {
    pumped = 1;
    if (msg.message == WM_QUIT) {
      window->should_close = 1;
      continue;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  return pumped;
}
#endif

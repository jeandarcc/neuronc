#include "platform/platform_manager_internal.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdlib.h>

typedef struct NeuronPlatformWin32WindowState {
  HWND hwnd;
  int should_close;
  int width;
  int height;
} NeuronPlatformWin32WindowState;

static LRESULT CALLBACK neuron_platform_window_proc(HWND hwnd, UINT msg,
                                                    WPARAM wparam,
                                                    LPARAM lparam) {
  NeuronPlatformWin32WindowState *state =
      (NeuronPlatformWin32WindowState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

  if (msg == WM_NCCREATE) {
    CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
    state = (NeuronPlatformWin32WindowState *)create->lpCreateParams;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);
  }

  switch (msg) {
  case WM_SIZE:
    if (state != NULL) {
      state->width = (int)LOWORD(lparam);
      state->height = (int)HIWORD(lparam);
    }
    return 0;
  case WM_CLOSE:
    if (state != NULL) {
      state->should_close = 1;
    }
    DestroyWindow(hwnd);
    return 0;
  case WM_DESTROY:
    if (state != NULL) {
      state->should_close = 1;
    }
    return 0;
  default:
    break;
  }

  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static int neuron_platform_register_window_class(void) {
  static ATOM g_window_class = 0;
  if (g_window_class != 0) {
    return 1;
  }

  WNDCLASSEXW cls;
  ZeroMemory(&cls, sizeof(cls));
  cls.cbSize = sizeof(cls);
  cls.style = CS_HREDRAW | CS_VREDRAW;
  cls.lpfnWndProc = neuron_platform_window_proc;
  cls.hInstance = GetModuleHandleW(NULL);
  cls.hCursor = NULL;
  cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  cls.lpszClassName = L"NeuronPlatformWindow";

  g_window_class = RegisterClassExW(&cls);
  if (g_window_class == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    neuron_platform_set_last_error("create_window: RegisterClassExW failed (%lu)",
                                   (unsigned long)GetLastError());
    return 0;
  }
  return 1;
}

static wchar_t *neuron_platform_utf8_to_wide(const char *text) {
  if (text == NULL || text[0] == '\0') {
    text = "Neuron Platform Window";
  }
  int required = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
  if (required <= 0) {
    return NULL;
  }
  wchar_t *wide = (wchar_t *)calloc((size_t)required, sizeof(wchar_t));
  if (wide == NULL) {
    return NULL;
  }
  if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, required) <= 0) {
    free(wide);
    return NULL;
  }
  return wide;
}

int neuron_platform_create_window_impl(const char *title, int32_t width,
                                       int32_t height,
                                       NeuronPlatformWindowHandle *out_window) {
  if (out_window == NULL) {
    neuron_platform_set_last_error("create_window: out_window is null");
    return 0;
  }
  out_window->native_handle = NULL;

  if (!neuron_platform_register_window_class()) {
    return 0;
  }

  if (width <= 0) {
    width = 1280;
  }
  if (height <= 0) {
    height = 720;
  }

  NeuronPlatformWin32WindowState *state =
      (NeuronPlatformWin32WindowState *)calloc(1, sizeof(*state));
  if (state == NULL) {
    neuron_platform_set_last_error("create_window: out of memory");
    return 0;
  }
  state->width = width;
  state->height = height;

  wchar_t *wide_title = neuron_platform_utf8_to_wide(title);
  if (wide_title == NULL) {
    free(state);
    neuron_platform_set_last_error("create_window: failed to convert title to UTF-16");
    return 0;
  }

  RECT rect;
  rect.left = 0;
  rect.top = 0;
  rect.right = width;
  rect.bottom = height;
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

  state->hwnd = CreateWindowExW(
      0, L"NeuronPlatformWindow", wide_title, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
      rect.bottom - rect.top, NULL, NULL, GetModuleHandleW(NULL), state);
  free(wide_title);

  if (state->hwnd == NULL) {
    neuron_platform_set_last_error("create_window: CreateWindowExW failed (%lu)",
                                   (unsigned long)GetLastError());
    free(state);
    return 0;
  }

  ShowWindow(state->hwnd, SW_SHOW);
  UpdateWindow(state->hwnd);
  out_window->native_handle = state;
  return 1;
}

void neuron_platform_destroy_window_impl(NeuronPlatformWindowHandle window) {
  NeuronPlatformWin32WindowState *state =
      (NeuronPlatformWin32WindowState *)window.native_handle;
  if (state == NULL) {
    return;
  }
  if (state->hwnd != NULL) {
    DestroyWindow(state->hwnd);
    state->hwnd = NULL;
  }
  free(state);
}

int32_t neuron_platform_pump_events_impl(NeuronPlatformWindowHandle window) {
  NeuronPlatformWin32WindowState *state =
      (NeuronPlatformWin32WindowState *)window.native_handle;
  MSG msg;
  int32_t pumped = 0;
  if (state == NULL || state->hwnd == NULL) {
    return 0;
  }
  while (PeekMessageW(&msg, state->hwnd, 0, 0, PM_REMOVE)) {
    pumped = 1;
    if (msg.message == WM_QUIT) {
      state->should_close = 1;
      continue;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return pumped;
}

int neuron_platform_request_surface_impl(NeuronPlatformWindowHandle window,
                                         void **out_surface_handle) {
  (void)window;
  if (out_surface_handle != NULL) {
    *out_surface_handle = NULL;
  }
  neuron_platform_set_last_error(
      "surface capability is not implemented in this iteration");
  return 0;
}

int neuron_platform_window_should_close_impl(NeuronPlatformWindowHandle window) {
  NeuronPlatformWin32WindowState *state =
      (NeuronPlatformWin32WindowState *)window.native_handle;
  return state != NULL && state->should_close != 0 ? 1 : 0;
}
#endif


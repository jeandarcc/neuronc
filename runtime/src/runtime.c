#include "neuron_runtime.h"
#include "neuron_tensor.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

static size_t g_allocated_bytes = 0;
static int g_runtime_started = 0;
static int g_async_pending = 0;
static char g_last_exception[512] = {0};

#ifdef _WIN32
static int neuron_runtime_stream_is_console(FILE *stream, HANDLE *out_handle) {
  DWORD std_id = STD_OUTPUT_HANDLE;
  if (stream == stderr) {
    std_id = STD_ERROR_HANDLE;
  }

  HANDLE handle = GetStdHandle(std_id);
  DWORD mode = 0;
  if (handle == INVALID_HANDLE_VALUE || handle == NULL ||
      !GetConsoleMode(handle, &mode)) {
    return 0;
  }

  if (out_handle != NULL) {
    *out_handle = handle;
  }
  return 1;
}
#endif

void neuron_runtime_prepare_console(void) {
#ifdef _WIN32
  static int prepared = 0;
  if (prepared) {
    return;
  }
  prepared = 1;

  if (neuron_runtime_stream_is_console(stdout, NULL)) {
    (void)SetConsoleOutputCP(CP_UTF8);
  }

  HANDLE in_handle = GetStdHandle(STD_INPUT_HANDLE);
  DWORD input_mode = 0;
  if (in_handle != INVALID_HANDLE_VALUE && in_handle != NULL &&
      GetConsoleMode(in_handle, &input_mode)) {
    (void)SetConsoleCP(CP_UTF8);
  }
#endif
}

static void neuron_runtime_write_text(FILE *stream, const char *text,
                                      int append_newline) {
  const char *safe = text == NULL ? "" : text;

#ifdef _WIN32
  neuron_runtime_prepare_console();
  HANDLE handle = INVALID_HANDLE_VALUE;
  if (neuron_runtime_stream_is_console(stream, &handle)) {
    int utf8_len = (int)strlen(safe);
    int wide_len = 0;
    if (utf8_len > 0) {
      wide_len =
          MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, safe, utf8_len,
                              NULL, 0);
      if (wide_len <= 0) {
        wide_len = MultiByteToWideChar(CP_UTF8, 0, safe, utf8_len, NULL, 0);
      }
    }

    if (utf8_len == 0 || wide_len > 0) {
      int total_wide_len = wide_len + (append_newline ? 2 : 0);
      WCHAR *wide_text = NULL;
      DWORD written = 0;

      if (total_wide_len > 0) {
        wide_text = (WCHAR *)malloc((size_t)total_wide_len * sizeof(WCHAR));
      }
      if (total_wide_len == 0 || wide_text != NULL) {
        int converted = 0;
        if (wide_len > 0) {
          converted = MultiByteToWideChar(CP_UTF8, 0, safe, utf8_len, wide_text,
                                          wide_len);
          if (converted <= 0) {
            free(wide_text);
            wide_text = NULL;
          }
        }
        if (wide_text != NULL || total_wide_len == 0) {
          int write_len = converted;
          if (append_newline) {
            if (wide_text != NULL) {
              wide_text[write_len++] = L'\r';
              wide_text[write_len++] = L'\n';
            } else {
              static const WCHAR newline_only[] = L"\r\n";
              (void)WriteConsoleW(handle, newline_only, 2, &written, NULL);
              return;
            }
          }
          if (write_len > 0) {
            (void)WriteConsoleW(handle, wide_text, (DWORD)write_len, &written,
                                NULL);
          }
          free(wide_text);
          return;
        }
      }
      free(wide_text);
    }
  }
#endif

  fputs(safe, stream);
  if (append_newline) {
    fputc('\n', stream);
  }
  fflush(stream);
}

void neuron_runtime_startup(void) {
  if (g_runtime_started) {
    return;
  }
  neuron_runtime_prepare_console();
  g_runtime_started = 1;
  srand((unsigned int)time(NULL));
}

void neuron_runtime_shutdown(void) {
  if (!g_runtime_started) {
    return;
  }
  neuron_tensor_release_workspace_cache();
  g_runtime_started = 0;
  g_async_pending = 0;
}

void neuron_module_init(const char *module_name) {
  if (module_name == NULL || *module_name == '\0') {
    return;
  }
  // Module init registry will be added when package manager lands.
  (void)module_name;
}

void *neuron_alloc(size_t size) {
  if (size == 0) {
    return NULL;
  }
  void *ptr = malloc(size);
  if (ptr != NULL) {
    g_allocated_bytes += size;
  }
  return ptr;
}

void neuron_dealloc(void *ptr) {
  free(ptr);
}

size_t neuron_allocated_bytes(void) { return g_allocated_bytes; }

void neuron_thread_submit(neuron_task_fn task, void *ctx) {
  // Placeholder scheduler: run immediately on current thread.
  if (task != NULL) {
    task(ctx);
  }
}

void neuron_async_submit(neuron_task_fn task, void *ctx) {
  if (task == NULL) {
    return;
  }
  g_async_pending++;
  task(ctx);
  g_async_pending--;
}

void neuron_async_wait_all(void) {
  // Immediate execution model, so there is nothing to wait for.
  while (g_async_pending > 0) {
  }
}

void neuron_throw(const char *message) {
  if (message == NULL) {
    g_last_exception[0] = '\0';
    return;
  }
  strncpy(g_last_exception, message, sizeof(g_last_exception) - 1);
  g_last_exception[sizeof(g_last_exception) - 1] = '\0';
}

const char *neuron_last_exception(void) {
  return g_last_exception[0] == '\0' ? NULL : g_last_exception;
}

void neuron_clear_exception(void) { g_last_exception[0] = '\0'; }

int64_t neuron_has_exception(void) { return g_last_exception[0] == '\0' ? 0 : 1; }

void neuron_system_print_int(int64_t value) {
  char buffer[64] = {0};
  (void)snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
  neuron_runtime_write_text(stdout, buffer, 1);
}

void neuron_system_print_str(const char *value) {
  neuron_runtime_write_text(stdout, value, 1);
}

void neuron_repl_echo_string(const char *value) {
  const char *safe = value == NULL ? "" : value;
  size_t escaped_len = 2;
  for (const unsigned char *cursor = (const unsigned char *)safe; *cursor != '\0';
       ++cursor) {
    switch (*cursor) {
    case '\\':
    case '\'':
    case '\n':
    case '\r':
    case '\t':
      escaped_len += 2;
      break;
    default:
      escaped_len += 1;
      break;
    }
  }

  char *escaped = (char *)malloc(escaped_len + 1);
  if (escaped == NULL) {
    neuron_runtime_write_text(stdout, safe, 1);
    return;
  }

  size_t write_index = 0;
  escaped[write_index++] = '\'';
  for (const unsigned char *cursor = (const unsigned char *)safe; *cursor != '\0';
       ++cursor) {
    switch (*cursor) {
    case '\\':
      escaped[write_index++] = '\\';
      escaped[write_index++] = '\\';
      break;
    case '\'':
      escaped[write_index++] = '\\';
      escaped[write_index++] = '\'';
      break;
    case '\n':
      escaped[write_index++] = '\\';
      escaped[write_index++] = 'n';
      break;
    case '\r':
      escaped[write_index++] = '\\';
      escaped[write_index++] = 'r';
      break;
    case '\t':
      escaped[write_index++] = '\\';
      escaped[write_index++] = 't';
      break;
    default:
      escaped[write_index++] = (char)*cursor;
      break;
    }
  }
  escaped[write_index++] = '\'';
  escaped[write_index] = '\0';

  neuron_runtime_write_text(stdout, escaped, 1);
  free(escaped);
}

void neuron_system_exit(int32_t code) { exit(code); }

double neuron_math_sqrt(double value) { return sqrt(value); }

double neuron_math_pow(double base, double exponent) {
  return pow(base, exponent);
}

double neuron_math_abs(double value) { return fabs(value); }

void neuron_io_write_line(const char *text) { neuron_system_print_str(text); }

int64_t neuron_io_read_int(void) {
  return neuron_io_input_int(NULL, 0, 0, 0, 0, 0, 0, -1);
}

int64_t neuron_time_now_ms(void) {
#ifdef _WIN32
  static LARGE_INTEGER freq = {0};
  static int freqReady = 0;
  LARGE_INTEGER counter;
  if (!freqReady) {
    if (QueryPerformanceFrequency(&freq) && freq.QuadPart > 0) {
      freqReady = 1;
    } else {
      return (int64_t)GetTickCount64();
    }
  }
  if (!QueryPerformanceCounter(&counter)) {
    return (int64_t)GetTickCount64();
  }
  return (int64_t)((counter.QuadPart * 1000LL) / freq.QuadPart);
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
#endif
}

int64_t neuron_random_int(int64_t min, int64_t max) {
  if (max <= min) {
    return min;
  }
  int64_t range = max - min + 1;
  return min + ((int64_t)rand() % range);
}

double neuron_random_float(void) {
  return (double)rand() / (double)RAND_MAX;
}

void neuron_log_info(const char *msg) {
  char buffer[4096] = {0};
  (void)snprintf(buffer, sizeof(buffer), "[INFO] %s", msg == NULL ? "" : msg);
  neuron_runtime_write_text(stdout, buffer, 1);
}

void neuron_log_warning(const char *msg) {
  char buffer[4096] = {0};
  (void)snprintf(buffer, sizeof(buffer), "[WARN] %s", msg == NULL ? "" : msg);
  neuron_runtime_write_text(stdout, buffer, 1);
}

void neuron_log_error(const char *msg) {
  char buffer[4096] = {0};
  (void)snprintf(buffer, sizeof(buffer), "[ERROR] %s", msg == NULL ? "" : msg);
  neuron_runtime_write_text(stderr, buffer, 1);
}

NeuronArray *neuron_array_create(int64_t length, int64_t element_size) {
  if (length < 0 || element_size <= 0) {
    return NULL;
  }

  NeuronArray *array = (NeuronArray *)malloc(sizeof(NeuronArray));
  if (array == NULL) {
    return NULL;
  }

  array->length = length;
  array->element_size = element_size;

  if (length == 0) {
    array->data = NULL;
    return array;
  }

  size_t bytes = (size_t)length * (size_t)element_size;
  array->data = calloc(1, bytes);
  if (array->data == NULL) {
    free(array);
    return NULL;
  }

  return array;
}

void neuron_array_free(NeuronArray *array) {
  if (array == NULL) {
    return;
  }
  free(array->data);
  free(array);
}

void *neuron_array_data(NeuronArray *array) {
  if (array == NULL) {
    return NULL;
  }
  return array->data;
}

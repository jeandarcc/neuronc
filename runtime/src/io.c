#include "neuron_runtime.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#endif

typedef struct {
#ifdef _WIN32
  HANDLE input_handle;
  DWORD previous_mode;
  char pending_utf8[16];
  int pending_utf8_count;
  int pending_utf8_index;
#else
  struct termios previous_termios;
#endif
  int active;
} NeuronInputRawState;

static char *g_last_input_string = NULL;
enum {
  NEURON_INPUT_KEY_UP = 1001,
  NEURON_INPUT_KEY_DOWN = 1002,
  NEURON_INPUT_KEY_LEFT = 1003,
  NEURON_INPUT_KEY_RIGHT = 1004,
};

static int64_t neuron_input_now_ms(void);

typedef struct {
  char **items;
  int64_t count;
  char *storage;
} NeuronInputEnumOptions;

static int neuron_input_is_text_byte(int key) {
  unsigned char byte = (unsigned char)key;
  return byte >= 0x20 && byte != 0x7F;
}

#ifdef _WIN32
static int neuron_input_take_pending_utf8_byte(NeuronInputRawState *state) {
  if (state == NULL || state->pending_utf8_index >= state->pending_utf8_count) {
    return -1;
  }

  const int key =
      (unsigned char)state->pending_utf8[state->pending_utf8_index++];
  if (state->pending_utf8_index >= state->pending_utf8_count) {
    state->pending_utf8_index = 0;
    state->pending_utf8_count = 0;
  }
  return key;
}

static int neuron_input_key_text_from_event(const KEY_EVENT_RECORD *key,
                                            wchar_t *out_text,
                                            int out_capacity) {
  if (key == NULL || out_text == NULL || out_capacity <= 0) {
    return 0;
  }

  if (key->uChar.UnicodeChar != 0) {
    out_text[0] = key->uChar.UnicodeChar;
    return 1;
  }

  BYTE keyboard_state[256] = {0};
  if (!GetKeyboardState(keyboard_state)) {
    return 0;
  }

  const HKL layout = GetKeyboardLayout(0);
  const int count =
      ToUnicodeEx(key->wVirtualKeyCode, key->wVirtualScanCode, keyboard_state,
                  out_text, out_capacity, 0, layout);
  if (count <= 0) {
    return 0;
  }
  return count;
}

static int neuron_input_queue_utf8_bytes(NeuronInputRawState *state,
                                         const wchar_t *text, int text_len) {
  if (state == NULL || text == NULL || text_len <= 0) {
    return 0;
  }

  const int utf8_len = WideCharToMultiByte(
      CP_UTF8, 0, text, text_len, state->pending_utf8,
      (int)sizeof(state->pending_utf8), NULL, NULL);
  if (utf8_len <= 0) {
    state->pending_utf8_count = 0;
    state->pending_utf8_index = 0;
    return 0;
  }

  state->pending_utf8_count = utf8_len;
  state->pending_utf8_index = 0;
  return 1;
}

static int neuron_input_raw_read_console_key(NeuronInputRawState *state,
                                             int64_t timeout_ms,
                                             int allow_arrows) {
  const int pending_key = neuron_input_take_pending_utf8_byte(state);
  if (pending_key >= 0) {
    return pending_key;
  }

  const int64_t start_ms = neuron_input_now_ms();
  for (;;) {
    DWORD wait_ms = INFINITE;
    if (timeout_ms >= 0) {
      const int64_t elapsed = neuron_input_now_ms() - start_ms;
      if (elapsed >= timeout_ms) {
        return -1;
      }
      const int64_t remaining = timeout_ms - elapsed;
      wait_ms = remaining > (int64_t)UINT32_MAX ? UINT32_MAX : (DWORD)remaining;
    }

    const DWORD wait_result = WaitForSingleObject(state->input_handle, wait_ms);
    if (wait_result == WAIT_TIMEOUT) {
      return -1;
    }
    if (wait_result != WAIT_OBJECT_0) {
      return -1;
    }

    INPUT_RECORD record;
    DWORD read_count = 0;
    memset(&record, 0, sizeof(record));
    if (!ReadConsoleInputW(state->input_handle, &record, 1, &read_count) ||
        read_count == 0) {
      return -1;
    }
    if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) {
      continue;
    }

    const KEY_EVENT_RECORD *key = &record.Event.KeyEvent;
    const int control_down =
        (key->dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
    const int alt_down =
        (key->dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
    const int plain_ctrl_down = control_down && !alt_down;

    if (key->wVirtualKeyCode == VK_RETURN) {
      return '\r';
    }
    if (key->wVirtualKeyCode == VK_BACK) {
      return 8;
    }
    if (key->wVirtualKeyCode == VK_ESCAPE) {
      return 27;
    }
    if (allow_arrows) {
      if (key->wVirtualKeyCode == VK_UP) {
        return NEURON_INPUT_KEY_UP;
      }
      if (key->wVirtualKeyCode == VK_DOWN) {
        return NEURON_INPUT_KEY_DOWN;
      }
      if (key->wVirtualKeyCode == VK_LEFT) {
        return NEURON_INPUT_KEY_LEFT;
      }
      if (key->wVirtualKeyCode == VK_RIGHT) {
        return NEURON_INPUT_KEY_RIGHT;
      }
    }
    if (plain_ctrl_down) {
      continue;
    }

    wchar_t translated[8] = {0};
    const int translated_count =
        neuron_input_key_text_from_event(key, translated,
                                         (int)(sizeof(translated) /
                                               sizeof(translated[0])));
    if (translated_count <= 0) {
      continue;
    }
    if (!neuron_input_queue_utf8_bytes(state, translated, translated_count)) {
      continue;
    }

    return neuron_input_take_pending_utf8_byte(state);
  }
}
#endif

static size_t neuron_input_previous_utf8_start(const char *buffer,
                                               size_t length) {
  if (buffer == NULL || length == 0) {
    return 0;
  }
  size_t pos = length - 1;
  while (pos > 0 && (((unsigned char)buffer[pos] & 0xC0u) == 0x80u)) {
    pos--;
  }
  return pos;
}

static int64_t neuron_input_now_ms(void) {
#ifdef _WIN32
  return (int64_t)GetTickCount64();
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
#endif
}

static void neuron_input_sleep_ms(int64_t ms) {
  if (ms <= 0) {
    return;
  }
#ifdef _WIN32
  Sleep((DWORD)ms);
#else
  usleep((unsigned int)(ms * 1000));
#endif
}

static int neuron_input_is_tty(void) {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
}

static int64_t neuron_input_remaining_ms(int64_t start_ms, int64_t timeout_ms) {
  if (timeout_ms < 0) {
    return -1;
  }
  const int64_t elapsed = neuron_input_now_ms() - start_ms;
  if (elapsed >= timeout_ms) {
    return 0;
  }
  return timeout_ms - elapsed;
}

static void neuron_input_enable_ansi_console(void) {
#ifdef _WIN32
  static int initialized = 0;
  if (initialized) {
    return;
  }
  initialized = 1;

  HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (stdout_handle == INVALID_HANDLE_VALUE) {
    return;
  }
  if (!GetConsoleMode(stdout_handle, &mode)) {
    return;
  }
  mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  (void)SetConsoleMode(stdout_handle, mode);
#endif
}

static void neuron_input_show_invalid_char(int key) {
  if (key <= 0 || !neuron_input_is_text_byte(key)) {
    return;
  }
  if ((unsigned char)key >= 0x80) {
    // Avoid flashing partial UTF-8 bytes as garbled glyphs.
    return;
  }
  neuron_input_enable_ansi_console();
  fputs("\x1b[31m", stdout);
  fputc((char)key, stdout);
  fputs("\x1b[0m", stdout);
  fflush(stdout);
  neuron_input_sleep_ms(120);
  fputs("\b \b", stdout);
  fflush(stdout);
}

static int neuron_input_raw_begin(NeuronInputRawState *state) {
  if (state == NULL) {
    return 0;
  }
  memset(state, 0, sizeof(*state));
#ifdef _WIN32
  state->input_handle = GetStdHandle(STD_INPUT_HANDLE);
  if (state->input_handle == INVALID_HANDLE_VALUE ||
      !GetConsoleMode(state->input_handle, &state->previous_mode)) {
    return 0;
  }
  DWORD mode = state->previous_mode;
  mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
  if (!SetConsoleMode(state->input_handle, mode)) {
    return 0;
  }
  state->active = 1;
  return 1;
#else
  if (tcgetattr(STDIN_FILENO, &state->previous_termios) != 0) {
    return 0;
  }
  struct termios raw = state->previous_termios;
  raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
    return 0;
  }
  state->active = 1;
  return 1;
#endif
}

static void neuron_input_raw_end(NeuronInputRawState *state) {
  if (state == NULL || !state->active) {
    return;
  }
#ifdef _WIN32
  (void)SetConsoleMode(state->input_handle, state->previous_mode);
#else
  (void)tcsetattr(STDIN_FILENO, TCSANOW, &state->previous_termios);
#endif
  state->active = 0;
}

static int neuron_input_raw_read_key(NeuronInputRawState *state,
                                     int64_t timeout_ms) {
#ifdef _WIN32
  return neuron_input_raw_read_console_key(state, timeout_ms, 0);
#else
  (void)state;
  fd_set input_set;
  FD_ZERO(&input_set);
  FD_SET(STDIN_FILENO, &input_set);

  struct timeval tv;
  struct timeval *tv_ptr = NULL;
  if (timeout_ms >= 0) {
    tv.tv_sec = (time_t)(timeout_ms / 1000);
    tv.tv_usec = (suseconds_t)((timeout_ms % 1000) * 1000);
    tv_ptr = &tv;
  }

  const int ready = select(STDIN_FILENO + 1, &input_set, NULL, NULL, tv_ptr);
  if (ready <= 0) {
    return -1;
  }

  unsigned char ch = 0;
  const ssize_t read_count = read(STDIN_FILENO, &ch, 1);
  if (read_count == 1) {
    return (int)ch;
  }
  return -1;
#endif
}

static int neuron_input_raw_read_key_with_arrows(NeuronInputRawState *state,
                                                 int64_t timeout_ms) {
#ifdef _WIN32
  return neuron_input_raw_read_console_key(state, timeout_ms, 1);
#else
  int key = neuron_input_raw_read_key(state, timeout_ms);
  if (key != 27) {
    return key;
  }

  int bracket = neuron_input_raw_read_key(state, 20);
  if (bracket != '[') {
    return key;
  }

  int direction = neuron_input_raw_read_key(state, 20);
  if (direction == 'A') {
    return NEURON_INPUT_KEY_UP;
  }
  if (direction == 'B') {
    return NEURON_INPUT_KEY_DOWN;
  }
  if (direction == 'D') {
    return NEURON_INPUT_KEY_LEFT;
  }
  if (direction == 'C') {
    return NEURON_INPUT_KEY_RIGHT;
  }
  return key;
#endif
}

static int neuron_input_ascii_equals_ignore_case(const char *left,
                                                 const char *right) {
  if (left == NULL || right == NULL) {
    return 0;
  }
  while (*left != '\0' && *right != '\0') {
    const char left_lower = (char)tolower((unsigned char)*left);
    const char right_lower = (char)tolower((unsigned char)*right);
    if (left_lower != right_lower) {
      return 0;
    }
    left++;
    right++;
  }
  return *left == '\0' && *right == '\0';
}

static void neuron_input_free_enum_options(NeuronInputEnumOptions *options) {
  if (options == NULL) {
    return;
  }
  free(options->items);
  free(options->storage);
  options->items = NULL;
  options->storage = NULL;
  options->count = 0;
}

static int neuron_input_parse_enum_options(const char *payload,
                                           int64_t expected_count,
                                           NeuronInputEnumOptions *out_options) {
  if (out_options == NULL) {
    return 0;
  }
  memset(out_options, 0, sizeof(*out_options));
  if (payload == NULL || *payload == '\0') {
    return 0;
  }

  int64_t parsed_count = 1;
  for (const char *cursor = payload; *cursor != '\0'; ++cursor) {
    if (*cursor == '\n' && cursor[1] != '\0') {
      parsed_count++;
    }
  }
  if (parsed_count <= 0) {
    return 0;
  }

  int64_t final_count = parsed_count;
  if (expected_count > 0 && expected_count < final_count) {
    final_count = expected_count;
  }
  if (final_count <= 0) {
    return 0;
  }

  const size_t payload_len = strlen(payload);
  char *storage = (char *)malloc(payload_len + 1);
  if (storage == NULL) {
    return 0;
  }
  memcpy(storage, payload, payload_len + 1);

  char **items = (char **)calloc((size_t)final_count, sizeof(char *));
  if (items == NULL) {
    free(storage);
    return 0;
  }

  int64_t index = 0;
  char *cursor = storage;
  while (cursor != NULL && *cursor != '\0' && index < final_count) {
    items[index++] = cursor;
    char *newline = strchr(cursor, '\n');
    if (newline == NULL) {
      break;
    }
    *newline = '\0';
    cursor = newline + 1;
  }

  if (index <= 0) {
    free(items);
    free(storage);
    return 0;
  }

  out_options->items = items;
  out_options->storage = storage;
  out_options->count = index;
  return 1;
}

static int64_t neuron_input_parse_enum_selection(
    const char *line, const NeuronInputEnumOptions *options) {
  if (line == NULL || options == NULL || options->count <= 0) {
    return -1;
  }

  const char *start = line;
  while (*start != '\0' && isspace((unsigned char)*start)) {
    start++;
  }
  const char *end = start + strlen(start);
  while (end > start && isspace((unsigned char)*(end - 1))) {
    end--;
  }

  if (start == end) {
    return -1;
  }

  const size_t len = (size_t)(end - start);
  char normalized[256] = {0};
  if (len >= sizeof(normalized)) {
    return -1;
  }
  memcpy(normalized, start, len);
  normalized[len] = '\0';

  char *number_end = NULL;
  long long as_number = strtoll(normalized, &number_end, 10);
  if (number_end != normalized && number_end != NULL && *number_end == '\0') {
    if (as_number >= 0 && as_number < options->count) {
      return (int64_t)as_number;
    }
    if (as_number >= 1 && as_number <= options->count) {
      return (int64_t)(as_number - 1);
    }
  }

  for (int64_t i = 0; i < options->count; ++i) {
    const char *option = options->items[i];
    if (option == NULL) {
      continue;
    }
    if (strcmp(normalized, option) == 0 ||
        neuron_input_ascii_equals_ignore_case(normalized, option)) {
      return i;
    }
  }
  return -1;
}

static void neuron_input_render_enum_menu(const NeuronInputEnumOptions *options,
                                          int64_t selected_index,
                                          int first_render) {
  if (options == NULL || options->count <= 0) {
    return;
  }

  neuron_input_enable_ansi_console();
  if (!first_render) {
    for (int64_t i = 0; i < options->count; ++i) {
      fputs("\x1b[1A", stdout);
    }
  }

  for (int64_t i = 0; i < options->count; ++i) {
    fputs("\r\x1b[2K", stdout);
    if (i == selected_index) {
      fputs("> ", stdout);
    } else {
      fputs("  ", stdout);
    }
    fputs(options->items[i] == NULL ? "" : options->items[i], stdout);
    fputc('\n', stdout);
  }
  fflush(stdout);
}

static int neuron_input_append_char(char **buffer, size_t *length, size_t *cap,
                                    char ch) {
  if (buffer == NULL || length == NULL || cap == NULL) {
    return 0;
  }
  if (*length + 2 > *cap) {
    size_t next_cap = (*cap == 0) ? 32 : (*cap * 2);
    char *resized = (char *)realloc(*buffer, next_cap);
    if (resized == NULL) {
      return 0;
    }
    *buffer = resized;
    *cap = next_cap;
  }
  (*buffer)[*length] = ch;
  (*length)++;
  (*buffer)[*length] = '\0';
  return 1;
}

typedef int (*NeuronInputValidatorFn)(char ch, const char *buffer, size_t len);

static int neuron_input_is_valid_int_char(char ch, const char *buffer,
                                          size_t len) {
  (void)buffer;
  if (ch >= '0' && ch <= '9') {
    return 1;
  }
  if (ch == '-' && len == 0) {
    return 1;
  }
  return 0;
}

static int neuron_input_is_valid_float_char(char ch, const char *buffer,
                                            size_t len) {
  if (ch >= '0' && ch <= '9') {
    return 1;
  }
  if (ch == '-' && len == 0) {
    return 1;
  }
  if (ch == '.') {
    return buffer == NULL || strchr(buffer, '.') == NULL;
  }
  return 0;
}

static int neuron_input_is_valid_bool_char(char ch, const char *buffer,
                                           size_t len) {
  (void)buffer;
  (void)len;
  return isalpha((unsigned char)ch) || isdigit((unsigned char)ch);
}

static int neuron_input_is_valid_string_char(char ch, const char *buffer,
                                             size_t len) {
  (void)buffer;
  (void)len;
  if (ch == '\r' || ch == '\n') {
    return 0;
  }
  unsigned char byte = (unsigned char)ch;
  return byte >= 0x20 && byte != 0x7F;
}

static int neuron_input_prefers_buffered_mode(int secret_mode,
                                              NeuronInputValidatorFn validator) {
  return secret_mode == 0 && validator == neuron_input_is_valid_string_char;
}

static void neuron_input_print_prompt(const char *prompt) {
  neuron_runtime_prepare_console();
  if (prompt != NULL && *prompt != '\0') {
    fputs(prompt, stdout);
  }
  fflush(stdout);
}

static int neuron_input_read_filtered_line(const char *prompt, int secret_mode,
                                           int64_t timeout_ms,
                                           NeuronInputValidatorFn validator,
                                           char **out_line,
                                           int *out_timed_out) {
  NeuronInputRawState raw_state;
  char *line = NULL;
  size_t length = 0;
  size_t cap = 0;
  const int interactive = neuron_input_is_tty();
  const int prefer_buffered_mode =
      neuron_input_prefers_buffered_mode(secret_mode, validator);
  const int64_t start_ms = neuron_input_now_ms();

  if (out_line == NULL || out_timed_out == NULL) {
    return 0;
  }
  *out_line = NULL;
  *out_timed_out = 0;

  if (!neuron_input_append_char(&line, &length, &cap, '\0')) {
    free(line);
    return 0;
  }
  length = 0;
  line[0] = '\0';

  neuron_input_print_prompt(prompt);

  if (prefer_buffered_mode || !interactive || !neuron_input_raw_begin(&raw_state)) {
    char fallback[2048] = {0};
    if (fgets(fallback, (int)sizeof(fallback), stdin) == NULL) {
      free(line);
      return 0;
    }
    size_t fallback_len = strlen(fallback);
    while (fallback_len > 0 &&
           (fallback[fallback_len - 1] == '\n' ||
            fallback[fallback_len - 1] == '\r')) {
      fallback[--fallback_len] = '\0';
    }
    free(line);
    line = (char *)malloc(fallback_len + 1);
    if (line == NULL) {
      return 0;
    }
    memcpy(line, fallback, fallback_len + 1);
    *out_line = line;
    return 1;
  }

  for (;;) {
    const int64_t remaining = neuron_input_remaining_ms(start_ms, timeout_ms);
    if (timeout_ms >= 0 && remaining <= 0) {
      *out_timed_out = 1;
      break;
    }

    const int key = neuron_input_raw_read_key(&raw_state, remaining);
    if (key < 0) {
      *out_timed_out = 1;
      break;
    }

    if (key == '\r' || key == '\n') {
      fputc('\n', stdout);
      fflush(stdout);
      break;
    }

    if (key == 8 || key == 127) {
      if (length > 0) {
        length = neuron_input_previous_utf8_start(line, length);
        line[length] = '\0';
        fputs("\b \b", stdout);
        fflush(stdout);
      }
      continue;
    }

    if (!neuron_input_is_text_byte(key)) {
      continue;
    }

    if (validator != NULL && !validator((char)key, line, length)) {
      neuron_input_show_invalid_char(key);
      continue;
    }

    if (!neuron_input_append_char(&line, &length, &cap, (char)key)) {
      neuron_input_raw_end(&raw_state);
      free(line);
      return 0;
    }

    if (secret_mode) {
      if (((unsigned char)key & 0xC0u) != 0x80u) {
        fputc('*', stdout);
      }
    } else {
      fputc((char)key, stdout);
    }
    fflush(stdout);
  }

  neuron_input_raw_end(&raw_state);
  *out_line = line;
  return 1;
}

static int neuron_input_parse_int64(const char *text, int64_t *out_value) {
  if (text == NULL || out_value == NULL || *text == '\0') {
    return 0;
  }
  char *end = NULL;
  const long long parsed = strtoll(text, &end, 10);
  if (end == text || (end != NULL && *end != '\0')) {
    return 0;
  }
  *out_value = (int64_t)parsed;
  return 1;
}

static int neuron_input_parse_double(const char *text, double *out_value) {
  if (text == NULL || out_value == NULL || *text == '\0') {
    return 0;
  }
  char *end = NULL;
  const double parsed = strtod(text, &end);
  if (end == text || (end != NULL && *end != '\0')) {
    return 0;
  }
  *out_value = parsed;
  return 1;
}

static int neuron_input_parse_bool(const char *text, int64_t *out_value) {
  if (text == NULL || out_value == NULL || *text == '\0') {
    return 0;
  }
  char lowered[32] = {0};
  size_t len = strlen(text);
  if (len >= sizeof(lowered)) {
    return 0;
  }
  for (size_t i = 0; i < len; ++i) {
    lowered[i] = (char)tolower((unsigned char)text[i]);
  }
  lowered[len] = '\0';

  if (strcmp(lowered, "1") == 0 || strcmp(lowered, "true") == 0 ||
      strcmp(lowered, "t") == 0 || strcmp(lowered, "yes") == 0 ||
      strcmp(lowered, "y") == 0 || strcmp(lowered, "on") == 0) {
    *out_value = 1;
    return 1;
  }
  if (strcmp(lowered, "0") == 0 || strcmp(lowered, "false") == 0 ||
      strcmp(lowered, "f") == 0 || strcmp(lowered, "no") == 0 ||
      strcmp(lowered, "n") == 0 || strcmp(lowered, "off") == 0) {
    *out_value = 0;
    return 1;
  }
  return 0;
}

static const char *neuron_input_store_string(const char *value) {
  const char *safe = value == NULL ? "" : value;
  const size_t len = strlen(safe);
  char *resized = (char *)realloc(g_last_input_string, len + 1);
  if (resized == NULL) {
    return g_last_input_string == NULL ? "" : g_last_input_string;
  }
  g_last_input_string = resized;
  memcpy(g_last_input_string, safe, len);
  g_last_input_string[len] = '\0';
  return g_last_input_string;
}

void neuron_print_int(int64_t val) { neuron_system_print_int(val); }

void neuron_print_str(const char *str) { neuron_system_print_str(str); }

int64_t neuron_io_input_int(const char *prompt, int64_t has_min,
                            int64_t min_value, int64_t has_max,
                            int64_t max_value, int64_t has_default,
                            int64_t default_value, int64_t timeout_ms) {
  const int interactive = neuron_input_is_tty();
  int attempts = 0;
  for (;;) {
    char *line = NULL;
    int timed_out = 0;
    int64_t value = 0;
    attempts++;

    if (!neuron_input_read_filtered_line(
            prompt, 0, timeout_ms, neuron_input_is_valid_int_char, &line,
            &timed_out)) {
      free(line);
      return has_default ? default_value : 0;
    }

    if (timed_out) {
      free(line);
      return has_default ? default_value : 0;
    }

    if (line != NULL && line[0] == '\0' && has_default) {
      free(line);
      return default_value;
    }

    if (!neuron_input_parse_int64(line, &value)) {
      free(line);
      if (!interactive || attempts >= 2) {
        return has_default ? default_value : 0;
      }
      continue;
    }
    free(line);

    if (has_min && value < min_value) {
      if (!interactive || attempts >= 2) {
        return has_default ? default_value : min_value;
      }
      continue;
    }
    if (has_max && value > max_value) {
      if (!interactive || attempts >= 2) {
        return has_default ? default_value : max_value;
      }
      continue;
    }
    return value;
  }
}

float neuron_io_input_float(const char *prompt, int64_t has_min,
                            float min_value, int64_t has_max, float max_value,
                            int64_t has_default, float default_value,
                            int64_t timeout_ms) {
  const int interactive = neuron_input_is_tty();
  int attempts = 0;
  for (;;) {
    char *line = NULL;
    int timed_out = 0;
    double value = 0.0;
    attempts++;

    if (!neuron_input_read_filtered_line(
            prompt, 0, timeout_ms, neuron_input_is_valid_float_char, &line,
            &timed_out)) {
      free(line);
      return has_default ? default_value : 0.0f;
    }

    if (timed_out) {
      free(line);
      return has_default ? default_value : 0.0f;
    }

    if (line != NULL && line[0] == '\0' && has_default) {
      free(line);
      return default_value;
    }

    if (!neuron_input_parse_double(line, &value)) {
      free(line);
      if (!interactive || attempts >= 2) {
        return has_default ? default_value : 0.0f;
      }
      continue;
    }
    free(line);

    if (has_min && value < (double)min_value) {
      if (!interactive || attempts >= 2) {
        return has_default ? default_value : min_value;
      }
      continue;
    }
    if (has_max && value > (double)max_value) {
      if (!interactive || attempts >= 2) {
        return has_default ? default_value : max_value;
      }
      continue;
    }
    return (float)value;
  }
}

double neuron_io_input_double(const char *prompt, int64_t has_min,
                              double min_value, int64_t has_max,
                              double max_value, int64_t has_default,
                              double default_value, int64_t timeout_ms) {
  const int interactive = neuron_input_is_tty();
  int attempts = 0;
  for (;;) {
    char *line = NULL;
    int timed_out = 0;
    double value = 0.0;
    attempts++;

    if (!neuron_input_read_filtered_line(
            prompt, 0, timeout_ms, neuron_input_is_valid_float_char, &line,
            &timed_out)) {
      free(line);
      return has_default ? default_value : 0.0;
    }

    if (timed_out) {
      free(line);
      return has_default ? default_value : 0.0;
    }

    if (line != NULL && line[0] == '\0' && has_default) {
      free(line);
      return default_value;
    }

    if (!neuron_input_parse_double(line, &value)) {
      free(line);
      if (!interactive || attempts >= 2) {
        return has_default ? default_value : 0.0;
      }
      continue;
    }
    free(line);

    if (has_min && value < min_value) {
      if (!interactive || attempts >= 2) {
        return has_default ? default_value : min_value;
      }
      continue;
    }
    if (has_max && value > max_value) {
      if (!interactive || attempts >= 2) {
        return has_default ? default_value : max_value;
      }
      continue;
    }
    return value;
  }
}

int64_t neuron_io_input_bool(const char *prompt, int64_t has_default,
                             int64_t default_value, int64_t timeout_ms) {
  const int interactive = neuron_input_is_tty();
  int attempts = 0;
  for (;;) {
    char *line = NULL;
    int timed_out = 0;
    int64_t value = 0;
    attempts++;

    if (!neuron_input_read_filtered_line(
            prompt, 0, timeout_ms, neuron_input_is_valid_bool_char, &line,
            &timed_out)) {
      free(line);
      return has_default ? (default_value != 0 ? 1 : 0) : 0;
    }

    if (timed_out) {
      free(line);
      return has_default ? (default_value != 0 ? 1 : 0) : 0;
    }

    if (line != NULL && line[0] == '\0' && has_default) {
      free(line);
      return default_value != 0 ? 1 : 0;
    }

    if (!neuron_input_parse_bool(line, &value)) {
      free(line);
      if (!interactive || attempts >= 2) {
        return has_default ? (default_value != 0 ? 1 : 0) : 0;
      }
      continue;
    }
    free(line);
    return value != 0 ? 1 : 0;
  }
}

int64_t neuron_io_input_enum(const char *prompt, const char *options_payload,
                             int64_t option_count, int64_t has_default,
                             int64_t default_value, int64_t timeout_ms) {
  NeuronInputEnumOptions options;
  if (!neuron_input_parse_enum_options(options_payload, option_count, &options)) {
    return has_default ? default_value : 0;
  }

  int64_t fallback_value = 0;
  if (has_default && default_value >= 0 && default_value < options.count) {
    fallback_value = default_value;
  } else if (options.count > 0) {
    fallback_value = 0;
  }

  const int interactive = neuron_input_is_tty();
  if (!interactive) {
    char *line = NULL;
    int timed_out = 0;
    if (!neuron_input_read_filtered_line(prompt, 0, timeout_ms, NULL, &line,
                                         &timed_out)) {
      free(line);
      neuron_input_free_enum_options(&options);
      return fallback_value;
    }

    if (timed_out || line == NULL || line[0] == '\0') {
      free(line);
      neuron_input_free_enum_options(&options);
      return fallback_value;
    }

    const int64_t parsed = neuron_input_parse_enum_selection(line, &options);
    const int64_t resolved_count = options.count;
    free(line);
    neuron_input_free_enum_options(&options);
    if (parsed >= 0 && parsed < resolved_count) {
      return parsed;
    }
    return fallback_value;
  }

  NeuronInputRawState raw_state;
  if (!neuron_input_raw_begin(&raw_state)) {
    char *line = NULL;
    int timed_out = 0;
    if (!neuron_input_read_filtered_line(prompt, 0, timeout_ms, NULL, &line,
                                         &timed_out)) {
      free(line);
      neuron_input_free_enum_options(&options);
      return fallback_value;
    }
    const int64_t parsed = neuron_input_parse_enum_selection(line, &options);
    const int64_t resolved_count = options.count;
    free(line);
    neuron_input_free_enum_options(&options);
    if (timed_out || parsed < 0 || parsed >= resolved_count) {
      return fallback_value;
    }
    return parsed;
  }

  neuron_input_print_prompt(prompt);
  int64_t selected = fallback_value;
  if (selected < 0 || selected >= options.count) {
    selected = 0;
  }
  neuron_input_render_enum_menu(&options, selected, 1);

  const int64_t start_ms = neuron_input_now_ms();
  int64_t result = fallback_value;
  int done = 0;
  while (!done) {
    const int64_t remaining = neuron_input_remaining_ms(start_ms, timeout_ms);
    if (timeout_ms >= 0 && remaining <= 0) {
      result = fallback_value;
      break;
    }

    const int key = neuron_input_raw_read_key_with_arrows(&raw_state, remaining);
    if (key < 0) {
      result = fallback_value;
      break;
    }

    if (key == NEURON_INPUT_KEY_UP || key == NEURON_INPUT_KEY_LEFT) {
      if (options.count > 0) {
        selected = (selected + options.count - 1) % options.count;
        neuron_input_render_enum_menu(&options, selected, 0);
      }
      continue;
    }

    if (key == NEURON_INPUT_KEY_DOWN || key == NEURON_INPUT_KEY_RIGHT) {
      if (options.count > 0) {
        selected = (selected + 1) % options.count;
        neuron_input_render_enum_menu(&options, selected, 0);
      }
      continue;
    }

    if (key == '\r' || key == '\n') {
      result = selected;
      done = 1;
      continue;
    }
  }

  neuron_input_raw_end(&raw_state);
  neuron_input_free_enum_options(&options);
  fputc('\n', stdout);
  fflush(stdout);
  return result;
}

const char *neuron_io_input_string(const char *prompt, int64_t secret,
                                   int64_t has_default,
                                   const char *default_value,
                                   int64_t timeout_ms) {
  char *line = NULL;
  int timed_out = 0;
  if (!neuron_input_read_filtered_line(prompt, secret != 0, timeout_ms,
                                       neuron_input_is_valid_string_char, &line,
                                       &timed_out)) {
    free(line);
    return neuron_input_store_string(has_default ? default_value : "");
  }

  if (timed_out) {
    free(line);
    return neuron_input_store_string(has_default ? default_value : "");
  }

  if ((line == NULL || line[0] == '\0') && has_default) {
    free(line);
    return neuron_input_store_string(default_value);
  }

  const char *stored = neuron_input_store_string(line == NULL ? "" : line);
  free(line);
  return stored;
}

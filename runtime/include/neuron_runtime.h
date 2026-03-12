#ifndef NEURON_RUNTIME_H
#define NEURON_RUNTIME_H

#include "neuron_runtime_export.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*neuron_task_fn)(void *ctx);

// Runtime lifecycle
NEURON_RUNTIME_API void neuron_runtime_startup(void);
NEURON_RUNTIME_API void neuron_runtime_shutdown(void);
NEURON_RUNTIME_API void neuron_module_init(const char *module_name);
NEURON_RUNTIME_API void neuron_runtime_prepare_console(void);

// Memory manager
NEURON_RUNTIME_API void *neuron_alloc(size_t size);
NEURON_RUNTIME_API void neuron_dealloc(void *ptr);
NEURON_RUNTIME_API size_t neuron_allocated_bytes(void);

// Thread/async runtime (lightweight scheduler stubs for now)
NEURON_RUNTIME_API void neuron_thread_submit(neuron_task_fn task, void *ctx);
NEURON_RUNTIME_API void neuron_async_submit(neuron_task_fn task, void *ctx);
NEURON_RUNTIME_API void neuron_async_wait_all(void);

// Exception runtime
NEURON_RUNTIME_API void neuron_throw(const char *message);
NEURON_RUNTIME_API const char *neuron_last_exception(void);
NEURON_RUNTIME_API void neuron_clear_exception(void);
NEURON_RUNTIME_API int64_t neuron_has_exception(void);

// Standard library runtime surface
NEURON_RUNTIME_API void neuron_print_int(int64_t value);
NEURON_RUNTIME_API void neuron_print_str(const char *value);
NEURON_RUNTIME_API void neuron_repl_echo_string(const char *value);
NEURON_RUNTIME_API void neuron_system_print_int(int64_t value);
NEURON_RUNTIME_API void neuron_system_print_str(const char *value);
NEURON_RUNTIME_API void neuron_system_exit(int32_t code);

NEURON_RUNTIME_API double neuron_math_sqrt(double value);
NEURON_RUNTIME_API double neuron_math_pow(double base, double exponent);
NEURON_RUNTIME_API double neuron_math_abs(double value);

NEURON_RUNTIME_API void neuron_io_write_line(const char *text);
NEURON_RUNTIME_API int64_t neuron_io_read_int(void);
NEURON_RUNTIME_API int64_t neuron_io_input_int(const char *prompt,
                                               int64_t has_min,
                                               int64_t min_value,
                                               int64_t has_max,
                                               int64_t max_value,
                                               int64_t has_default,
                                               int64_t default_value,
                                               int64_t timeout_ms);
NEURON_RUNTIME_API float neuron_io_input_float(const char *prompt,
                                               int64_t has_min,
                                               float min_value,
                                               int64_t has_max,
                                               float max_value,
                                               int64_t has_default,
                                               float default_value,
                                               int64_t timeout_ms);
NEURON_RUNTIME_API double neuron_io_input_double(const char *prompt,
                                                 int64_t has_min,
                                                 double min_value,
                                                 int64_t has_max,
                                                 double max_value,
                                                 int64_t has_default,
                                                 double default_value,
                                                 int64_t timeout_ms);
NEURON_RUNTIME_API int64_t neuron_io_input_bool(const char *prompt,
                                                int64_t has_default,
                                                int64_t default_value,
                                                int64_t timeout_ms);
NEURON_RUNTIME_API const char *
neuron_io_input_string(const char *prompt, int64_t secret,
                       int64_t has_default, const char *default_value,
                       int64_t timeout_ms);
NEURON_RUNTIME_API int64_t
neuron_io_input_enum(const char *prompt, const char *options_payload,
                     int64_t option_count, int64_t has_default,
                     int64_t default_value, int64_t timeout_ms);

NEURON_RUNTIME_API int64_t neuron_time_now_ms(void);
NEURON_RUNTIME_API int64_t neuron_random_int(int64_t min, int64_t max);
NEURON_RUNTIME_API double neuron_random_float(void);
NEURON_RUNTIME_API void neuron_log_info(const char *msg);
NEURON_RUNTIME_API void neuron_log_warning(const char *msg);
NEURON_RUNTIME_API void neuron_log_error(const char *msg);

typedef struct {
  void *data;
  int64_t length;
  int64_t element_size;
} NeuronArray;

typedef enum {
  NEURON_MODULECPP_KIND_VOID = 0,
  NEURON_MODULECPP_KIND_INT = 1,
  NEURON_MODULECPP_KIND_FLOAT = 2,
  NEURON_MODULECPP_KIND_DOUBLE = 3,
  NEURON_MODULECPP_KIND_BOOL = 4,
  NEURON_MODULECPP_KIND_STRING = 5
} NeuronModuleCppKind;

typedef struct {
  int32_t kind;
  int32_t reserved;
  int64_t int_value;
  double float_value;
  const char *string_value;
} NeuronModuleCppValue;

NEURON_RUNTIME_API int64_t
neuron_modulecpp_register(const char *call_target, const char *library_path,
                          const char *symbol_name,
                          const char *parameter_types_csv,
                          const char *return_type);
NEURON_RUNTIME_API int64_t neuron_modulecpp_startup(void);
NEURON_RUNTIME_API void neuron_modulecpp_shutdown(void);
NEURON_RUNTIME_API int64_t
neuron_modulecpp_invoke(const char *call_target,
                        const NeuronModuleCppValue *args, int64_t arg_count,
                        NeuronModuleCppValue *out_value);

NEURON_RUNTIME_API NeuronArray *neuron_array_create(int64_t length,
                                                    int64_t element_size);
NEURON_RUNTIME_API void neuron_array_free(NeuronArray *array);
NEURON_RUNTIME_API void *neuron_array_data(NeuronArray *array);

#ifdef __cplusplus
}
#endif

#endif // NEURON_RUNTIME_H

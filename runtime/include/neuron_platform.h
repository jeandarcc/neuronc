#ifndef NEURON_PLATFORM_H
#define NEURON_PLATFORM_H

#include "neuron_runtime_export.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *NeuronPlatformLibraryHandle;

typedef struct {
  int64_t process_id;
  void *native_handle;
} NeuronPlatformProcessHandle;

typedef struct {
  void *native_handle;
} NeuronPlatformWindowHandle;

NEURON_RUNTIME_API NeuronPlatformLibraryHandle
neuron_platform_open_library(const char *path);
NEURON_RUNTIME_API void
neuron_platform_close_library(NeuronPlatformLibraryHandle handle);
NEURON_RUNTIME_API void *
neuron_platform_load_symbol(NeuronPlatformLibraryHandle handle,
                            const char *symbol_name);

NEURON_RUNTIME_API const char *neuron_platform_get_env(const char *name);
NEURON_RUNTIME_API int64_t neuron_platform_now_ms(void);
NEURON_RUNTIME_API void neuron_platform_sleep_ms(int64_t duration_ms);
NEURON_RUNTIME_API char neuron_platform_path_separator(void);
NEURON_RUNTIME_API int
neuron_platform_current_working_directory(char *buffer, size_t buffer_size);
NEURON_RUNTIME_API int
neuron_platform_current_executable_path(char *buffer, size_t buffer_size);
NEURON_RUNTIME_API const char *neuron_platform_name(void);
NEURON_RUNTIME_API const char *neuron_platform_arch_name(void);
NEURON_RUNTIME_API const char *neuron_platform_last_error(void);
NEURON_RUNTIME_API void neuron_platform_pin_current_thread(int32_t thread_index);

NEURON_RUNTIME_API int
neuron_platform_spawn_process(const char *command_line,
                              NeuronPlatformProcessHandle *out_process);
NEURON_RUNTIME_API int
neuron_platform_wait_process(NeuronPlatformProcessHandle process,
                             int32_t *out_exit_code);
NEURON_RUNTIME_API int
neuron_platform_terminate_process(NeuronPlatformProcessHandle process);

NEURON_RUNTIME_API int
neuron_platform_create_window(const char *title, int32_t width, int32_t height,
                              NeuronPlatformWindowHandle *out_window);
NEURON_RUNTIME_API void
neuron_platform_destroy_window(NeuronPlatformWindowHandle window);
NEURON_RUNTIME_API int32_t
neuron_platform_pump_events(NeuronPlatformWindowHandle window);
NEURON_RUNTIME_API int
neuron_platform_request_surface(NeuronPlatformWindowHandle window,
                                void **out_surface_handle);
NEURON_RUNTIME_API int
neuron_platform_window_should_close(NeuronPlatformWindowHandle window);

NEURON_RUNTIME_API int
neuron_platform_start_dev_server(const char *root_dir, int32_t port);
NEURON_RUNTIME_API void neuron_platform_stop_dev_server(void);

#ifdef __cplusplus
}
#endif

#endif


#ifndef Neuron_RUNTIME_PLATFORM_MANAGER_INTERNAL_H
#define Neuron_RUNTIME_PLATFORM_MANAGER_INTERNAL_H

#include "neuron_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

void neuron_platform_set_last_error(const char *fmt, ...);
void neuron_platform_clear_last_error(void);

NeuronPlatformLibraryHandle neuron_platform_open_library_impl(const char *path);
void neuron_platform_close_library_impl(NeuronPlatformLibraryHandle handle);
void *neuron_platform_load_symbol_impl(NeuronPlatformLibraryHandle handle,
                                       const char *symbol_name);
const char *neuron_platform_get_env_impl(const char *name);
int64_t neuron_platform_now_ms_impl(void);
void neuron_platform_sleep_ms_impl(int64_t duration_ms);
char neuron_platform_path_separator_impl(void);
int neuron_platform_current_working_directory_impl(char *buffer,
                                                   size_t buffer_size);
int neuron_platform_current_executable_path_impl(char *buffer,
                                                 size_t buffer_size);
const char *neuron_platform_name_impl(void);
const char *neuron_platform_arch_name_impl(void);
void neuron_platform_pin_current_thread_impl(int32_t thread_index);

int neuron_platform_spawn_process_impl(const char *command_line,
                                       NeuronPlatformProcessHandle *out_process);
int neuron_platform_wait_process_impl(NeuronPlatformProcessHandle process,
                                      int32_t *out_exit_code);
int neuron_platform_terminate_process_impl(NeuronPlatformProcessHandle process);

int neuron_platform_create_window_impl(const char *title, int32_t width,
                                       int32_t height,
                                       NeuronPlatformWindowHandle *out_window);
void neuron_platform_destroy_window_impl(NeuronPlatformWindowHandle window);
int32_t neuron_platform_pump_events_impl(NeuronPlatformWindowHandle window);
int neuron_platform_request_surface_impl(NeuronPlatformWindowHandle window,
                                         void **out_surface_handle);
int neuron_platform_window_should_close_impl(NeuronPlatformWindowHandle window);

#ifdef __cplusplus
}
#endif

#endif


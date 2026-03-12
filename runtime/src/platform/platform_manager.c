#include "neuron_platform.h"
#include "platform/platform_manager_internal.h"

NeuronPlatformLibraryHandle neuron_platform_open_library(const char *path) {
  neuron_platform_clear_last_error();
  return neuron_platform_open_library_impl(path);
}

void neuron_platform_close_library(NeuronPlatformLibraryHandle handle) {
  neuron_platform_close_library_impl(handle);
}

void *neuron_platform_load_symbol(NeuronPlatformLibraryHandle handle,
                                  const char *symbol_name) {
  neuron_platform_clear_last_error();
  return neuron_platform_load_symbol_impl(handle, symbol_name);
}

const char *neuron_platform_get_env(const char *name) {
  return neuron_platform_get_env_impl(name);
}

int64_t neuron_platform_now_ms(void) { return neuron_platform_now_ms_impl(); }

void neuron_platform_sleep_ms(int64_t duration_ms) {
  neuron_platform_sleep_ms_impl(duration_ms);
}

char neuron_platform_path_separator(void) {
  return neuron_platform_path_separator_impl();
}

int neuron_platform_current_working_directory(char *buffer,
                                              size_t buffer_size) {
  neuron_platform_clear_last_error();
  return neuron_platform_current_working_directory_impl(buffer, buffer_size);
}

int neuron_platform_current_executable_path(char *buffer, size_t buffer_size) {
  neuron_platform_clear_last_error();
  return neuron_platform_current_executable_path_impl(buffer, buffer_size);
}

const char *neuron_platform_name(void) { return neuron_platform_name_impl(); }

const char *neuron_platform_arch_name(void) {
  return neuron_platform_arch_name_impl();
}

void neuron_platform_pin_current_thread(int32_t thread_index) {
  neuron_platform_pin_current_thread_impl(thread_index);
}

int neuron_platform_spawn_process(const char *command_line,
                                  NeuronPlatformProcessHandle *out_process) {
  neuron_platform_clear_last_error();
  return neuron_platform_spawn_process_impl(command_line, out_process);
}

int neuron_platform_wait_process(NeuronPlatformProcessHandle process,
                                 int32_t *out_exit_code) {
  neuron_platform_clear_last_error();
  return neuron_platform_wait_process_impl(process, out_exit_code);
}

int neuron_platform_terminate_process(NeuronPlatformProcessHandle process) {
  neuron_platform_clear_last_error();
  return neuron_platform_terminate_process_impl(process);
}

int neuron_platform_create_window(const char *title, int32_t width,
                                  int32_t height,
                                  NeuronPlatformWindowHandle *out_window) {
  neuron_platform_clear_last_error();
  return neuron_platform_create_window_impl(title, width, height, out_window);
}

void neuron_platform_destroy_window(NeuronPlatformWindowHandle window) {
  neuron_platform_destroy_window_impl(window);
}

int32_t neuron_platform_pump_events(NeuronPlatformWindowHandle window) {
  return neuron_platform_pump_events_impl(window);
}

int neuron_platform_request_surface(NeuronPlatformWindowHandle window,
                                    void **out_surface_handle) {
  neuron_platform_clear_last_error();
  return neuron_platform_request_surface_impl(window, out_surface_handle);
}

int neuron_platform_window_should_close(NeuronPlatformWindowHandle window) {
  return neuron_platform_window_should_close_impl(window);
}

int neuron_platform_start_dev_server(const char *root_dir, int32_t port) {
  (void)root_dir;
  (void)port;
  neuron_platform_set_last_error(
      "dev server capability is not implemented in this iteration");
  return 0;
}

void neuron_platform_stop_dev_server(void) {}

#include "gpu_internal.h"

#include <stdio.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/webgpu.h>
#endif

#ifndef NPP_ENABLE_WEBGPU_BACKEND
#define NPP_ENABLE_WEBGPU_BACKEND 0
#endif

#if NPP_ENABLE_WEBGPU_BACKEND && defined(__EMSCRIPTEN__)

static int g_webgpu_initialized = 0;

int neuron_gpu_webgpu_try_initialize(char *error_buffer, size_t error_size) {
  if (g_webgpu_initialized) {
    if (error_buffer != NULL && error_size > 0) {
      error_buffer[0] = '\0';
    }
    return 1;
  }

  WGPUDevice device = emscripten_webgpu_get_device();
  if (device == NULL) {
    if (error_buffer != NULL && error_size > 0) {
      snprintf(error_buffer, error_size,
               "emscripten_webgpu_get_device returned null device");
    }
    return 0;
  }

  g_webgpu_initialized = 1;
  if (error_buffer != NULL && error_size > 0) {
    error_buffer[0] = '\0';
  }
  return 1;
}

void neuron_gpu_webgpu_shutdown(void) { g_webgpu_initialized = 0; }

int neuron_gpu_webgpu_supports_op(NeuronGpuOpKind op) {
  (void)op;
  return 0;
}

int neuron_gpu_webgpu_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                      const float *b, float *out,
                                      int32_t element_count,
                                      char *error_buffer, size_t error_size) {
  (void)op;
  (void)a;
  (void)b;
  (void)out;
  (void)element_count;
  if (error_buffer != NULL && error_size > 0) {
    snprintf(error_buffer, error_size,
             "WebGPU binary dispatch is not implemented yet");
  }
  return -1;
}

int neuron_gpu_webgpu_dispatch_fma(const float *a, const float *b,
                                   const float *c, float *out,
                                   int32_t element_count,
                                   char *error_buffer, size_t error_size) {
  (void)a;
  (void)b;
  (void)c;
  (void)out;
  (void)element_count;
  if (error_buffer != NULL && error_size > 0) {
    snprintf(error_buffer, error_size,
             "WebGPU fma dispatch is not implemented yet");
  }
  return -1;
}

int neuron_gpu_webgpu_dispatch_matmul(const NeuronGpuMatMulDispatchDesc *desc,
                                      char *error_buffer, size_t error_size) {
  (void)desc;
  if (error_buffer != NULL && error_size > 0) {
    snprintf(error_buffer, error_size,
             "WebGPU matmul dispatch is not implemented yet");
  }
  return -1;
}

#else

int neuron_gpu_webgpu_try_initialize(char *error_buffer, size_t error_size) {
  if (error_buffer != NULL && error_size > 0) {
    snprintf(error_buffer, error_size,
             "WebGPU backend is unavailable for this build target");
  }
  return 0;
}

void neuron_gpu_webgpu_shutdown(void) {}

int neuron_gpu_webgpu_supports_op(NeuronGpuOpKind op) {
  (void)op;
  return 0;
}

int neuron_gpu_webgpu_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                      const float *b, float *out,
                                      int32_t element_count,
                                      char *error_buffer, size_t error_size) {
  (void)op;
  (void)a;
  (void)b;
  (void)out;
  (void)element_count;
  if (error_buffer != NULL && error_size > 0) {
    snprintf(error_buffer, error_size,
             "WebGPU backend is unavailable for this build target");
  }
  return -1;
}

int neuron_gpu_webgpu_dispatch_fma(const float *a, const float *b,
                                   const float *c, float *out,
                                   int32_t element_count,
                                   char *error_buffer, size_t error_size) {
  (void)a;
  (void)b;
  (void)c;
  (void)out;
  (void)element_count;
  if (error_buffer != NULL && error_size > 0) {
    snprintf(error_buffer, error_size,
             "WebGPU backend is unavailable for this build target");
  }
  return -1;
}

int neuron_gpu_webgpu_dispatch_matmul(const NeuronGpuMatMulDispatchDesc *desc,
                                      char *error_buffer, size_t error_size) {
  (void)desc;
  if (error_buffer != NULL && error_size > 0) {
    snprintf(error_buffer, error_size,
             "WebGPU backend is unavailable for this build target");
  }
  return -1;
}

#endif

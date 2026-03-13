#ifndef Neuron_RUNTIME_GPU_INTERNAL_H
#define Neuron_RUNTIME_GPU_INTERNAL_H

#include "neuron_gpu.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int neuron_gpu_dispatch_tensor_binary(NeuronGpuOpKind op, const float *a,
                                      const float *b, float *out,
                                      int32_t element_count);
int neuron_gpu_dispatch_tensor_fma(const float *a, const float *b,
                                   const float *c, float *out,
                                   int32_t element_count);
int neuron_gpu_prepare_cpu_tensor(const float *host_data, int32_t element_count);

typedef struct NeuronPackedMatrix {
  int32_t rows;
  int32_t cols;
  int32_t kc;
  int32_t nc;
  int32_t kBlocks;
  int32_t nBlocks;
  size_t panelCount;
  size_t *offsets;
  float *data;
  uint64_t checksum;
} NeuronPackedMatrix;

typedef struct {
  const float *a;
  const float *b;
  const NeuronPackedMatrix *packed_b;
  float *out;
  const float *bias;
  int32_t bias_rows;
  int32_t bias_cols;
  const float *residual;
  int32_t residual_cols;
  int32_t m;
  int32_t n;
  int32_t k;
  int32_t activation;
  int32_t accumulate;
} NeuronGpuMatMulDispatchDesc;

int neuron_gpu_dispatch_tensor_matmul(const NeuronGpuMatMulDispatchDesc *desc);

int neuron_gpu_cuda_try_initialize(char *error_buffer, size_t error_size);
void neuron_gpu_cuda_shutdown(void);
int neuron_gpu_cuda_supports_op(NeuronGpuOpKind op);
int neuron_gpu_cuda_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                    const float *b, float *out,
                                    int32_t element_count, char *error_buffer,
                                    size_t error_size);
int neuron_gpu_cuda_dispatch_fma(const float *a, const float *b,
                                 const float *c, float *out,
                                 int32_t element_count, char *error_buffer,
                                 size_t error_size);
int neuron_gpu_cuda_dispatch_matmul(const NeuronGpuMatMulDispatchDesc *desc,
                                    char *error_buffer, size_t error_size);

int neuron_gpu_vulkan_try_initialize(char *error_buffer, size_t error_size);
void neuron_gpu_vulkan_shutdown(void);
int neuron_gpu_vulkan_supports_op(NeuronGpuOpKind op);
int neuron_gpu_vulkan_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                      const float *b, float *out,
                                      int32_t element_count, char *error_buffer,
                                      size_t error_size);
int neuron_gpu_vulkan_dispatch_fma(const float *a, const float *b,
                                   const float *c, float *out,
                                   int32_t element_count, char *error_buffer,
                                   size_t error_size);
int neuron_gpu_vulkan_dispatch_matmul(const NeuronGpuMatMulDispatchDesc *desc,
                                      char *error_buffer, size_t error_size);
void neuron_gpu_vulkan_set_scope_preference(NeuronGpuScopeMode mode,
                                            NeuronGpuDeviceClass device_class);
int neuron_gpu_vulkan_scope_begin(char *error_buffer, size_t error_size);
int neuron_gpu_vulkan_scope_end(char *error_buffer, size_t error_size);
int neuron_gpu_vulkan_materialize(const void *host_ptr, size_t byte_size,
                                  char *error_buffer, size_t error_size);
int neuron_gpu_vulkan_export_buffer(const void *host_ptr, size_t byte_size,
                                    void **out_buffer_handle,
                                    size_t *out_buffer_size,
                                    uint32_t *out_queue_family_index,
                                    char *error_buffer, size_t error_size);

int neuron_gpu_webgpu_try_initialize(char *error_buffer, size_t error_size);
void neuron_gpu_webgpu_shutdown(void);
int neuron_gpu_webgpu_supports_op(NeuronGpuOpKind op);
int neuron_gpu_webgpu_dispatch_binary(NeuronGpuOpKind op, const float *a,
                                      const float *b, float *out,
                                      int32_t element_count,
                                      char *error_buffer, size_t error_size);
int neuron_gpu_webgpu_dispatch_fma(const float *a, const float *b,
                                   const float *c, float *out,
                                   int32_t element_count,
                                   char *error_buffer, size_t error_size);
int neuron_gpu_webgpu_dispatch_matmul(const NeuronGpuMatMulDispatchDesc *desc,
                                      char *error_buffer, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif // Neuron_RUNTIME_GPU_INTERNAL_H

#ifndef NEURON_TENSOR_H
#define NEURON_TENSOR_H

#include "neuron_runtime_export.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  NEURON_TENSOR_F32 = 0,
} NeuronTensorType;

typedef struct {
  float *data;
  int32_t *shape;
  int32_t dimensions;
  int32_t size;
  int32_t element_size;
  NeuronTensorType data_type;
} NeuronTensor;

typedef struct NeuronPackedMatrix NeuronPackedMatrix;

typedef enum {
  NEURON_TENSOR_ACTIVATION_NONE = 0,
  NEURON_TENSOR_ACTIVATION_RELU = 1,
  NEURON_TENSOR_ACTIVATION_GELU = 2,
} NeuronTensorActivation;

typedef enum {
  NEURON_TENSOR_STRUCTURE_NONE = 0,
  NEURON_TENSOR_STRUCTURE_CIRCULANT = 1,
  NEURON_TENSOR_STRUCTURE_TOEPLITZ = 2,
} NeuronTensorStructureKind;

typedef enum {
  NEURON_TENSOR_EXEC_AUTO = 0,
  NEURON_TENSOR_EXEC_GPU_PREFER = 1,
  NEURON_TENSOR_EXEC_CPU_ONLY = 2,
} NeuronTensorExecHint;

typedef struct {
  int32_t rows;
  int32_t cols;
  float circulant_score;
  float toeplitz_score;
  float sparsity;
  float selected_score;
  NeuronTensorStructureKind selected_kind;
} NeuronTensorStructureInfo;

enum {
  NEURON_TENSOR_MATMUL_FLAG_ACCUMULATE = 1u << 0,
  NEURON_TENSOR_MATMUL_FLAG_AUTOTUNE = 1u << 1,
};

// Memory management
NEURON_RUNTIME_API NeuronTensor *
neuron_tensor_create(int32_t dimensions, int32_t *shape);
NEURON_RUNTIME_API NeuronTensor *neuron_tensor_create_default();
NEURON_RUNTIME_API NeuronTensor *neuron_tensor_random_2d(int32_t rows,
                                                         int32_t cols);
NEURON_RUNTIME_API void neuron_tensor_free(NeuronTensor *tensor);

// Basic Ops
NEURON_RUNTIME_API NeuronTensor *
neuron_tensor_add_ex(NeuronTensor *a, NeuronTensor *b,
                     NeuronTensorExecHint hint);
NEURON_RUNTIME_API NeuronTensor *
neuron_tensor_sub_ex(NeuronTensor *a, NeuronTensor *b,
                     NeuronTensorExecHint hint);
NEURON_RUNTIME_API NeuronTensor *
neuron_tensor_mul_ex(NeuronTensor *a, NeuronTensor *b,
                     NeuronTensorExecHint hint);
NEURON_RUNTIME_API NeuronTensor *
neuron_tensor_div_ex(NeuronTensor *a, NeuronTensor *b,
                     NeuronTensorExecHint hint);
NEURON_RUNTIME_API NeuronTensor *
neuron_tensor_fma_ex(NeuronTensor *a, NeuronTensor *b, NeuronTensor *c,
                     NeuronTensorExecHint hint);
NEURON_RUNTIME_API NeuronTensor *neuron_tensor_add(NeuronTensor *a,
                                                   NeuronTensor *b);
NEURON_RUNTIME_API NeuronTensor *neuron_tensor_sub(NeuronTensor *a,
                                                   NeuronTensor *b);
NEURON_RUNTIME_API NeuronTensor *neuron_tensor_mul(NeuronTensor *a,
                                                   NeuronTensor *b);
NEURON_RUNTIME_API NeuronTensor *neuron_tensor_div(NeuronTensor *a,
                                                   NeuronTensor *b);
NEURON_RUNTIME_API NeuronTensor *neuron_tensor_fma(NeuronTensor *a,
                                                   NeuronTensor *b,
                                                   NeuronTensor *c);
NEURON_RUNTIME_API NeuronTensor *neuron_tensor_matmul(NeuronTensor *a,
                                                      NeuronTensor *b);
NEURON_RUNTIME_API NeuronTensor *
neuron_tensor_matmul_ex_hint(NeuronTensor *a, NeuronTensor *b,
                             NeuronTensor *out, uint32_t flags,
                             NeuronTensorExecHint hint);
NEURON_RUNTIME_API NeuronTensor *
neuron_tensor_matmul_ex(NeuronTensor *a, NeuronTensor *b, NeuronTensor *out,
                        uint32_t flags);
NEURON_RUNTIME_API NeuronTensor *
neuron_tensor_matmul_add_ex_hint(NeuronTensor *a, NeuronTensor *b,
                                 NeuronTensor *bias,
                                 NeuronTensorExecHint hint);
NEURON_RUNTIME_API NeuronTensor *
neuron_tensor_matmul_packed(NeuronTensor *a,
                            const NeuronPackedMatrix *packed_b,
                            NeuronTensor *out, uint32_t flags);
NEURON_RUNTIME_API NeuronTensor *
neuron_tensor_matmul_add(NeuronTensor *a, NeuronTensor *b,
                         NeuronTensor *bias);
NEURON_RUNTIME_API NeuronTensor *neuron_tensor_linear_fused_ex_hint(
    NeuronTensor *a, NeuronTensor *b, const NeuronPackedMatrix *packed_b,
    NeuronTensor *bias, NeuronTensor *residual, int32_t activation,
    NeuronTensor *out, uint32_t flags, NeuronTensorExecHint hint);
NEURON_RUNTIME_API NeuronTensor *neuron_tensor_linear_fused(
    NeuronTensor *a, NeuronTensor *b, const NeuronPackedMatrix *packed_b,
    NeuronTensor *bias, NeuronTensor *residual, int32_t activation,
    NeuronTensor *out, uint32_t flags);
NEURON_RUNTIME_API NeuronTensor *
neuron_tensor_conv2d_batchnorm_relu_ex_hint(
    NeuronTensor *input, NeuronTensor *kernel, NeuronTensor *bias,
    NeuronTensor *gamma, NeuronTensor *beta, NeuronTensor *mean,
    NeuronTensor *variance, float epsilon, int32_t stride_h, int32_t stride_w,
    int32_t padding_h, int32_t padding_w, NeuronTensorExecHint hint);
NEURON_RUNTIME_API NeuronTensor *neuron_tensor_conv2d_batchnorm_relu(
    NeuronTensor *input, NeuronTensor *kernel, NeuronTensor *bias,
    NeuronTensor *gamma, NeuronTensor *beta, NeuronTensor *mean,
    NeuronTensor *variance, float epsilon, int32_t stride_h, int32_t stride_w,
    int32_t padding_h, int32_t padding_w);
NEURON_RUNTIME_API int
neuron_tensor_pack_b(const NeuronTensor *b, NeuronPackedMatrix **out_packed);
NEURON_RUNTIME_API void
neuron_tensor_packed_free(NeuronPackedMatrix *packed);
NEURON_RUNTIME_API void neuron_tensor_release_workspace_cache(void);
NEURON_RUNTIME_API int
neuron_tensor_analyze_structure(const NeuronTensor *matrix,
                                NeuronTensorStructureInfo *out_info);

// IO
NEURON_RUNTIME_API void neuron_tensor_print(NeuronTensor *tensor);

#ifdef __cplusplus
}
#endif

#endif // NEURON_TENSOR_H

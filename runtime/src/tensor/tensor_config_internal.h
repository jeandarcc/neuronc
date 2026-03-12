#ifndef NPP_RUNTIME_TENSOR_CONFIG_INTERNAL_H
#define NPP_RUNTIME_TENSOR_CONFIG_INTERNAL_H

#include <stdint.h>

typedef int TensorProfile;
#ifndef NPP_RUNTIME_TENSOR_KERNEL_VARIANT_DEFINED
#define NPP_RUNTIME_TENSOR_KERNEL_VARIANT_DEFINED

typedef int TensorKernelVariant;

enum {
  kTensorKernel8x8 = 0,
  kTensorKernel4x16 = 1,
};
#endif

enum {
  kTensorProfileBalanced = 0,
  kTensorProfileGemmParity = 1,
  kTensorProfileAIFused = 2,
};

int tensor_requested_threads(void);
TensorProfile tensor_runtime_profile(void);
int tensor_env_enabled(const char *name);
float tensor_env_float_or(const char *name, float fallback);
int tensor_structured_enabled(void);
int tensor_structured_debug_enabled(void);
float tensor_structured_threshold(void);
float tensor_structured_hybrid_threshold(void);
float tensor_structured_residual_eps(void);
float tensor_structured_hybrid_max_density(void);
float tensor_structured_hybrid_dense_fallback_density(void);
float tensor_structured_hybrid_dense_correction_density(void);
int32_t tensor_structured_hybrid_dense_correction_max_n(void);
int tensor_pin_threads_enabled(void);
int tensor_autotune_enabled(void);
int tensor_retune_requested(void);
TensorKernelVariant tensor_choose_kernel_variant(int32_t m, int32_t n,
                                                 int32_t k,
                                                 TensorProfile profile);
int32_t tensor_bucket_i32(int32_t value, int32_t step);
int tensor_lookup_tuned_threads(int32_t m, int32_t n, int32_t k,
                                TensorProfile profile);
void tensor_store_tuned_threads(int32_t m, int32_t n, int32_t k,
                                TensorProfile profile, int threads);

#endif


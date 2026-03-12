#include "tensor/tensor_config_internal.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TENSOR_NC 256
#define TENSOR_MR 8
#define TENSOR_NR 8
#define TENSOR_PARALLEL_MIN_WORKLOAD 67108864LL
#define TENSOR_TUNE_CACHE_SIZE 64

typedef struct {
  int valid;
  int32_t m;
  int32_t n;
  int32_t kBucket;
  int32_t profile;
  int threads;
} TensorTuneEntry;

static TensorTuneEntry g_tensorTuneCache[TENSOR_TUNE_CACHE_SIZE];

int tensor_requested_threads(void) {
  const char *env = getenv("NEURON_TENSOR_THREADS");
  if (env == NULL || *env == '\0') {
    return 0;
  }
  const int parsed = atoi(env);
  return parsed > 0 ? parsed : 0;
}

TensorProfile tensor_runtime_profile(void) {
  const char *env = getenv("NEURON_TENSOR_PROFILE");
  if (env == NULL || *env == '\0') {
    return kTensorProfileBalanced;
  }

  char lowered[32];
  size_t i = 0;
  for (; env[i] != '\0' && i + 1 < sizeof(lowered); ++i) {
    lowered[i] = (char)tolower((unsigned char)env[i]);
  }
  lowered[i] = '\0';

  if (strcmp(lowered, "gemm_parity") == 0 || strcmp(lowered, "gemmparity") == 0) {
    return kTensorProfileGemmParity;
  }
  if (strcmp(lowered, "ai_fused") == 0 || strcmp(lowered, "aifused") == 0) {
    return kTensorProfileAIFused;
  }
  return kTensorProfileBalanced;
}

int tensor_env_enabled(const char *name) {
  const char *env = getenv(name);
  if (env == NULL || *env == '\0') {
    return 0;
  }

  char lowered[16];
  size_t i = 0;
  for (; env[i] != '\0' && i + 1 < sizeof(lowered); ++i) {
    lowered[i] = (char)tolower((unsigned char)env[i]);
  }
  lowered[i] = '\0';

  return strcmp(lowered, "1") == 0 || strcmp(lowered, "true") == 0 ||
         strcmp(lowered, "yes") == 0 || strcmp(lowered, "on") == 0;
}

float tensor_env_float_or(const char *name, float fallback) {
  const char *env = getenv(name);
  if (env == NULL || *env == '\0') {
    return fallback;
  }
  char *end = NULL;
  const float parsed = strtof(env, &end);
  if (end == env) {
    return fallback;
  }
  return parsed;
}

int tensor_structured_enabled(void) {
  const char *env = getenv("NEURON_TENSOR_STRUCTURED_MATMUL");
  if (env == NULL || *env == '\0') {
    return 1;
  }
  return tensor_env_enabled("NEURON_TENSOR_STRUCTURED_MATMUL");
}

int tensor_structured_debug_enabled(void) {
  return tensor_env_enabled("NEURON_TENSOR_STRUCTURED_DEBUG");
}

float tensor_structured_threshold(void) {
  float threshold = tensor_env_float_or("NEURON_TENSOR_STRUCTURED_THRESHOLD", 0.99f);
  if (threshold < 0.0f) {
    threshold = 0.0f;
  } else if (threshold > 1.0f) {
    threshold = 1.0f;
  }
  return threshold;
}

float tensor_structured_hybrid_threshold(void) {
  float threshold =
      tensor_env_float_or("NEURON_TENSOR_STRUCTURED_HYBRID_THRESHOLD", 0.85f);
  if (threshold < 0.0f) {
    threshold = 0.0f;
  } else if (threshold > 1.0f) {
    threshold = 1.0f;
  }
  return threshold;
}

float tensor_structured_residual_eps(void) {
  float eps = tensor_env_float_or("NEURON_TENSOR_STRUCTURED_RESIDUAL_EPS", 1e-2f);
  if (eps < 0.0f) {
    eps = 0.0f;
  }
  return eps;
}

float tensor_structured_hybrid_max_density(void) {
  float density =
      tensor_env_float_or("NEURON_TENSOR_STRUCTURED_HYBRID_MAX_DENSITY", 0.10f);
  if (density < 0.0f) {
    density = 0.0f;
  } else if (density > 1.0f) {
    density = 1.0f;
  }
  return density;
}

float tensor_structured_hybrid_dense_fallback_density(void) {
  float density = tensor_env_float_or(
      "NEURON_TENSOR_HYBRID_DENSE_FALLBACK_DENSITY", 0.035f);
  if (density < 0.0f) {
    density = 0.0f;
  } else if (density > 1.0f) {
    density = 1.0f;
  }
  return density;
}

float tensor_structured_hybrid_dense_correction_density(void) {
  float density = tensor_env_float_or(
      "NEURON_TENSOR_HYBRID_DENSE_CORRECTION_DENSITY", 0.008f);
  if (density < 0.0f) {
    density = 0.0f;
  } else if (density > 1.0f) {
    density = 1.0f;
  }
  return density;
}

int32_t tensor_structured_hybrid_dense_correction_max_n(void) {
  const char *env = getenv("NEURON_TENSOR_HYBRID_DENSE_CORRECTION_MAX_N");
  if (env == NULL || *env == '\0') {
    return 2048;
  }
  char *end = NULL;
  long parsed = strtol(env, &end, 10);
  if (end == env) {
    return 2048;
  }
  if (parsed < 64) {
    parsed = 64;
  } else if (parsed > 32768) {
    parsed = 32768;
  }
  return (int32_t)parsed;
}

int tensor_pin_threads_enabled(void) {
  return tensor_env_enabled("NEURON_TENSOR_PIN_THREADS");
}

int tensor_autotune_enabled(void) {
  const char *env = getenv("NEURON_TENSOR_AUTOTUNE");
  if (env == NULL || *env == '\0') {
    return 1;
  }
  return tensor_env_enabled("NEURON_TENSOR_AUTOTUNE");
}

int tensor_retune_requested(void) {
  return tensor_env_enabled("NEURON_TENSOR_RETUNE");
}

TensorKernelVariant tensor_choose_kernel_variant(int32_t m, int32_t n,
                                                 int32_t k,
                                                 TensorProfile profile) {
  if (m <= 0 || n <= 0 || k <= 0) {
    return kTensorKernel8x8;
  }

  const int shapeFriendly = (n % 16) == 0 && m >= 4 && k >= 64;
  if (!shapeFriendly) {
    return kTensorKernel8x8;
  }

  if (profile == kTensorProfileGemmParity || profile == kTensorProfileAIFused) {
    return kTensorKernel4x16;
  }

  if ((m == 1024 && n == 1024) || (m == 768 && (n == 768 || n == 3072)) ||
      (m == 3072 && n == 768)) {
    return kTensorKernel4x16;
  }

  if (n >= 256) {
    return kTensorKernel4x16;
  }

  return kTensorKernel8x8;
}

int32_t tensor_bucket_i32(int32_t value, int32_t step) {
  if (value <= 0 || step <= 0) {
    return value;
  }
  return ((value + step - 1) / step) * step;
}

int tensor_lookup_tuned_threads(int32_t m, int32_t n, int32_t k,
                                TensorProfile profile) {
  const int32_t kBucket = tensor_bucket_i32(k, 64);
  for (int i = 0; i < TENSOR_TUNE_CACHE_SIZE; ++i) {
    const TensorTuneEntry *entry = &g_tensorTuneCache[i];
    if (entry->valid && entry->m == m && entry->n == n &&
        entry->kBucket == kBucket &&
        entry->profile == (int32_t)profile) {
      return entry->threads;
    }
  }
  return 0;
}

void tensor_store_tuned_threads(int32_t m, int32_t n, int32_t k,
                                TensorProfile profile, int threads) {
  if (threads <= 0) {
    return;
  }
  const int32_t kBucket = tensor_bucket_i32(k, 64);
  const int slot =
      (int)((((uint32_t)m * 73856093u) ^ ((uint32_t)n * 19349663u) ^
             ((uint32_t)kBucket * 83492791u) ^ (uint32_t)profile) %
            (uint32_t)TENSOR_TUNE_CACHE_SIZE);
  g_tensorTuneCache[slot].valid = 1;
  g_tensorTuneCache[slot].m = m;
  g_tensorTuneCache[slot].n = n;
  g_tensorTuneCache[slot].kBucket = kBucket;
  g_tensorTuneCache[slot].profile = (int32_t)profile;
  g_tensorTuneCache[slot].threads = threads;
}


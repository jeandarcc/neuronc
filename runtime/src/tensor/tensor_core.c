#include "neuron_runtime.h"
#include "neuron_platform.h"
#include "tensor_config_internal.h"
#include "tensor_core_internal.h"


#include <stdlib.h>

#if defined(_OPENMP)
#include <omp.h>
#endif

TENSOR_THREAD_LOCAL TensorThreadWorkspace g_tensorWorkspace = {0};
TENSOR_THREAD_LOCAL TensorPackedBCache g_tensorPackedBCache = {0};

int32_t tensor_min_i32(int32_t a, int32_t b) { return a < b ? a : b; }

void *tensor_aligned_alloc(size_t size, size_t alignment) {
  if (size == 0 || alignment < sizeof(void *)) {
    return NULL;
  }
  const size_t padded = size + alignment + sizeof(void *);
  void *raw = neuron_alloc(padded);
  if (raw == NULL) {
    return NULL;
  }
  uintptr_t base = (uintptr_t)raw + sizeof(void *);
  uintptr_t aligned = (base + (alignment - 1u)) & ~(uintptr_t)(alignment - 1u);
  ((void **)aligned)[-1] = raw;
  return (void *)aligned;
}

void tensor_aligned_free(void *ptr) {
  if (ptr == NULL) {
    return;
  }
  neuron_dealloc(((void **)ptr)[-1]);
}

float *tensor_workspace_reserve(float **slot, size_t *capacity,
                                size_t elementCount) {
  if (slot == NULL || capacity == NULL) {
    return NULL;
  }
  if (elementCount == 0) {
    return *slot;
  }
  if (*slot != NULL && *capacity >= elementCount) {
    return *slot;
  }

  float *replacement =
      (float *)tensor_aligned_alloc(elementCount * sizeof(float), 64u);
  if (replacement == NULL) {
    return NULL;
  }

  tensor_aligned_free(*slot);
  *slot = replacement;
  *capacity = elementCount;
  return *slot;
}

TensorComplex *tensor_workspace_reserve_complex(TensorComplex **slot,
                                                size_t *capacity,
                                                size_t elementCount) {
  if (slot == NULL || capacity == NULL) {
    return NULL;
  }
  if (elementCount == 0) {
    return *slot;
  }
  if (*slot != NULL && *capacity >= elementCount) {
    return *slot;
  }

  TensorComplex *replacement = (TensorComplex *)tensor_aligned_alloc(
      elementCount * sizeof(TensorComplex), 64u);
  if (replacement == NULL) {
    return NULL;
  }

  tensor_aligned_free(*slot);
  *slot = replacement;
  *capacity = elementCount;
  return *slot;
}

uint32_t *tensor_workspace_reserve_u32(uint32_t **slot, size_t *capacity,
                                       size_t elementCount) {
  if (slot == NULL || capacity == NULL) {
    return NULL;
  }
  if (elementCount == 0) {
    return *slot;
  }
  if (*slot != NULL && *capacity >= elementCount) {
    return *slot;
  }

  uint32_t *replacement =
      (uint32_t *)tensor_aligned_alloc(elementCount * sizeof(uint32_t), 64u);
  if (replacement == NULL) {
    return NULL;
  }

  tensor_aligned_free(*slot);
  *slot = replacement;
  *capacity = elementCount;
  return *slot;
}

void tensor_pin_current_thread(int threadIndex) {
  if (!tensor_pin_threads_enabled() || threadIndex < 0) {
    return;
  }
  neuron_platform_pin_current_thread(threadIndex);
}

int tensor_recommended_threads(int64_t workload, int32_t m, int32_t n,
                               int32_t k) {
#if defined(_OPENMP)
  int threads = omp_get_max_threads();
  const int requested = tensor_requested_threads();
  if (requested > 0 && requested < threads) {
    threads = requested;
  }
  if (threads <= 1) {
    return 1;
  }

  const TensorProfile profile = tensor_runtime_profile();
  const int autotune = tensor_autotune_enabled();
  const int retune = tensor_retune_requested();

  if (autotune && !retune) {
    const int cached = tensor_lookup_tuned_threads(m, n, k, profile);
    if (cached > 0) {
      const int highWorkload = workload >= TENSOR_PARALLEL_MIN_WORKLOAD &&
                               m >= TENSOR_MR && n >= (2 * TENSOR_NC);
      if (!(highWorkload && cached == 1)) {
        return tensor_min_i32(cached, threads);
      }
    }
  }

  if (profile == kTensorProfileGemmParity) {
    if (workload >= (TENSOR_PARALLEL_MIN_WORKLOAD / 2) && n >= TENSOR_NR) {
      tensor_store_tuned_threads(m, n, k, profile, threads);
      return threads;
    }
    tensor_store_tuned_threads(m, n, k, profile, 1);
    return 1;
  }

  if (profile == kTensorProfileAIFused) {
    if (m <= 64 || n <= 64) {
      const int tuned = tensor_min_i32(threads, 4);
      tensor_store_tuned_threads(m, n, k, profile, tuned);
      return tuned;
    }
    if (workload >= TENSOR_PARALLEL_MIN_WORKLOAD) {
      tensor_store_tuned_threads(m, n, k, profile, threads);
      return threads;
    }
    tensor_store_tuned_threads(m, n, k, profile, 1);
    return 1;
  }

  if (!autotune) {
    if (workload < TENSOR_PARALLEL_MIN_WORKLOAD || m < TENSOR_MR ||
        n < (2 * TENSOR_NC)) {
      tensor_store_tuned_threads(m, n, k, profile, 1);
      return 1;
    }
    tensor_store_tuned_threads(m, n, k, profile, threads);
    return threads;
  }

  if (workload < (TENSOR_PARALLEL_MIN_WORKLOAD / 2) || m < TENSOR_MR ||
      n < TENSOR_NC) {
    tensor_store_tuned_threads(m, n, k, profile, 1);
    return 1;
  }
  if (n < (2 * TENSOR_NC)) {
    const int tuned = tensor_min_i32(threads, 2);
    tensor_store_tuned_threads(m, n, k, profile, tuned);
    return tuned;
  }
  tensor_store_tuned_threads(m, n, k, profile, threads);
  return threads;
#else
  (void)workload;
  (void)m;
  (void)n;
  (void)k;
  return 1;
#endif
}

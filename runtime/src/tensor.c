#include "gpu_internal.h"
#include "neuron_gpu.h"
#include "neuron_runtime.h"
#include "neuron_tensor.h"
#include "tensor/tensor_config_internal.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(__AVX2__) || defined(__SSE__)
#include <immintrin.h>
#endif

#if defined(_OPENMP)
#include <omp.h>
#endif

#include "tensor/tensor_core_internal.h"
#include "tensor/tensor_math_internal.h"

static TENSOR_THREAD_LOCAL int g_tensorStructuredDebugPrinted = 0;

static int tensor_is_valid(const NeuronTensor *t);

#include "tensor/tensor_microkernel_internal.h"

static float tensor_clamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

static uint64_t tensor_hash_mix_u64(uint64_t seed, uint32_t value) {
  seed ^= (uint64_t)value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
  return seed;
}

static uint32_t tensor_float_bits(float value) {
  union {
    float f;
    uint32_t u;
  } conv;
  conv.f = value;
  return conv.u;
}

static uint64_t tensor_matrix_fingerprint(const float *data, int32_t rows,
                                          int32_t cols) {
  if (data == NULL || rows <= 0 || cols <= 0) {
    return 0ULL;
  }

  const int32_t size = rows * cols;
  const int32_t sampleCount = size <= 4096 ? size : 64;
  const int32_t step =
      sampleCount > 0 ? tensor_min_i32(size / sampleCount, size) : 1;
  const int32_t stride = step > 0 ? step : 1;

  uint64_t hash = 1469598103934665603ULL;
  for (int32_t i = 0; i < size; i += stride) {
    hash = tensor_hash_mix_u64(hash, tensor_float_bits(data[i]));
  }

  hash = tensor_hash_mix_u64(hash, (uint32_t)rows);
  hash = tensor_hash_mix_u64(hash, (uint32_t)cols);
  hash = tensor_hash_mix_u64(hash, (uint32_t)size);
  return hash;
}

static TensorStructureAnalysis
tensor_analyze_structure_raw(const float *data, int32_t rows, int32_t cols) {
  TensorStructureAnalysis analysis;
  memset(&analysis, 0, sizeof(analysis));
  analysis.selectedKind = kTensorStructuredNone;
  if (data == NULL || rows <= 0 || cols <= 0) {
    return analysis;
  }

  float normSq = 0.0f;
  float circErrSq = 0.0f;
  float toeErrSq = 0.0f;
  float maxAbs = 0.0f;
  int32_t nearZero = 0;
  const float eps = 1e-6f;

  for (int32_t r = 0; r < rows; ++r) {
    const float *rowData = data + (size_t)r * (size_t)cols;
    for (int32_t c = 0; c < cols; ++c) {
      const float v = rowData[c];
      normSq += v * v;
      const float absV = fabsf(v);
      if (absV > maxAbs) {
        maxAbs = absV;
      }
      if (fabsf(v) <= eps) {
        ++nearZero;
      }
    }
  }

  if (rows == cols) {
    for (int32_t r = 0; r < rows; ++r) {
      for (int32_t c = 0; c < cols; ++c) {
        const int32_t shifted = (c - r) % cols;
        const int32_t index = shifted < 0 ? shifted + cols : shifted;
        const float expected = data[index];
        const float actual = data[(size_t)r * (size_t)cols + (size_t)c];
        const float diff = actual - expected;
        circErrSq += diff * diff;
      }
    }
  } else {
    circErrSq = normSq;
  }

  for (int32_t r = 0; r < rows; ++r) {
    for (int32_t c = 0; c < cols; ++c) {
      float expected = 0.0f;
      if (c >= r) {
        expected = data[c - r];
      } else {
        expected = data[(size_t)(r - c) * (size_t)cols];
      }
      const float actual = data[(size_t)r * (size_t)cols + (size_t)c];
      const float diff = actual - expected;
      toeErrSq += diff * diff;
    }
  }

  const float denom = sqrtf(normSq + 1e-12f);
  const float circErr = sqrtf(circErrSq);
  const float toeErr = sqrtf(toeErrSq);
  analysis.circulantScore = tensor_clamp01(1.0f - circErr / denom);
  analysis.toeplitzScore = tensor_clamp01(1.0f - toeErr / denom);
  analysis.sparsity = (float)nearZero / (float)(rows * cols);

  if (rows == cols) {
    const float scoreDiff =
        fabsf(analysis.circulantScore - analysis.toeplitzScore);
    const float tieWindow = 0.02f;
    int chooseCirculant = analysis.circulantScore >= analysis.toeplitzScore;

    if (scoreDiff <= tieWindow) {
      float residualEps = tensor_structured_residual_eps();
      if (maxAbs > 0.0f) {
        const float relative = maxAbs * 1e-4f;
        if (residualEps < relative) {
          residualEps = relative;
        }
      }

      int32_t circNnz = 0;
      int32_t toeNnz = 0;
      for (int32_t r = 0; r < rows; ++r) {
        for (int32_t c = 0; c < cols; ++c) {
          const float actual = data[(size_t)r * (size_t)cols + (size_t)c];
          const int32_t circIdx = (c - r + cols) % cols;
          const float circExpected = data[circIdx];
          const float toeExpected =
              c >= r ? data[c - r] : data[(size_t)(r - c) * (size_t)cols];
          if (fabsf(actual - circExpected) > residualEps) {
            ++circNnz;
          }
          if (fabsf(actual - toeExpected) > residualEps) {
            ++toeNnz;
          }
        }
      }

      const float invSize = 1.0f / (float)(rows * cols);
      const float circDensity = (float)circNnz * invSize;
      const float toeDensity = (float)toeNnz * invSize;
      if (circDensity + 1e-6f < toeDensity) {
        chooseCirculant = 1;
      } else if (toeDensity + 1e-6f < circDensity) {
        chooseCirculant = 0;
      }
    }

    analysis.selectedKind = chooseCirculant ? kTensorStructuredCirculant
                                            : kTensorStructuredToeplitz;
    analysis.selectedScore =
        chooseCirculant ? analysis.circulantScore : analysis.toeplitzScore;
  } else {
    if (analysis.toeplitzScore > analysis.circulantScore) {
      analysis.selectedKind = kTensorStructuredToeplitz;
      analysis.selectedScore = analysis.toeplitzScore;
    } else {
      analysis.selectedKind = kTensorStructuredNone;
      analysis.selectedScore = analysis.circulantScore;
    }
  }

  if (analysis.selectedScore <= 0.0f) {
    analysis.selectedKind = kTensorStructuredNone;
  }
  return analysis;
}

static float tensor_structured_template_value(const float *matrix, int32_t n,
                                              TensorStructuredKind kind,
                                              int32_t row, int32_t col) {
  if (matrix == NULL || n <= 0) {
    return 0.0f;
  }
  if (kind == kTensorStructuredCirculant) {
    const int32_t idx = (col - row + n) % n;
    return matrix[idx];
  }
  if (kind == kTensorStructuredToeplitz) {
    if (col >= row) {
      return matrix[col - row];
    }
    return matrix[(size_t)(row - col) * (size_t)n];
  }
  return 0.0f;
}

static float tensor_structured_residual_eps_for_matrix(const float *matrix,
                                                       int32_t n) {
  if (matrix == NULL || n <= 0) {
    return tensor_structured_residual_eps();
  }

  float maxAbs = 0.0f;
  for (int32_t i = 0; i < n * n; ++i) {
    const float av = fabsf(matrix[i]);
    if (av > maxAbs) {
      maxAbs = av;
    }
  }

  float eps = tensor_structured_residual_eps();
  if (maxAbs > 0.0f) {
    const float relative = maxAbs * 1e-4f;
    if (eps < relative) {
      eps = relative;
    }
  }
  return eps;
}

static float tensor_estimate_residual_density(const float *matrix, int32_t n,
                                              TensorStructuredKind kind,
                                              float eps, int32_t *outNnz) {
  if (matrix == NULL || n <= 0 ||
      (kind != kTensorStructuredCirculant &&
       kind != kTensorStructuredToeplitz)) {
    if (outNnz != NULL) {
      *outNnz = 0;
    }
    return 1.0f;
  }

  int32_t nnz = 0;
  for (int32_t r = 0; r < n; ++r) {
    for (int32_t c = 0; c < n; ++c) {
      const float expected =
          tensor_structured_template_value(matrix, n, kind, r, c);
      const float residual =
          matrix[(size_t)r * (size_t)n + (size_t)c] - expected;
      if (fabsf(residual) > eps) {
        ++nnz;
      }
    }
  }

  if (outNnz != NULL) {
    *outNnz = nnz;
  }
  return (float)nnz / (float)(n * n);
}

static void tensor_clear_hybrid_residual(TensorPackedBCache *cache) {
  if (cache == NULL) {
    return;
  }
  neuron_tensor_packed_free(cache->hybridResidualPacked);
  neuron_dealloc(cache->hybridRowPtr);
  neuron_dealloc(cache->hybridColIdx);
  neuron_dealloc(cache->hybridValues);
  cache->hybridResidualPacked = NULL;
  cache->hybridUseDenseCorrection = 0;
  cache->hybridRowPtr = NULL;
  cache->hybridColIdx = NULL;
  cache->hybridValues = NULL;
  cache->hybridN = 0;
  cache->hybridNnz = 0;
  cache->hybridDensity = 0.0f;
  cache->hybridEnabled = 0;
  cache->hybridBaseKind = kTensorStructuredNone;
}

static int tensor_build_hybrid_dense_correction(TensorPackedBCache *cache,
                                                const float *matrix, int32_t n,
                                                TensorStructuredKind baseKind,
                                                float eps) {
  if (cache == NULL || matrix == NULL || n <= 1 ||
      (baseKind != kTensorStructuredCirculant &&
       baseKind != kTensorStructuredToeplitz)) {
    return 0;
  }

  float *denseResidual =
      (float *)tensor_aligned_alloc((size_t)n * (size_t)n * sizeof(float), 64u);
  if (denseResidual == NULL) {
    return 0;
  }

  for (int32_t r = 0; r < n; ++r) {
    for (int32_t c = 0; c < n; ++c) {
      const float expected =
          tensor_structured_template_value(matrix, n, baseKind, r, c);
      float residual = matrix[(size_t)r * (size_t)n + (size_t)c] - expected;
      if (fabsf(residual) <= eps) {
        residual = 0.0f;
      }
      denseResidual[(size_t)r * (size_t)n + (size_t)c] = residual;
    }
  }

  int32_t shape[2] = {n, n};
  NeuronTensor residualView;
  memset(&residualView, 0, sizeof(residualView));
  residualView.data = denseResidual;
  residualView.shape = shape;
  residualView.dimensions = 2;
  residualView.size = n * n;
  residualView.element_size = (int32_t)sizeof(float);
  residualView.data_type = NEURON_TENSOR_F32;

  NeuronPackedMatrix *packedResidual = NULL;
  const int packedOk =
      neuron_tensor_pack_b(&residualView, &packedResidual) == 0 &&
      packedResidual != NULL;
  tensor_aligned_free(denseResidual);
  if (!packedOk) {
    neuron_tensor_packed_free(packedResidual);
    return 0;
  }

  cache->hybridResidualPacked = packedResidual;
  cache->hybridUseDenseCorrection = 1;
  return 1;
}

static int tensor_build_hybrid_residual(TensorPackedBCache *cache,
                                        const float *matrix, int32_t n,
                                        TensorStructuredKind baseKind) {
  if (cache == NULL || matrix == NULL || n <= 1 ||
      (baseKind != kTensorStructuredCirculant &&
       baseKind != kTensorStructuredToeplitz)) {
    return 0;
  }

  tensor_clear_hybrid_residual(cache);

  const float eps = tensor_structured_residual_eps_for_matrix(matrix, n);
  int32_t nnz = 0;
  const float density =
      tensor_estimate_residual_density(matrix, n, baseKind, eps, &nnz);
  if (nnz <= 0) {
    return 0;
  }

  const float maxDensity = tensor_structured_hybrid_max_density();
  const float densityTolerance = 1e-4f;
  if ((density - maxDensity) > densityTolerance) {
    return 0;
  }

  cache->hybridN = n;
  cache->hybridNnz = nnz;
  cache->hybridDensity = density;
  cache->hybridBaseKind = baseKind;

  const float denseCorrectionDensity =
      tensor_structured_hybrid_dense_correction_density();
  const float denseFallbackDensity =
      tensor_structured_hybrid_dense_fallback_density();
  const int32_t denseCorrectionMaxN =
      tensor_structured_hybrid_dense_correction_max_n();
  if (density >= denseCorrectionDensity && density <= denseFallbackDensity &&
      n <= denseCorrectionMaxN) {
    if (tensor_build_hybrid_dense_correction(cache, matrix, n, baseKind, eps)) {
      cache->hybridEnabled = 1;
      if (tensor_structured_debug_enabled()) {
        fprintf(stderr,
                "[Neuron Tensor] Hybrid cache: %s dense correction selected "
                "(n=%d, nnz=%d, density=%.4f, eps=%.6f)\n",
                baseKind == kTensorStructuredToeplitz ? "toeplitz"
                                                      : "circulant",
                (int)n, (int)nnz, density, eps);
      }
      return 1;
    }
  }

  int32_t *rowPtr = (int32_t *)neuron_alloc((size_t)(n + 1) * sizeof(int32_t));
  int32_t *colIdx = (int32_t *)neuron_alloc((size_t)nnz * sizeof(int32_t));
  float *values = (float *)neuron_alloc((size_t)nnz * sizeof(float));
  if (rowPtr == NULL || colIdx == NULL || values == NULL) {
    neuron_dealloc(rowPtr);
    neuron_dealloc(colIdx);
    neuron_dealloc(values);
    return 0;
  }

  int32_t cursor = 0;
  rowPtr[0] = 0;
  for (int32_t r = 0; r < n; ++r) {
    for (int32_t c = 0; c < n; ++c) {
      const float expected =
          tensor_structured_template_value(matrix, n, baseKind, r, c);
      const float residual =
          matrix[(size_t)r * (size_t)n + (size_t)c] - expected;
      if (fabsf(residual) > eps) {
        colIdx[cursor] = c;
        values[cursor] = residual;
        ++cursor;
      }
    }
    rowPtr[r + 1] = cursor;
  }

  cache->hybridRowPtr = rowPtr;
  cache->hybridColIdx = colIdx;
  cache->hybridValues = values;
  cache->hybridNnz = cursor;
  cache->hybridEnabled = cursor > 0;
  if (cache->hybridEnabled && tensor_structured_debug_enabled()) {
    fprintf(stderr,
            "[Neuron Tensor] Hybrid cache: %s sparse correction selected "
            "(n=%d, nnz=%d, density=%.4f, eps=%.6f)\n",
            baseKind == kTensorStructuredToeplitz ? "toeplitz" : "circulant",
            (int)n, (int)cursor, density, eps);
  }
  return cache->hybridEnabled;
}

static TensorStructuredKind
tensor_select_hybrid_base_kind(const float *matrix, int32_t n,
                               const TensorStructureAnalysis *analysis) {
  if (matrix == NULL || analysis == NULL || n <= 1) {
    return kTensorStructuredNone;
  }

  const float eps = tensor_structured_residual_eps_for_matrix(matrix, n);
  const float tieWindow = 0.02f;
  const float scoreDiff =
      fabsf(analysis->circulantScore - analysis->toeplitzScore);

  int32_t circNnz = 0;
  int32_t toeNnz = 0;
  float circDensity = 1.0f;
  float toeDensity = 1.0f;

  if (tensor_is_power_of_two_i32(n)) {
    circDensity = tensor_estimate_residual_density(
        matrix, n, kTensorStructuredCirculant, eps, &circNnz);
  }
  toeDensity = tensor_estimate_residual_density(
      matrix, n, kTensorStructuredToeplitz, eps, &toeNnz);

  if (scoreDiff <= tieWindow) {
    if (circDensity + 1e-6f < toeDensity) {
      return kTensorStructuredCirculant;
    }
    if (toeDensity + 1e-6f < circDensity) {
      return kTensorStructuredToeplitz;
    }
  }

  if (analysis->selectedKind == kTensorStructuredCirculant &&
      tensor_is_power_of_two_i32(n)) {
    return kTensorStructuredCirculant;
  }
  if (analysis->selectedKind == kTensorStructuredToeplitz) {
    return kTensorStructuredToeplitz;
  }

  if (toeDensity <= circDensity || !tensor_is_power_of_two_i32(n)) {
    return kTensorStructuredToeplitz;
  }
  return kTensorStructuredCirculant;
}

static int tensor_prepare_circulant_spectrum(TensorPackedBCache *cache,
                                             const float *bData, int32_t n) {
  if (cache == NULL || bData == NULL || n <= 0 ||
      !tensor_is_power_of_two_i32(n)) {
    return 0;
  }

  if (cache->circulantSpectrum != NULL && cache->circulantFftSize == n) {
    return 1;
  }

  tensor_aligned_free(cache->circulantSpectrum);
  cache->circulantSpectrum = NULL;
  cache->circulantFftSize = 0;
  cache->circulantSpectrum = (TensorComplex *)tensor_aligned_alloc(
      (size_t)n * sizeof(TensorComplex), 64u);
  if (cache->circulantSpectrum == NULL) {
    return 0;
  }

  cache->circulantFftSize = n;
  for (int32_t i = 0; i < n; ++i) {
    cache->circulantSpectrum[i].re = bData[i];
    cache->circulantSpectrum[i].im = 0.0f;
  }
  if (!tensor_fft_inplace(cache->circulantSpectrum, n, 0)) {
    tensor_aligned_free(cache->circulantSpectrum);
    cache->circulantSpectrum = NULL;
    cache->circulantFftSize = 0;
    return 0;
  }
  return 1;
}

static int32_t tensor_next_power_of_two_i32(int32_t value) {
  if (value <= 1) {
    return 1;
  }
  uint32_t v = (uint32_t)(value - 1);
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return (int32_t)(v + 1u);
}

static int tensor_prepare_toeplitz_spectrum(TensorPackedBCache *cache,
                                            const float *bData, int32_t n) {
  if (cache == NULL || bData == NULL || n <= 1) {
    return 0;
  }

  const int32_t requiredFftSize = tensor_next_power_of_two_i32(2 * n);
  if (!tensor_is_power_of_two_i32(requiredFftSize)) {
    return 0;
  }

  if (cache->toeplitzSpectrum != NULL &&
      cache->toeplitzFftSize == requiredFftSize) {
    return 1;
  }

  tensor_aligned_free(cache->toeplitzSpectrum);
  cache->toeplitzSpectrum = NULL;
  cache->toeplitzFftSize = 0;
  cache->toeplitzSpectrum = (TensorComplex *)tensor_aligned_alloc(
      (size_t)requiredFftSize * sizeof(TensorComplex), 64u);
  if (cache->toeplitzSpectrum == NULL) {
    return 0;
  }

  // For y = x * B, use Toeplitz embedding of B^T:
  // v = [first_row(B), 0, reverse(first_col(B)[1:])]
  for (int32_t i = 0; i < requiredFftSize; ++i) {
    cache->toeplitzSpectrum[i].re = 0.0f;
    cache->toeplitzSpectrum[i].im = 0.0f;
  }

  for (int32_t i = 0; i < n; ++i) {
    cache->toeplitzSpectrum[i].re = bData[i];
  }
  for (int32_t i = 1; i < n; ++i) {
    const float colValue = bData[(size_t)i * (size_t)n];
    cache->toeplitzSpectrum[requiredFftSize - i].re = colValue;
  }

  cache->toeplitzFftSize = requiredFftSize;
  if (!tensor_fft_inplace(cache->toeplitzSpectrum, requiredFftSize, 0)) {
    tensor_aligned_free(cache->toeplitzSpectrum);
    cache->toeplitzSpectrum = NULL;
    cache->toeplitzFftSize = 0;
    return 0;
  }
  return 1;
}

static int tensor_matmul_circulant_fft_row(const float *aRow, float *outRow,
                                           int32_t n,
                                           const TensorComplex *spectrum,
                                           int accumulate) {
  TensorComplex *work = tensor_workspace_reserve_complex(
      &g_tensorWorkspace.fftWork, &g_tensorWorkspace.fftWorkCapacity,
      (size_t)n);
  if (work == NULL) {
    return 0;
  }

  for (int32_t i = 0; i < n; ++i) {
    work[i].re = aRow[i];
    work[i].im = 0.0f;
  }
  if (!tensor_fft_inplace(work, n, 0)) {
    return 0;
  }

  tensor_complex_pointwise_mul_inplace(work, spectrum, n);
  if (!tensor_fft_inplace(work, n, 1)) {
    return 0;
  }

  if (accumulate) {
    for (int32_t i = 0; i < n; ++i) {
      outRow[i] += work[i].re;
    }
  } else {
    for (int32_t i = 0; i < n; ++i) {
      outRow[i] = work[i].re;
    }
  }
  return 1;
}

static int tensor_matmul_fft_two_real_rows(const float *aRow0,
                                           const float *aRow1, float *outRow0,
                                           float *outRow1, int32_t inputN,
                                           int32_t fftN, int32_t outputN,
                                           const TensorComplex *spectrum,
                                           int accumulate) {
  if (aRow0 == NULL || aRow1 == NULL || outRow0 == NULL || outRow1 == NULL ||
      spectrum == NULL || inputN <= 0 || fftN <= 0 || outputN <= 0 ||
      inputN > fftN || outputN > fftN || !tensor_is_power_of_two_i32(fftN)) {
    return 0;
  }

  TensorComplex *work = tensor_workspace_reserve_complex(
      &g_tensorWorkspace.fftWork, &g_tensorWorkspace.fftWorkCapacity,
      (size_t)fftN);
  TensorComplex *aux = tensor_workspace_reserve_complex(
      &g_tensorWorkspace.fftAux, &g_tensorWorkspace.fftAuxCapacity,
      (size_t)fftN);
  if (work == NULL || aux == NULL) {
    return 0;
  }

  for (int32_t i = 0; i < inputN; ++i) {
    work[i].re = aRow0[i];
    work[i].im = aRow1[i];
  }
  for (int32_t i = inputN; i < fftN; ++i) {
    work[i].re = 0.0f;
    work[i].im = 0.0f;
  }

  if (!tensor_fft_inplace(work, fftN, 0)) {
    return 0;
  }

  for (int32_t k = 0; k < fftN; ++k) {
    const int32_t nk = (fftN - k) & (fftN - 1);
    const TensorComplex a = work[k];
    TensorComplex b = work[nk];
    b.im = -b.im;

    TensorComplex x;
    x.re = 0.5f * (a.re + b.re);
    x.im = 0.5f * (a.im + b.im);
    TensorComplex y;
    y.re = 0.5f * (a.im - b.im);
    y.im = 0.5f * (b.re - a.re);

    const TensorComplex cx = tensor_complex_mul(x, spectrum[k]);
    const TensorComplex cy = tensor_complex_mul(y, spectrum[k]);
    aux[k].re = cx.re - cy.im;
    aux[k].im = cx.im + cy.re;
  }

  if (!tensor_fft_inplace(aux, fftN, 1)) {
    return 0;
  }

  if (accumulate) {
    for (int32_t i = 0; i < outputN; ++i) {
      outRow0[i] += aux[i].re;
      outRow1[i] += aux[i].im;
    }
  } else {
    for (int32_t i = 0; i < outputN; ++i) {
      outRow0[i] = aux[i].re;
      outRow1[i] = aux[i].im;
    }
  }

  return 1;
}

static int tensor_matmul_try_circulant_fft(const float *aData, float *cData,
                                           int32_t m, int32_t n,
                                           const TensorComplex *spectrum,
                                           int accumulate) {
  if (aData == NULL || cData == NULL || spectrum == NULL || m <= 0 || n <= 0 ||
      !tensor_is_power_of_two_i32(n)) {
    return 0;
  }

  int failed = 0;
  const int64_t fftWorkload =
      (int64_t)m * (int64_t)n * (int64_t)tensor_log2_floor_i32(n);
  const int threads = tensor_recommended_threads(fftWorkload, m, n, n);
  const int32_t pairCount = (m + 1) / 2;
#if defined(_OPENMP)
#pragma omp parallel for schedule(static) num_threads(threads)                 \
    reduction(| : failed)
#endif
  for (int32_t pair = 0; pair < pairCount; ++pair) {
    const int32_t row0 = pair * 2;
    const int32_t row1 = row0 + 1;
    const float *aRow0 = aData + (size_t)row0 * (size_t)n;
    float *outRow0 = cData + (size_t)row0 * (size_t)n;
    if (row1 < m) {
      const float *aRow1 = aData + (size_t)row1 * (size_t)n;
      float *outRow1 = cData + (size_t)row1 * (size_t)n;
      if (!tensor_matmul_fft_two_real_rows(aRow0, aRow1, outRow0, outRow1, n, n,
                                           n, spectrum, accumulate)) {
        failed = 1;
      }
      continue;
    }
    if (!tensor_matmul_circulant_fft_row(aRow0, outRow0, n, spectrum,
                                         accumulate)) {
      failed = 1;
    }
  }
  return failed ? 0 : 1;
}

static int tensor_matmul_toeplitz_fft_row(const float *aRow, float *outRow,
                                          int32_t n,
                                          const TensorComplex *spectrum,
                                          int32_t fftSize, int accumulate) {
  TensorComplex *work = tensor_workspace_reserve_complex(
      &g_tensorWorkspace.fftWork, &g_tensorWorkspace.fftWorkCapacity,
      (size_t)fftSize);
  if (work == NULL) {
    return 0;
  }

  for (int32_t i = 0; i < n; ++i) {
    work[i].re = aRow[i];
    work[i].im = 0.0f;
  }
  for (int32_t i = n; i < fftSize; ++i) {
    work[i].re = 0.0f;
    work[i].im = 0.0f;
  }
  if (!tensor_fft_inplace(work, fftSize, 0)) {
    return 0;
  }

  tensor_complex_pointwise_mul_inplace(work, spectrum, fftSize);
  if (!tensor_fft_inplace(work, fftSize, 1)) {
    return 0;
  }

  if (accumulate) {
    for (int32_t i = 0; i < n; ++i) {
      outRow[i] += work[i].re;
    }
  } else {
    for (int32_t i = 0; i < n; ++i) {
      outRow[i] = work[i].re;
    }
  }
  return 1;
}

static int tensor_matmul_try_toeplitz_fft(const float *aData, float *cData,
                                          int32_t m, int32_t n,
                                          const TensorComplex *spectrum,
                                          int32_t fftSize, int accumulate) {
  if (aData == NULL || cData == NULL || spectrum == NULL || m <= 0 || n <= 1 ||
      fftSize < (2 * n) || !tensor_is_power_of_two_i32(fftSize)) {
    return 0;
  }

  int failed = 0;
  const int64_t fftWorkload =
      (int64_t)m * (int64_t)fftSize * (int64_t)tensor_log2_floor_i32(fftSize);
  const int threads = tensor_recommended_threads(fftWorkload, m, n, n);
  const int32_t pairCount = (m + 1) / 2;
#if defined(_OPENMP)
#pragma omp parallel for schedule(static) num_threads(threads)                 \
    reduction(| : failed)
#endif
  for (int32_t pair = 0; pair < pairCount; ++pair) {
    const int32_t row0 = pair * 2;
    const int32_t row1 = row0 + 1;
    const float *aRow0 = aData + (size_t)row0 * (size_t)n;
    float *outRow0 = cData + (size_t)row0 * (size_t)n;
    if (row1 < m) {
      const float *aRow1 = aData + (size_t)row1 * (size_t)n;
      float *outRow1 = cData + (size_t)row1 * (size_t)n;
      if (!tensor_matmul_fft_two_real_rows(aRow0, aRow1, outRow0, outRow1, n,
                                           fftSize, n, spectrum, accumulate)) {
        failed = 1;
      }
      continue;
    }
    if (!tensor_matmul_toeplitz_fft_row(aRow0, outRow0, n, spectrum, fftSize,
                                        accumulate)) {
      failed = 1;
    }
  }

  return failed ? 0 : 1;
}

static void tensor_apply_sparse_residual(const float *aData, float *cData,
                                         int32_t m, int32_t n,
                                         const int32_t *rowPtr,
                                         const int32_t *colIdx,
                                         const float *values, int32_t nnzHint) {
  if (aData == NULL || cData == NULL || m <= 0 || n <= 0 || rowPtr == NULL ||
      colIdx == NULL || values == NULL) {
    return;
  }

#if defined(_OPENMP)
  const int32_t nnz = nnzHint > 0 ? nnzHint : rowPtr[n];
  const int64_t sparseWorkload = (int64_t)m * (int64_t)(nnz > 0 ? nnz : n);
  int threads = omp_get_max_threads();
  const int requested = tensor_requested_threads();
  if (requested > 0 && requested < threads) {
    threads = requested;
  }
  if (threads > 1) {
    if (sparseWorkload < 4LL * 1024LL * 1024LL || nnz < 2048 || m < 64) {
      threads = 1;
    } else if (sparseWorkload < 16LL * 1024LL * 1024LL) {
      threads = tensor_min_i32(threads, 4);
    }
  }
  if (threads > 1) {
#pragma omp parallel num_threads(threads)
    {
      tensor_pin_current_thread(omp_get_thread_num());
#pragma omp for schedule(static)
      for (int32_t i = 0; i < m; ++i) {
        const float *aRow = aData + (size_t)i * (size_t)n;
        float *cRow = cData + (size_t)i * (size_t)n;
        for (int32_t src = 0; src < n; ++src) {
          const float aVal = aRow[src];
          if (aVal == 0.0f) {
            continue;
          }
          const int32_t start = rowPtr[src];
          const int32_t end = rowPtr[src + 1];
          int32_t p = start;
          for (; p + 8 <= end; p += 8) {
            const int32_t c0 = colIdx[p + 0];
            const int32_t c1 = colIdx[p + 1];
            const int32_t c2 = colIdx[p + 2];
            const int32_t c3 = colIdx[p + 3];
            const int32_t c4 = colIdx[p + 4];
            const int32_t c5 = colIdx[p + 5];
            const int32_t c6 = colIdx[p + 6];
            const int32_t c7 = colIdx[p + 7];
            cRow[c0] += aVal * values[p + 0];
            cRow[c1] += aVal * values[p + 1];
            cRow[c2] += aVal * values[p + 2];
            cRow[c3] += aVal * values[p + 3];
            cRow[c4] += aVal * values[p + 4];
            cRow[c5] += aVal * values[p + 5];
            cRow[c6] += aVal * values[p + 6];
            cRow[c7] += aVal * values[p + 7];
          }
          for (; p + 4 <= end; p += 4) {
            const int32_t c0 = colIdx[p + 0];
            const int32_t c1 = colIdx[p + 1];
            const int32_t c2 = colIdx[p + 2];
            const int32_t c3 = colIdx[p + 3];
            cRow[c0] += aVal * values[p + 0];
            cRow[c1] += aVal * values[p + 1];
            cRow[c2] += aVal * values[p + 2];
            cRow[c3] += aVal * values[p + 3];
          }
          for (; p < end; ++p) {
            cRow[colIdx[p]] += aVal * values[p];
          }
        }
      }
    }
    return;
  }
#endif

  for (int32_t i = 0; i < m; ++i) {
    const float *aRow = aData + (size_t)i * (size_t)n;
    float *cRow = cData + (size_t)i * (size_t)n;
    for (int32_t src = 0; src < n; ++src) {
      const float aVal = aRow[src];
      if (aVal == 0.0f) {
        continue;
      }
      const int32_t start = rowPtr[src];
      const int32_t end = rowPtr[src + 1];
      int32_t p = start;
      for (; p + 8 <= end; p += 8) {
        const int32_t c0 = colIdx[p + 0];
        const int32_t c1 = colIdx[p + 1];
        const int32_t c2 = colIdx[p + 2];
        const int32_t c3 = colIdx[p + 3];
        const int32_t c4 = colIdx[p + 4];
        const int32_t c5 = colIdx[p + 5];
        const int32_t c6 = colIdx[p + 6];
        const int32_t c7 = colIdx[p + 7];
        cRow[c0] += aVal * values[p + 0];
        cRow[c1] += aVal * values[p + 1];
        cRow[c2] += aVal * values[p + 2];
        cRow[c3] += aVal * values[p + 3];
        cRow[c4] += aVal * values[p + 4];
        cRow[c5] += aVal * values[p + 5];
        cRow[c6] += aVal * values[p + 6];
        cRow[c7] += aVal * values[p + 7];
      }
      for (; p + 4 <= end; p += 4) {
        const int32_t c0 = colIdx[p + 0];
        const int32_t c1 = colIdx[p + 1];
        const int32_t c2 = colIdx[p + 2];
        const int32_t c3 = colIdx[p + 3];
        cRow[c0] += aVal * values[p + 0];
        cRow[c1] += aVal * values[p + 1];
        cRow[c2] += aVal * values[p + 2];
        cRow[c3] += aVal * values[p + 3];
      }
      for (; p < end; ++p) {
        cRow[colIdx[p]] += aVal * values[p];
      }
    }
  }
}

static int tensor_matmul_try_batched_skinny(const float *aData,
                                            const float *bData, float *cData,
                                            int32_t m, int32_t n, int32_t k) {
  if (m <= 1 || n <= 1 || n > 8 || k < 32) {
    return 0;
  }

  memset(cData, 0, (size_t)m * (size_t)n * sizeof(float));

#if defined(__AVX2__) && defined(__FMA__)
  if (n == 8) {
    int32_t i = 0;
    for (; i + 4 <= m; i += 4) {
      __m256 c0 = _mm256_setzero_ps();
      __m256 c1 = _mm256_setzero_ps();
      __m256 c2 = _mm256_setzero_ps();
      __m256 c3 = _mm256_setzero_ps();
      for (int32_t p = 0; p < k; ++p) {
        __m256 vb = _mm256_loadu_ps(bData + (size_t)p * 8u);
        __m256 va0 =
            _mm256_set1_ps(aData[(size_t)(i + 0) * (size_t)k + (size_t)p]);
        __m256 va1 =
            _mm256_set1_ps(aData[(size_t)(i + 1) * (size_t)k + (size_t)p]);
        __m256 va2 =
            _mm256_set1_ps(aData[(size_t)(i + 2) * (size_t)k + (size_t)p]);
        __m256 va3 =
            _mm256_set1_ps(aData[(size_t)(i + 3) * (size_t)k + (size_t)p]);
        c0 = _mm256_fmadd_ps(va0, vb, c0);
        c1 = _mm256_fmadd_ps(va1, vb, c1);
        c2 = _mm256_fmadd_ps(va2, vb, c2);
        c3 = _mm256_fmadd_ps(va3, vb, c3);
      }
      _mm256_storeu_ps(cData + (size_t)(i + 0) * 8u, c0);
      _mm256_storeu_ps(cData + (size_t)(i + 1) * 8u, c1);
      _mm256_storeu_ps(cData + (size_t)(i + 2) * 8u, c2);
      _mm256_storeu_ps(cData + (size_t)(i + 3) * 8u, c3);
    }

    for (; i < m; ++i) {
      __m256 c0 = _mm256_setzero_ps();
      for (int32_t p = 0; p < k; ++p) {
        __m256 vb = _mm256_loadu_ps(bData + (size_t)p * 8u);
        __m256 va = _mm256_set1_ps(aData[(size_t)i * (size_t)k + (size_t)p]);
        c0 = _mm256_fmadd_ps(va, vb, c0);
      }
      _mm256_storeu_ps(cData + (size_t)i * 8u, c0);
    }
    return 1;
  }
#endif

  for (int32_t i = 0; i < m; i += 4) {
    const int32_t rows = tensor_min_i32(4, m - i);
    float acc0[8] = {0};
    float acc1[8] = {0};
    float acc2[8] = {0};
    float acc3[8] = {0};

    for (int32_t p = 0; p < k; ++p) {
      const float *bRow = bData + (size_t)p * (size_t)n;
      const float a0 = aData[(size_t)(i + 0) * (size_t)k + (size_t)p];
      const float a1 =
          rows > 1 ? aData[(size_t)(i + 1) * (size_t)k + (size_t)p] : 0.0f;
      const float a2 =
          rows > 2 ? aData[(size_t)(i + 2) * (size_t)k + (size_t)p] : 0.0f;
      const float a3 =
          rows > 3 ? aData[(size_t)(i + 3) * (size_t)k + (size_t)p] : 0.0f;
      for (int32_t j = 0; j < n; ++j) {
        const float bv = bRow[j];
        acc0[j] += a0 * bv;
        if (rows > 1)
          acc1[j] += a1 * bv;
        if (rows > 2)
          acc2[j] += a2 * bv;
        if (rows > 3)
          acc3[j] += a3 * bv;
      }
    }

    memcpy(cData + (size_t)(i + 0) * (size_t)n, acc0,
           (size_t)n * sizeof(float));
    if (rows > 1) {
      memcpy(cData + (size_t)(i + 1) * (size_t)n, acc1,
             (size_t)n * sizeof(float));
    }
    if (rows > 2) {
      memcpy(cData + (size_t)(i + 2) * (size_t)n, acc2,
             (size_t)n * sizeof(float));
    }
    if (rows > 3) {
      memcpy(cData + (size_t)(i + 3) * (size_t)n, acc3,
             (size_t)n * sizeof(float));
    }
  }

  return 1;
}

static int tensor_matmul_try_small_or_skinny(const float *aData,
                                             const float *bData, float *cData,
                                             int32_t m, int32_t n, int32_t k) {
  if (m <= 0 || n <= 0 || k <= 0) {
    return 0;
  }

  // Row-vector (1 x k) @ (k x n)
  if (m == 1) {
    memset(cData, 0, (size_t)n * sizeof(float));
    const float *aRow = aData;
    for (int32_t p = 0; p < k; ++p) {
      const float aVal = aRow[p];
      const float *bRow = bData + (size_t)p * (size_t)n;
      int32_t j = 0;
#if defined(__AVX2__)
      const __m256 vA = _mm256_set1_ps(aVal);
      for (; j + 8 <= n; j += 8) {
        __m256 vOut = _mm256_loadu_ps(cData + j);
        __m256 vB = _mm256_loadu_ps(bRow + j);
#if defined(__FMA__)
        vOut = _mm256_fmadd_ps(vA, vB, vOut);
#else
        vOut = _mm256_add_ps(vOut, _mm256_mul_ps(vA, vB));
#endif
        _mm256_storeu_ps(cData + j, vOut);
      }
#endif
#if defined(__SSE__)
      const __m128 vA4 = _mm_set1_ps(aVal);
      for (; j + 4 <= n; j += 4) {
        __m128 vOut = _mm_loadu_ps(cData + j);
        __m128 vB = _mm_loadu_ps(bRow + j);
        vOut = _mm_add_ps(vOut, _mm_mul_ps(vA4, vB));
        _mm_storeu_ps(cData + j, vOut);
      }
#endif
      for (; j < n; ++j) {
        cData[j] += aVal * bRow[j];
      }
    }
    return 1;
  }

  // Matrix-vector (m x k) @ (k x 1)
  if (n == 1) {
    for (int32_t i = 0; i < m; ++i) {
      const float *aRow = aData + (size_t)i * (size_t)k;
      cData[i] = tensor_dot_product(aRow, bData, k);
    }
    return 1;
  }

  // Skinny dimensions avoid heavy packing overhead.
  if (m <= 16 || n <= 16 || k <= 16) {
    for (int32_t i = 0; i < m; ++i) {
      const float *aRow = aData + (size_t)i * (size_t)k;
      float *cRow = cData + (size_t)i * (size_t)n;
      for (int32_t j = 0; j < n; ++j) {
        float sum = 0.0f;
        for (int32_t p = 0; p < k; ++p) {
          sum += aRow[p] * bData[(size_t)p * (size_t)n + (size_t)j];
        }
        cRow[j] = sum;
      }
    }
    return 1;
  }

  return 0;
}

static TensorEpilogueConfig tensor_build_epilogue_config(
    int enabled, const float *biasData, int32_t biasRows, int32_t biasCols,
    const float *residualData, int32_t residualCols, int32_t activation) {
  TensorEpilogueConfig cfg;
  memset(&cfg, 0, sizeof(cfg));
  if (!enabled) {
    return cfg;
  }
  cfg.enabled = 1;
  cfg.biasData = biasData;
  cfg.biasRows = biasRows;
  cfg.biasCols = biasCols;
  cfg.residualData = residualData;
  cfg.residualCols = residualCols;
  cfg.activation = activation;
  return cfg;
}

static float tensor_apply_activation_scalar(float value, int32_t activation) {
  if (activation == NEURON_TENSOR_ACTIVATION_RELU) {
    return value < 0.0f ? 0.0f : value;
  }
  if (activation == NEURON_TENSOR_ACTIVATION_GELU) {
    const float c = 0.044715f;
    const float scale = 0.7978845608f;
    const float cubic = value * value * value;
    return 0.5f * value * (1.0f + tanhf(scale * (value + c * cubic)));
  }
  return value;
}

static void tensor_apply_epilogue_block(
    float *cBlock, int32_t ldc, int32_t mc, int32_t nc, int32_t mc0,
    int32_t nc0, const float *biasData, int32_t biasRows, int32_t biasCols,
    const float *residualData, int32_t residualCols, int32_t activation) {
  if (cBlock == NULL || ldc <= 0 || mc <= 0 || nc <= 0) {
    return;
  }

  for (int32_t i = 0; i < mc; ++i) {
    float *cRow = cBlock + (size_t)i * (size_t)ldc;
    const float *residualRow =
        residualData != NULL
            ? residualData + (size_t)(mc0 + i) * (size_t)residualCols +
                  (size_t)nc0
            : NULL;
    const float *biasRow = NULL;
    if (biasData != NULL) {
      if (biasRows == 1) {
        biasRow = biasData + (size_t)nc0;
      } else {
        biasRow = biasData + (size_t)(mc0 + i) * (size_t)biasCols + (size_t)nc0;
      }
    }

    for (int32_t j = 0; j < nc; ++j) {
      float value = cRow[j];
      if (biasRow != NULL) {
        value += biasRow[j];
      }
      if (residualRow != NULL) {
        value += residualRow[j];
      }
      cRow[j] = tensor_apply_activation_scalar(value, activation);
    }
  }
}

static void tensor_apply_epilogue_full(float *cData, int32_t m, int32_t n,
                                       const TensorEpilogueConfig *epilogue) {
  if (cData == NULL || epilogue == NULL || !epilogue->enabled || m <= 0 ||
      n <= 0) {
    return;
  }

  for (int32_t r = 0; r < m; ++r) {
    float *outRow = cData + (size_t)r * (size_t)n;
    const float *biasRow = NULL;
    const float *residualRow = NULL;

    if (epilogue->biasData != NULL) {
      if (epilogue->biasRows == 1) {
        biasRow = epilogue->biasData;
      } else {
        biasRow = epilogue->biasData + (size_t)r * (size_t)epilogue->biasCols;
      }
    }

    if (epilogue->residualData != NULL) {
      residualRow =
          epilogue->residualData + (size_t)r * (size_t)epilogue->residualCols;
    }

    for (int32_t c = 0; c < n; ++c) {
      float value = outRow[c];
      if (biasRow != NULL) {
        value += biasRow[c];
      }
      if (residualRow != NULL) {
        value += residualRow[c];
      }
      outRow[c] = tensor_apply_activation_scalar(value, epilogue->activation);
    }
  }
}

static int tensor_packed_matches(const NeuronPackedMatrix *packed, int32_t rows,
                                 int32_t cols) {
  return packed != NULL && packed->rows == rows && packed->cols == cols &&
         packed->kc == TENSOR_KC && packed->nc == TENSOR_NC &&
         packed->offsets != NULL && packed->data != NULL &&
         packed->panelCount > 0;
}

static const float *tensor_packed_panel(const NeuronPackedMatrix *packed,
                                        int32_t ncBlockIndex,
                                        int32_t kcBlockIndex) {
  if (packed == NULL || ncBlockIndex < 0 || kcBlockIndex < 0 ||
      ncBlockIndex >= packed->nBlocks || kcBlockIndex >= packed->kBlocks) {
    return NULL;
  }
  const size_t panelIndex =
      (size_t)ncBlockIndex * (size_t)packed->kBlocks + (size_t)kcBlockIndex;
  if (panelIndex >= packed->panelCount) {
    return NULL;
  }
  return packed->data + packed->offsets[panelIndex];
}

static void
tensor_matmul_blocked_packed(const float *aData, const float *bData,
                             float *cData, int32_t m, int32_t n, int32_t k,
                             const TensorMatMulKernelOptions *options) {
  const int64_t workload = (int64_t)m * (int64_t)n * (int64_t)k;
  const TensorProfile profile = tensor_runtime_profile();
  const TensorKernelVariant kernelVariant =
      tensor_choose_kernel_variant(m, n, k, profile);
  const int threads = tensor_recommended_threads(workload, m, n, k);
  const int useAVX2 = tensor_cpu_supports_avx2_fma();
  const int accumulateInitial = options != NULL && options->accumulate;
  const NeuronPackedMatrix *packedB =
      (options != NULL) ? options->packedB : NULL;
  NeuronPackedMatrix *ownedPackedB = NULL;
  int hasPackedB = tensor_packed_matches(packedB, k, n);
  const NeuronTensor *bias = (options != NULL) ? options->bias : NULL;
  const NeuronTensor *residual = (options != NULL) ? options->residual : NULL;
  const int32_t activation =
      (options != NULL) ? options->activation : NEURON_TENSOR_ACTIVATION_NONE;

  const int activationFused = activation == NEURON_TENSOR_ACTIVATION_RELU;
  const int activationSupported =
      activation == NEURON_TENSOR_ACTIVATION_NONE || activationFused;

  const int biasSupported = bias != NULL && tensor_is_valid(bias) &&
                            bias->dimensions == 2 && bias->shape[1] == n &&
                            (bias->shape[0] == 1 || bias->shape[0] == m);
  const int residualSupported = residual != NULL && tensor_is_valid(residual) &&
                                residual->dimensions == 2 &&
                                residual->shape[0] == m &&
                                residual->shape[1] == n;

  const int applyFusedEpilogue =
      activationSupported &&
      (activationFused || biasSupported || residualSupported);
  const float *biasData = biasSupported ? bias->data : NULL;
  const int32_t biasRows = biasSupported ? bias->shape[0] : 0;
  const int32_t biasCols = biasSupported ? bias->shape[1] : 0;
  const float *residualData = residualSupported ? residual->data : NULL;
  const int32_t residualCols = residualSupported ? residual->shape[1] : 0;

  if (!hasPackedB && bData != NULL) {
    int32_t bShape[2] = {k, n};
    NeuronTensor bView;
    memset(&bView, 0, sizeof(bView));
    bView.data = (float *)bData;
    bView.shape = bShape;
    bView.dimensions = 2;
    bView.size = k * n;
    bView.element_size = (int32_t)sizeof(float);
    bView.data_type = NEURON_TENSOR_F32;

    if (neuron_tensor_pack_b(&bView, &ownedPackedB) == 0 &&
        tensor_packed_matches(ownedPackedB, k, n)) {
      packedB = ownedPackedB;
      hasPackedB = 1;
    }
  }

#if defined(_OPENMP)
  if (threads > 1) {
    const int32_t ncBlockCount = (n + TENSOR_NC - 1) / TENSOR_NC;
    const int32_t mcBlockCount = (m + TENSOR_MC - 1) / TENSOR_MC;
    const int32_t tileCount = ncBlockCount * mcBlockCount;
#pragma omp parallel num_threads(threads)
    {
      tensor_pin_current_thread(omp_get_thread_num());
      float *aPackThread = tensor_workspace_reserve(
          &g_tensorWorkspace.aPack, &g_tensorWorkspace.aPackCapacity,
          (size_t)TENSOR_MC * (size_t)TENSOR_KC);
      float *bPackThread =
          hasPackedB
              ? NULL
              : tensor_workspace_reserve(&g_tensorWorkspace.bPack,
                                         &g_tensorWorkspace.bPackCapacity,
                                         (size_t)TENSOR_KC * (size_t)TENSOR_NC);

#pragma omp for schedule(static)
      for (int32_t tileIdx = 0; tileIdx < tileCount; ++tileIdx) {
        const int32_t ncBlockIndex = tileIdx / mcBlockCount;
        const int32_t mcBlockIndex = tileIdx % mcBlockCount;
        const int32_t nc0 = ncBlockIndex * TENSOR_NC;
        const int32_t mc0 = mcBlockIndex * TENSOR_MC;
        const int32_t nc = tensor_min_i32(TENSOR_NC, n - nc0);
        const int32_t mc = tensor_min_i32(TENSOR_MC, m - mc0);
        float *cBlock = cData + (size_t)mc0 * (size_t)n + (size_t)nc0;

        for (int32_t kc0 = 0; kc0 < k; kc0 += TENSOR_KC) {
          const int32_t kc = tensor_min_i32(TENSOR_KC, k - kc0);
          const int32_t kcBlockIndex = kc0 / TENSOR_KC;
          const int accumulate = accumulateInitial || kc0 > 0;
          const int applyEpilogueNow = applyFusedEpilogue && (kc0 + kc >= k);
          const TensorEpilogueConfig epilogueConfig =
              tensor_build_epilogue_config(applyEpilogueNow, biasData, biasRows,
                                           biasCols, residualData, residualCols,
                                           activationFused
                                               ? NEURON_TENSOR_ACTIVATION_RELU
                                               : NEURON_TENSOR_ACTIVATION_NONE);
          const float *bPanel = NULL;

          if (hasPackedB) {
            bPanel = tensor_packed_panel(packedB, ncBlockIndex, kcBlockIndex);
          } else if (bPackThread != NULL) {
            tensor_pack_b_panel(bData + (size_t)kc0 * (size_t)n + (size_t)nc0,
                                n, bPackThread, kc, nc);
            bPanel = bPackThread;
          }

          if (aPackThread != NULL && bPanel != NULL) {
            tensor_pack_a_panel(aData + (size_t)mc0 * (size_t)k + (size_t)kc0,
                                k, aPackThread, mc, kc);
            const int epilogueApplied = tensor_compute_mc_block(
                aPackThread, bPanel, cBlock, n, kc, nc, mc, useAVX2, accumulate,
                kernelVariant, &epilogueConfig, mc0, nc0);
            if (applyEpilogueNow && !epilogueApplied) {
              tensor_apply_epilogue_block(
                  cBlock, n, mc, nc, mc0, nc0, biasData, biasRows, biasCols,
                  residualData, residualCols,
                  activationFused ? NEURON_TENSOR_ACTIVATION_RELU
                                  : NEURON_TENSOR_ACTIVATION_NONE);
            }
            continue;
          }

          if (bData == NULL) {
            continue;
          }

          for (int32_t i = 0; i < mc; ++i) {
            float *cRow = cBlock + (size_t)i * (size_t)n;
            const float *aRow =
                aData + (size_t)(mc0 + i) * (size_t)k + (size_t)kc0;
            for (int32_t j = 0; j < nc; ++j) {
              float sum = accumulate ? cRow[j] : 0.0f;
              for (int32_t p = 0; p < kc; ++p) {
                const float *bRow =
                    bData + (size_t)(kc0 + p) * (size_t)n + (size_t)nc0;
                sum += aRow[p] * bRow[j];
              }
              cRow[j] = sum;
            }
          }

          if (applyEpilogueNow) {
            tensor_apply_epilogue_block(
                cBlock, n, mc, nc, mc0, nc0, biasData, biasRows, biasCols,
                residualData, residualCols,
                activationFused ? NEURON_TENSOR_ACTIVATION_RELU
                                : NEURON_TENSOR_ACTIVATION_NONE);
          }
        }
      }
    }
    goto cleanup;
  }
#endif

  float *aPack = tensor_workspace_reserve(
      &g_tensorWorkspace.aPack, &g_tensorWorkspace.aPackCapacity,
      (size_t)TENSOR_MC * (size_t)TENSOR_KC);
  float *bPack =
      hasPackedB
          ? NULL
          : tensor_workspace_reserve(&g_tensorWorkspace.bPack,
                                     &g_tensorWorkspace.bPackCapacity,
                                     (size_t)TENSOR_KC * (size_t)TENSOR_NC);

  for (int32_t nc0 = 0; nc0 < n; nc0 += TENSOR_NC) {
    const int32_t nc = tensor_min_i32(TENSOR_NC, n - nc0);
    const int32_t ncBlockIndex = nc0 / TENSOR_NC;

    for (int32_t kc0 = 0; kc0 < k; kc0 += TENSOR_KC) {
      const int32_t kc = tensor_min_i32(TENSOR_KC, k - kc0);
      const int32_t kcBlockIndex = kc0 / TENSOR_KC;
      const int accumulate = accumulateInitial || kc0 > 0;
      const int applyEpilogueNow = applyFusedEpilogue && (kc0 + kc >= k);
      const TensorEpilogueConfig epilogueConfig = tensor_build_epilogue_config(
          applyEpilogueNow, biasData, biasRows, biasCols, residualData,
          residualCols,
          activationFused ? NEURON_TENSOR_ACTIVATION_RELU
                          : NEURON_TENSOR_ACTIVATION_NONE);
      const float *bPanel = NULL;

      if (hasPackedB) {
        bPanel = tensor_packed_panel(packedB, ncBlockIndex, kcBlockIndex);
      } else if (bPack != NULL) {
        tensor_pack_b_panel(bData + (size_t)kc0 * (size_t)n + (size_t)nc0, n,
                            bPack, kc, nc);
        bPanel = bPack;
      }

      for (int32_t mc0 = 0; mc0 < m; mc0 += TENSOR_MC) {
        const int32_t mc = tensor_min_i32(TENSOR_MC, m - mc0);
        float *cBlock = cData + (size_t)mc0 * (size_t)n + (size_t)nc0;

        if (aPack != NULL && bPanel != NULL) {
          tensor_pack_a_panel(aData + (size_t)mc0 * (size_t)k + (size_t)kc0, k,
                              aPack, mc, kc);
          const int epilogueApplied = tensor_compute_mc_block(
              aPack, bPanel, cBlock, n, kc, nc, mc, useAVX2, accumulate,
              kernelVariant, &epilogueConfig, mc0, nc0);
          if (applyEpilogueNow && !epilogueApplied) {
            tensor_apply_epilogue_block(
                cBlock, n, mc, nc, mc0, nc0, biasData, biasRows, biasCols,
                residualData, residualCols,
                activationFused ? NEURON_TENSOR_ACTIVATION_RELU
                                : NEURON_TENSOR_ACTIVATION_NONE);
          }
          continue;
        }

        if (bData == NULL) {
          continue;
        }

        for (int32_t i = 0; i < mc; ++i) {
          float *cRow = cBlock + (size_t)i * (size_t)n;
          const float *aRow =
              aData + (size_t)(mc0 + i) * (size_t)k + (size_t)kc0;
          for (int32_t j = 0; j < nc; ++j) {
            float sum = accumulate ? cRow[j] : 0.0f;
            for (int32_t p = 0; p < kc; ++p) {
              const float *bRow =
                  bData + (size_t)(kc0 + p) * (size_t)n + (size_t)nc0;
              sum += aRow[p] * bRow[j];
            }
            cRow[j] = sum;
          }
        }

        if (applyEpilogueNow) {
          tensor_apply_epilogue_block(
              cBlock, n, mc, nc, mc0, nc0, biasData, biasRows, biasCols,
              residualData, residualCols,
              activationFused ? NEURON_TENSOR_ACTIVATION_RELU
                              : NEURON_TENSOR_ACTIVATION_NONE);
        }
      }
    }
  }

cleanup:
  neuron_tensor_packed_free(ownedPackedB);
}

static int tensor_is_valid(const NeuronTensor *t) {
  return t != NULL && t->data != NULL && t->shape != NULL &&
         t->dimensions > 0 && t->size >= 0;
}

static int tensor_same_shape(const NeuronTensor *a, const NeuronTensor *b) {
  if (!tensor_is_valid(a) || !tensor_is_valid(b)) {
    return 0;
  }
  if (a->dimensions != b->dimensions || a->size != b->size) {
    return 0;
  }
  for (int32_t i = 0; i < a->dimensions; i++) {
    if (a->shape[i] != b->shape[i]) {
      return 0;
    }
  }
  return 1;
}

static NeuronTensor *tensor_create_like(const NeuronTensor *ref) {
  if (!tensor_is_valid(ref)) {
    return NULL;
  }
  return neuron_tensor_create(ref->dimensions, ref->shape);
}

static void tensor_fill_random(NeuronTensor *tensor) {
  if (!tensor_is_valid(tensor)) {
    return;
  }
  for (int32_t i = 0; i < tensor->size; i++) {
    tensor->data[i] = (float)neuron_random_float();
  }
}

static void tensor_binary_scalar(const float *a, const float *b, float *out,
                                 int32_t n, TensorBinaryOp op) {
  for (int32_t i = 0; i < n; i++) {
    switch (op) {
    case kTensorOpAdd:
      out[i] = a[i] + b[i];
      break;
    case kTensorOpSub:
      out[i] = a[i] - b[i];
      break;
    case kTensorOpMul:
      out[i] = a[i] * b[i];
      break;
    case kTensorOpDiv:
      out[i] = (b[i] == 0.0f) ? 0.0f : (a[i] / b[i]);
      break;
    }
  }
}

static void tensor_fma_scalar(const float *a, const float *b, const float *c,
                              float *out, int32_t n) {
  for (int32_t i = 0; i < n; i++) {
    out[i] = a[i] * b[i] + c[i];
  }
}

static void tensor_binary_vectorized(const float *a, const float *b, float *out,
                                     int32_t n, TensorBinaryOp op) {
  if (op == kTensorOpDiv) {
    tensor_binary_scalar(a, b, out, n, op);
    return;
  }

  int32_t i = 0;

#if defined(__AVX2__)
  for (; i + 8 <= n; i += 8) {
    __m256 va = _mm256_loadu_ps(a + i);
    __m256 vb = _mm256_loadu_ps(b + i);
    __m256 vr;
    if (op == kTensorOpAdd) {
      vr = _mm256_add_ps(va, vb);
    } else if (op == kTensorOpSub) {
      vr = _mm256_sub_ps(va, vb);
    } else {
      vr = _mm256_mul_ps(va, vb);
    }
    _mm256_storeu_ps(out + i, vr);
  }
#endif

#if defined(__SSE__)
  for (; i + 4 <= n; i += 4) {
    __m128 va = _mm_loadu_ps(a + i);
    __m128 vb = _mm_loadu_ps(b + i);
    __m128 vr;
    if (op == kTensorOpAdd) {
      vr = _mm_add_ps(va, vb);
    } else if (op == kTensorOpSub) {
      vr = _mm_sub_ps(va, vb);
    } else {
      vr = _mm_mul_ps(va, vb);
    }
    _mm_storeu_ps(out + i, vr);
  }
#endif

  tensor_binary_scalar(a + i, b + i, out + i, n - i, op);
}

static void tensor_fma_vectorized(const float *a, const float *b,
                                  const float *c, float *out, int32_t n) {
  int32_t i = 0;

#if defined(__AVX2__)
  for (; i + 8 <= n; i += 8) {
    __m256 va = _mm256_loadu_ps(a + i);
    __m256 vb = _mm256_loadu_ps(b + i);
    __m256 vc = _mm256_loadu_ps(c + i);
#if defined(__FMA__)
    __m256 vr = _mm256_fmadd_ps(va, vb, vc);
#else
    __m256 vr = _mm256_add_ps(_mm256_mul_ps(va, vb), vc);
#endif
    _mm256_storeu_ps(out + i, vr);
  }
#endif

#if defined(__SSE__)
  for (; i + 4 <= n; i += 4) {
    __m128 va = _mm_loadu_ps(a + i);
    __m128 vb = _mm_loadu_ps(b + i);
    __m128 vc = _mm_loadu_ps(c + i);
    __m128 vr = _mm_add_ps(_mm_mul_ps(va, vb), vc);
    _mm_storeu_ps(out + i, vr);
  }
#endif

  tensor_fma_scalar(a + i, b + i, c + i, out + i, n - i);
}

static NeuronTensor *tensor_create_impl(int32_t dimensions, int32_t *shape,
                                        int initializeToZero) {
  if (dimensions <= 0 || shape == NULL) {
    return NULL;
  }

  NeuronTensor *t = (NeuronTensor *)neuron_alloc(sizeof(NeuronTensor));
  if (t == NULL) {
    return NULL;
  }

  t->dimensions = dimensions;
  t->shape = (int32_t *)neuron_alloc(sizeof(int32_t) * (size_t)dimensions);
  if (t->shape == NULL) {
    neuron_dealloc(t);
    return NULL;
  }

  memcpy(t->shape, shape, sizeof(int32_t) * (size_t)dimensions);

  int32_t total_size = 1;
  for (int32_t i = 0; i < dimensions; i++) {
    if (shape[i] <= 0) {
      neuron_dealloc(t->shape);
      neuron_dealloc(t);
      return NULL;
    }
    total_size *= shape[i];
  }

  t->size = total_size;
  t->element_size = (int32_t)sizeof(float);
  t->data_type = NEURON_TENSOR_F32;
  t->data = (float *)neuron_alloc((size_t)total_size * sizeof(float));
  if (t->data == NULL) {
    neuron_dealloc(t->shape);
    neuron_dealloc(t);
    return NULL;
  }

  if (initializeToZero) {
    memset(t->data, 0, (size_t)total_size * sizeof(float));
  }
  return t;
}

NeuronTensor *neuron_tensor_create(int32_t dimensions, int32_t *shape) {
  return tensor_create_impl(dimensions, shape, 1);
}

NeuronTensor *neuron_tensor_create_default() {
  int32_t shape[] = {3, 3};
  return neuron_tensor_create(2, shape);
}

NeuronTensor *neuron_tensor_random_2d(int32_t rows, int32_t cols) {
  if (rows <= 0 || cols <= 0) {
    return NULL;
  }
  int32_t shape[] = {rows, cols};
  NeuronTensor *t = neuron_tensor_create(2, shape);
  if (t == NULL) {
    return NULL;
  }
  tensor_fill_random(t);
  return t;
}

void neuron_tensor_free(NeuronTensor *tensor) {
  if (tensor == NULL) {
    return;
  }
  neuron_dealloc(tensor->data);
  neuron_dealloc(tensor->shape);
  neuron_dealloc(tensor);
}

static int tensor_should_try_gpu(NeuronTensorExecHint hint,
                                 NeuronGpuOpKind op) {
  if (hint == NEURON_TENSOR_EXEC_CPU_ONLY) {
    return 0;
  }
  if (hint == NEURON_TENSOR_EXEC_GPU_PREFER) {
    return neuron_gpu_supports_op(op) != 0;
  }
  return 0;
}

static void tensor_prepare_cpu_operand(const NeuronTensor *tensor) {
  if (!tensor_is_valid(tensor)) {
    return;
  }
  (void)neuron_gpu_prepare_cpu_tensor(tensor->data, tensor->size);
}

NeuronTensor *neuron_tensor_add_ex(NeuronTensor *a, NeuronTensor *b,
                                   NeuronTensorExecHint hint) {
  if (!tensor_same_shape(a, b)) {
    printf("[Runtime Error] TensorAdd: shape mismatch or null tensor\n");
    return NULL;
  }
  NeuronTensor *result = tensor_create_like(a);
  if (result == NULL) {
    return NULL;
  }
  if (tensor_should_try_gpu(hint, NEURON_GPU_OP_TENSOR_ADD)) {
    if (neuron_gpu_dispatch_tensor_binary(NEURON_GPU_OP_TENSOR_ADD, a->data,
                                          b->data, result->data,
                                          a->size) == 0) {
      return result;
    }
  }
  tensor_prepare_cpu_operand(a);
  tensor_prepare_cpu_operand(b);
  tensor_binary_vectorized(a->data, b->data, result->data, a->size,
                           kTensorOpAdd);
  return result;
}

NeuronTensor *neuron_tensor_sub_ex(NeuronTensor *a, NeuronTensor *b,
                                   NeuronTensorExecHint hint) {
  if (!tensor_same_shape(a, b)) {
    return NULL;
  }
  NeuronTensor *result = tensor_create_like(a);
  if (result == NULL) {
    return NULL;
  }
  if (tensor_should_try_gpu(hint, NEURON_GPU_OP_TENSOR_SUB)) {
    if (neuron_gpu_dispatch_tensor_binary(NEURON_GPU_OP_TENSOR_SUB, a->data,
                                          b->data, result->data,
                                          a->size) == 0) {
      return result;
    }
  }
  tensor_prepare_cpu_operand(a);
  tensor_prepare_cpu_operand(b);
  tensor_binary_vectorized(a->data, b->data, result->data, a->size,
                           kTensorOpSub);
  return result;
}

NeuronTensor *neuron_tensor_mul_ex(NeuronTensor *a, NeuronTensor *b,
                                   NeuronTensorExecHint hint) {
  if (!tensor_same_shape(a, b)) {
    return NULL;
  }
  NeuronTensor *result = tensor_create_like(a);
  if (result == NULL) {
    return NULL;
  }
  if (tensor_should_try_gpu(hint, NEURON_GPU_OP_TENSOR_MUL)) {
    if (neuron_gpu_dispatch_tensor_binary(NEURON_GPU_OP_TENSOR_MUL, a->data,
                                          b->data, result->data,
                                          a->size) == 0) {
      return result;
    }
  }
  tensor_prepare_cpu_operand(a);
  tensor_prepare_cpu_operand(b);
  tensor_binary_vectorized(a->data, b->data, result->data, a->size,
                           kTensorOpMul);
  return result;
}

NeuronTensor *neuron_tensor_div_ex(NeuronTensor *a, NeuronTensor *b,
                                   NeuronTensorExecHint hint) {
  if (!tensor_same_shape(a, b)) {
    return NULL;
  }
  NeuronTensor *result = tensor_create_like(a);
  if (result == NULL) {
    return NULL;
  }
  if (tensor_should_try_gpu(hint, NEURON_GPU_OP_TENSOR_DIV)) {
    if (neuron_gpu_dispatch_tensor_binary(NEURON_GPU_OP_TENSOR_DIV, a->data,
                                          b->data, result->data,
                                          a->size) == 0) {
      return result;
    }
  }
  tensor_prepare_cpu_operand(a);
  tensor_prepare_cpu_operand(b);
  tensor_binary_vectorized(a->data, b->data, result->data, a->size,
                           kTensorOpDiv);
  return result;
}

NeuronTensor *neuron_tensor_fma_ex(NeuronTensor *a, NeuronTensor *b,
                                   NeuronTensor *c, NeuronTensorExecHint hint) {
  if (!tensor_same_shape(a, b) || !tensor_same_shape(a, c)) {
    printf("[Runtime Error] TensorFMA: shape mismatch or null tensor\n");
    return NULL;
  }
  NeuronTensor *result = tensor_create_like(a);
  if (result == NULL) {
    return NULL;
  }
  if (tensor_should_try_gpu(hint, NEURON_GPU_OP_TENSOR_FMA)) {
    if (neuron_gpu_dispatch_tensor_fma(a->data, b->data, c->data, result->data,
                                       a->size) == 0) {
      return result;
    }
  }
  tensor_prepare_cpu_operand(a);
  tensor_prepare_cpu_operand(b);
  tensor_prepare_cpu_operand(c);
  tensor_fma_vectorized(a->data, b->data, c->data, result->data, a->size);
  return result;
}

NeuronTensor *neuron_tensor_add(NeuronTensor *a, NeuronTensor *b) {
  return neuron_tensor_add_ex(a, b, NEURON_TENSOR_EXEC_AUTO);
}

NeuronTensor *neuron_tensor_sub(NeuronTensor *a, NeuronTensor *b) {
  return neuron_tensor_sub_ex(a, b, NEURON_TENSOR_EXEC_AUTO);
}

NeuronTensor *neuron_tensor_mul(NeuronTensor *a, NeuronTensor *b) {
  return neuron_tensor_mul_ex(a, b, NEURON_TENSOR_EXEC_AUTO);
}

NeuronTensor *neuron_tensor_div(NeuronTensor *a, NeuronTensor *b) {
  return neuron_tensor_div_ex(a, b, NEURON_TENSOR_EXEC_AUTO);
}

NeuronTensor *neuron_tensor_fma(NeuronTensor *a, NeuronTensor *b,
                                NeuronTensor *c) {
  return neuron_tensor_fma_ex(a, b, c, NEURON_TENSOR_EXEC_AUTO);
}

static int tensor_validate_output_matrix(const NeuronTensor *out, int32_t m,
                                         int32_t n) {
  return tensor_is_valid(out) && out->dimensions == 2 && out->shape[0] == m &&
         out->shape[1] == n;
}

static NeuronTensor *tensor_prepare_output_matrix(NeuronTensor *out, int32_t m,
                                                  int32_t n,
                                                  int initializeToZero) {
  if (out != NULL) {
    return tensor_validate_output_matrix(out, m, n) ? out : NULL;
  }
  int32_t outShape[2] = {m, n};
  return tensor_create_impl(2, outShape, initializeToZero);
}

int neuron_tensor_pack_b(const NeuronTensor *b,
                         NeuronPackedMatrix **out_packed) {
  if (out_packed == NULL) {
    return -1;
  }
  *out_packed = NULL;

  if (!tensor_is_valid(b) || b->dimensions != 2) {
    return -1;
  }

  const int32_t k = b->shape[0];
  const int32_t n = b->shape[1];
  if (k <= 0 || n <= 0) {
    return -1;
  }

  NeuronPackedMatrix *packed =
      (NeuronPackedMatrix *)neuron_alloc(sizeof(NeuronPackedMatrix));
  if (packed == NULL) {
    return -1;
  }
  memset(packed, 0, sizeof(*packed));

  packed->rows = k;
  packed->cols = n;
  packed->kc = TENSOR_KC;
  packed->nc = TENSOR_NC;
  packed->kBlocks = (k + TENSOR_KC - 1) / TENSOR_KC;
  packed->nBlocks = (n + TENSOR_NC - 1) / TENSOR_NC;
  packed->panelCount = (size_t)packed->kBlocks * (size_t)packed->nBlocks;

  packed->offsets = (size_t *)neuron_alloc(packed->panelCount * sizeof(size_t));
  if (packed->offsets == NULL) {
    neuron_dealloc(packed);
    return -1;
  }

  size_t totalElements = 0;
  for (int32_t nc0 = 0; nc0 < n; nc0 += TENSOR_NC) {
    const int32_t nc = tensor_min_i32(TENSOR_NC, n - nc0);
    for (int32_t kc0 = 0; kc0 < k; kc0 += TENSOR_KC) {
      const int32_t kc = tensor_min_i32(TENSOR_KC, k - kc0);
      totalElements += (size_t)kc * (size_t)nc;
    }
  }

  packed->data =
      (float *)tensor_aligned_alloc(totalElements * sizeof(float), 64u);
  if (packed->data == NULL) {
    neuron_dealloc(packed->offsets);
    neuron_dealloc(packed);
    return -1;
  }

  size_t cursor = 0;
  size_t panelIndex = 0;
  for (int32_t nc0 = 0; nc0 < n; nc0 += TENSOR_NC) {
    const int32_t nc = tensor_min_i32(TENSOR_NC, n - nc0);
    for (int32_t kc0 = 0; kc0 < k; kc0 += TENSOR_KC) {
      const int32_t kc = tensor_min_i32(TENSOR_KC, k - kc0);
      packed->offsets[panelIndex++] = cursor;
      tensor_pack_b_panel(b->data + (size_t)kc0 * (size_t)n + (size_t)nc0, n,
                          packed->data + cursor, kc, nc);
      cursor += (size_t)kc * (size_t)nc;
    }
  }

  packed->checksum = ((uint64_t)(uint32_t)k << 32) ^ (uint64_t)(uint32_t)n ^
                     (uint64_t)packed->panelCount;
  *out_packed = packed;
  return 0;
}

void neuron_tensor_packed_free(NeuronPackedMatrix *packed) {
  if (packed == NULL) {
    return;
  }
  tensor_aligned_free(packed->data);
  neuron_dealloc(packed->offsets);
  neuron_dealloc(packed);
}

static void tensor_clear_packed_b_cache(void) {
  neuron_tensor_packed_free(g_tensorPackedBCache.packed);
  tensor_aligned_free(g_tensorPackedBCache.circulantSpectrum);
  tensor_aligned_free(g_tensorPackedBCache.toeplitzSpectrum);
  tensor_clear_hybrid_residual(&g_tensorPackedBCache);
  memset(&g_tensorPackedBCache.analysis, 0,
         sizeof(g_tensorPackedBCache.analysis));
  g_tensorPackedBCache.packed = NULL;
  g_tensorPackedBCache.circulantSpectrum = NULL;
  g_tensorPackedBCache.circulantFftSize = 0;
  g_tensorPackedBCache.toeplitzSpectrum = NULL;
  g_tensorPackedBCache.toeplitzFftSize = 0;
  g_tensorPackedBCache.sourceData = NULL;
  g_tensorPackedBCache.rows = 0;
  g_tensorPackedBCache.cols = 0;
  g_tensorPackedBCache.dataFingerprint = 0ULL;
}

static const TensorPackedBCache *
tensor_get_or_create_cached_b_cache(const NeuronTensor *b) {
  if (!tensor_is_valid(b) || b->dimensions != 2) {
    return NULL;
  }

  const uint64_t fingerprint =
      tensor_matrix_fingerprint(b->data, b->shape[0], b->shape[1]);

  if (g_tensorPackedBCache.sourceData == b->data &&
      g_tensorPackedBCache.rows == b->shape[0] &&
      g_tensorPackedBCache.cols == b->shape[1] &&
      g_tensorPackedBCache.dataFingerprint == fingerprint) {
    return &g_tensorPackedBCache;
  }

  tensor_clear_packed_b_cache();
  g_tensorPackedBCache.sourceData = b->data;
  g_tensorPackedBCache.rows = b->shape[0];
  g_tensorPackedBCache.cols = b->shape[1];
  g_tensorPackedBCache.dataFingerprint = fingerprint;
  g_tensorPackedBCache.analysis =
      tensor_analyze_structure_raw(b->data, b->shape[0], b->shape[1]);

  const float structuredThreshold = tensor_structured_threshold();
  float hybridThreshold = tensor_structured_hybrid_threshold();
  if (hybridThreshold > structuredThreshold) {
    hybridThreshold = structuredThreshold;
  }

  const int matrixSquare = (b->shape[0] == b->shape[1]);
  const int32_t n = b->shape[1];
  const int selectedUsable =
      g_tensorPackedBCache.analysis.selectedKind != kTensorStructuredNone;
  const int eligibleForStructured =
      selectedUsable &&
      g_tensorPackedBCache.analysis.selectedScore >= hybridThreshold &&
      matrixSquare;

  if (eligibleForStructured) {
    const TensorStructuredKind selectedKind =
        g_tensorPackedBCache.analysis.selectedKind;
    TensorStructuredKind hybridKind = selectedKind;
    if (g_tensorPackedBCache.analysis.selectedScore < structuredThreshold) {
      hybridKind = tensor_select_hybrid_base_kind(
          b->data, n, &g_tensorPackedBCache.analysis);
    }

    if (selectedKind == kTensorStructuredCirculant &&
        tensor_is_power_of_two_i32(n)) {
      (void)tensor_prepare_circulant_spectrum(&g_tensorPackedBCache, b->data,
                                              n);
    } else if (selectedKind == kTensorStructuredToeplitz) {
      (void)tensor_prepare_toeplitz_spectrum(&g_tensorPackedBCache, b->data, n);
    }

    if (g_tensorPackedBCache.analysis.selectedScore < structuredThreshold) {
      if (hybridKind == kTensorStructuredCirculant &&
          tensor_is_power_of_two_i32(n)) {
        (void)tensor_prepare_circulant_spectrum(&g_tensorPackedBCache, b->data,
                                                n);
      } else if (hybridKind == kTensorStructuredToeplitz) {
        (void)tensor_prepare_toeplitz_spectrum(&g_tensorPackedBCache, b->data,
                                               n);
      }
      if (hybridKind != kTensorStructuredNone) {
        (void)tensor_build_hybrid_residual(&g_tensorPackedBCache, b->data, n,
                                           hybridKind);
      }
    }
  }

  NeuronPackedMatrix *packed = NULL;
  if (neuron_tensor_pack_b(b, &packed) == 0 && packed != NULL) {
    g_tensorPackedBCache.packed = packed;
  }

  return &g_tensorPackedBCache;
}

static int tensor_matmul_try_structured_from_cache(
    const TensorPackedBCache *cache, const float *aData, float *cData,
    int32_t m, int32_t n, int32_t k, int accumulate,
    const TensorEpilogueConfig *epilogue) {
  if (!tensor_structured_enabled() || cache == NULL || aData == NULL ||
      cData == NULL || m <= 0 || n <= 1 || k <= 0 || n != k) {
    return 0;
  }

  const float threshold = tensor_structured_threshold();
  int usedBase = 0;

  if (cache->analysis.selectedKind == kTensorStructuredCirculant &&
      cache->analysis.selectedScore >= threshold &&
      cache->circulantSpectrum != NULL && cache->circulantFftSize == n) {
    usedBase = tensor_matmul_try_circulant_fft(
        aData, cData, m, n, cache->circulantSpectrum, accumulate);
    if (usedBase && tensor_structured_debug_enabled() &&
        !g_tensorStructuredDebugPrinted) {
      g_tensorStructuredDebugPrinted = 1;
      fprintf(stderr,
              "[Neuron Tensor] Structured dispatch: circulant FFT path "
              "(score=%.4f, n=%d)\n",
              cache->analysis.selectedScore, (int)n);
    }
  } else if (cache->analysis.selectedKind == kTensorStructuredToeplitz &&
             cache->analysis.selectedScore >= threshold &&
             cache->toeplitzSpectrum != NULL &&
             cache->toeplitzFftSize >= (2 * n)) {
    usedBase = tensor_matmul_try_toeplitz_fft(
        aData, cData, m, n, cache->toeplitzSpectrum, cache->toeplitzFftSize,
        accumulate);
    if (usedBase && tensor_structured_debug_enabled() &&
        !g_tensorStructuredDebugPrinted) {
      g_tensorStructuredDebugPrinted = 1;
      fprintf(stderr,
              "[Neuron Tensor] Structured dispatch: toeplitz FFT path "
              "(score=%.4f, n=%d, fft=%d)\n",
              cache->analysis.selectedScore, (int)n,
              (int)cache->toeplitzFftSize);
    }
  }

  if (!usedBase && cache->hybridEnabled && cache->hybridN == n) {
    const float denseFallbackDensity =
        tensor_structured_hybrid_dense_fallback_density();
    if (cache->hybridDensity > denseFallbackDensity) {
      if (tensor_structured_debug_enabled() &&
          !g_tensorStructuredDebugPrinted) {
        g_tensorStructuredDebugPrinted = 1;
        fprintf(stderr,
                "[Neuron Tensor] Structured dispatch: hybrid density %.4f "
                "above fallback threshold %.4f -> dense path\n",
                cache->hybridDensity, denseFallbackDensity);
      }
      return 0;
    }

    if (cache->hybridBaseKind == kTensorStructuredCirculant &&
        cache->circulantSpectrum != NULL && cache->circulantFftSize == n) {
      usedBase = tensor_matmul_try_circulant_fft(
          aData, cData, m, n, cache->circulantSpectrum, accumulate);
    } else if (cache->hybridBaseKind == kTensorStructuredToeplitz &&
               cache->toeplitzSpectrum != NULL &&
               cache->toeplitzFftSize >= (2 * n)) {
      usedBase = tensor_matmul_try_toeplitz_fft(
          aData, cData, m, n, cache->toeplitzSpectrum, cache->toeplitzFftSize,
          accumulate);
    }

    if (usedBase) {
      if (cache->hybridUseDenseCorrection &&
          cache->hybridResidualPacked != NULL) {
        TensorMatMulKernelOptions correctionOptions;
        correctionOptions.packedB = cache->hybridResidualPacked;
        correctionOptions.accumulate = 1;
        correctionOptions.bias = NULL;
        correctionOptions.residual = NULL;
        correctionOptions.activation = NEURON_TENSOR_ACTIVATION_NONE;
        tensor_matmul_blocked_packed(aData, NULL, cData, m, n, n,
                                     &correctionOptions);
      } else {
        tensor_apply_sparse_residual(aData, cData, m, n, cache->hybridRowPtr,
                                     cache->hybridColIdx, cache->hybridValues,
                                     cache->hybridNnz);
      }
      if (tensor_structured_debug_enabled() &&
          !g_tensorStructuredDebugPrinted) {
        g_tensorStructuredDebugPrinted = 1;
        fprintf(stderr,
                "[Neuron Tensor] Structured dispatch: hybrid %s FFT + %s "
                "correction (score=%.4f, nnz=%d, density=%.4f)\n",
                cache->hybridBaseKind == kTensorStructuredToeplitz
                    ? "toeplitz"
                    : "circulant",
                (cache->hybridUseDenseCorrection &&
                 cache->hybridResidualPacked != NULL)
                    ? "dense"
                    : "sparse",
                cache->analysis.selectedScore, (int)cache->hybridNnz,
                cache->hybridDensity);
      }
    }
  }

  if (!usedBase) {
    return 0;
  }

  tensor_apply_epilogue_full(cData, m, n, epilogue);
  return 1;
}

void neuron_tensor_release_workspace_cache(void) {
  tensor_aligned_free(g_tensorWorkspace.aPack);
  tensor_aligned_free(g_tensorWorkspace.bPack);
  tensor_aligned_free(g_tensorWorkspace.fftWork);
  tensor_aligned_free(g_tensorWorkspace.fftAux);
  tensor_aligned_free(g_tensorWorkspace.fftTwiddleForward);
  tensor_aligned_free(g_tensorWorkspace.fftTwiddleInverse);
  tensor_aligned_free(g_tensorWorkspace.fftBitRev);
  g_tensorWorkspace.aPack = NULL;
  g_tensorWorkspace.bPack = NULL;
  g_tensorWorkspace.fftWork = NULL;
  g_tensorWorkspace.fftAux = NULL;
  g_tensorWorkspace.fftTwiddleForward = NULL;
  g_tensorWorkspace.fftTwiddleInverse = NULL;
  g_tensorWorkspace.fftBitRev = NULL;
  g_tensorWorkspace.aPackCapacity = 0;
  g_tensorWorkspace.bPackCapacity = 0;
  g_tensorWorkspace.fftWorkCapacity = 0;
  g_tensorWorkspace.fftAuxCapacity = 0;
  g_tensorWorkspace.fftTwiddleForwardCapacity = 0;
  g_tensorWorkspace.fftTwiddleForwardN = 0;
  g_tensorWorkspace.fftTwiddleInverseCapacity = 0;
  g_tensorWorkspace.fftTwiddleInverseN = 0;
  g_tensorWorkspace.fftBitRevCapacity = 0;
  g_tensorWorkspace.fftBitRevN = 0;
  tensor_clear_packed_b_cache();
  g_tensorStructuredDebugPrinted = 0;
}

static int tensor_try_gpu_matmul_dispatch(
    const float *aData, const float *bData, const NeuronPackedMatrix *packedB,
    float *outData, int32_t m, int32_t n, int32_t k, int32_t accumulate,
    const NeuronTensor *bias, const NeuronTensor *residual, int32_t activation,
    NeuronTensorExecHint hint) {
  if (!tensor_should_try_gpu(hint, NEURON_GPU_OP_TENSOR_MATMUL)) {
    return 0;
  }

  NeuronGpuMatMulDispatchDesc desc;
  memset(&desc, 0, sizeof(desc));
  desc.a = aData;
  desc.b = bData;
  desc.packed_b = packedB;
  desc.out = outData;
  desc.m = m;
  desc.n = n;
  desc.k = k;
  desc.accumulate = accumulate;
  desc.activation = activation;

  if (bias != NULL && tensor_is_valid(bias) && bias->dimensions == 2 &&
      bias->shape[1] == n && (bias->shape[0] == 1 || bias->shape[0] == m)) {
    desc.bias = bias->data;
    desc.bias_rows = bias->shape[0];
    desc.bias_cols = bias->shape[1];
  }

  if (residual != NULL && tensor_is_valid(residual) &&
      residual->dimensions == 2 && residual->shape[0] == m &&
      residual->shape[1] == n) {
    desc.residual = residual->data;
    desc.residual_cols = residual->shape[1];
  }

  return neuron_gpu_dispatch_tensor_matmul(&desc) == 0;
}

NeuronTensor *neuron_tensor_matmul_ex_hint(NeuronTensor *a, NeuronTensor *b,
                                           NeuronTensor *out, uint32_t flags,
                                           NeuronTensorExecHint hint) {
  if (!tensor_is_valid(a) || !tensor_is_valid(b)) {
    return NULL;
  }
  if (a->dimensions != 2 || b->dimensions != 2) {
    printf("[Runtime Error] TensorMatMul: only 2D tensors are supported\n");
    return NULL;
  }

  int32_t m = a->shape[0];
  int32_t k = a->shape[1];
  int32_t k2 = b->shape[0];
  int32_t n = b->shape[1];

  if (k != k2) {
    printf("[Runtime Error] TensorMatMul: incompatible shapes (%d x %d) @ (%d "
           "x %d)\n",
           m, k, k2, n);
    return NULL;
  }

  const int accumulate = (flags & NEURON_TENSOR_MATMUL_FLAG_ACCUMULATE) != 0;
  NeuronTensor *result = tensor_prepare_output_matrix(out, m, n, accumulate);
  if (result == NULL) {
    printf("[Runtime Error] TensorMatMul: invalid output tensor shape\n");
    return NULL;
  }

  if (tensor_try_gpu_matmul_dispatch(a->data, b->data, NULL, result->data, m, n,
                                     k, accumulate, NULL, NULL,
                                     NEURON_TENSOR_ACTIVATION_NONE, hint)) {
    return result;
  }

  tensor_prepare_cpu_operand(a);
  tensor_prepare_cpu_operand(b);
  if (accumulate) {
    tensor_prepare_cpu_operand(result);
  }

  const TensorPackedBCache *cachedBCache = NULL;
  const NeuronPackedMatrix *cachedPackedB = NULL;
  const int64_t workload = (int64_t)m * (int64_t)n * (int64_t)k;
  if (workload < 32768) {
    for (int32_t i = 0; i < m; i++) {
      for (int32_t j = 0; j < n; j++) {
        float sum = accumulate ? result->data[i * n + j] : 0.0f;
        for (int32_t p = 0; p < k; p++) {
          sum += a->data[i * k + p] * b->data[p * n + j];
        }
        result->data[i * n + j] = sum;
      }
    }
    return result;
  }

  if (!accumulate) {
    cachedBCache = tensor_get_or_create_cached_b_cache(b);
    if (cachedBCache != NULL) {
      cachedPackedB = cachedBCache->packed;
    }
  }

  if (!accumulate &&
      tensor_matmul_try_structured_from_cache(cachedBCache, a->data,
                                              result->data, m, n, k, 0, NULL)) {
    return result;
  }

  if (!accumulate && tensor_matmul_try_batched_skinny(a->data, b->data,
                                                      result->data, m, n, k)) {
    return result;
  }

  if (!accumulate && tensor_matmul_try_small_or_skinny(a->data, b->data,
                                                       result->data, m, n, k)) {
    return result;
  }

  TensorMatMulKernelOptions options;
  options.packedB = cachedPackedB;
  options.accumulate = accumulate;
  options.bias = NULL;
  options.residual = NULL;
  options.activation = NEURON_TENSOR_ACTIVATION_NONE;
  tensor_matmul_blocked_packed(a->data, cachedPackedB != NULL ? NULL : b->data,
                               result->data, m, n, k, &options);

  return result;
}

NeuronTensor *neuron_tensor_matmul_ex(NeuronTensor *a, NeuronTensor *b,
                                      NeuronTensor *out, uint32_t flags) {
  return neuron_tensor_matmul_ex_hint(a, b, out, flags,
                                      NEURON_TENSOR_EXEC_AUTO);
}

NeuronTensor *neuron_tensor_matmul_packed(NeuronTensor *a,
                                          const NeuronPackedMatrix *packed_b,
                                          NeuronTensor *out, uint32_t flags) {
  if (!tensor_is_valid(a) || packed_b == NULL ||
      packed_b->rows != a->shape[1]) {
    return NULL;
  }
  if (a->dimensions != 2) {
    return NULL;
  }

  const int32_t m = a->shape[0];
  const int32_t k = a->shape[1];
  const int32_t n = packed_b->cols;
  const int accumulate = (flags & NEURON_TENSOR_MATMUL_FLAG_ACCUMULATE) != 0;
  NeuronTensor *result = tensor_prepare_output_matrix(out, m, n, accumulate);
  if (result == NULL) {
    return NULL;
  }

  TensorMatMulKernelOptions options;
  options.packedB = packed_b;
  options.accumulate = accumulate;
  options.bias = NULL;
  options.residual = NULL;
  options.activation = NEURON_TENSOR_ACTIVATION_NONE;
  tensor_matmul_blocked_packed(a->data, NULL, result->data, m, n, k, &options);
  return result;
}

static void tensor_add_bias_inplace(NeuronTensor *out,
                                    const NeuronTensor *bias) {
  if (!tensor_is_valid(out) || bias == NULL || !tensor_is_valid(bias) ||
      bias->dimensions != 2) {
    return;
  }

  const int32_t rows = out->shape[0];
  const int32_t cols = out->shape[1];
  if (bias->shape[1] != cols) {
    return;
  }

  if (bias->shape[0] == 1) {
    for (int32_t r = 0; r < rows; ++r) {
      float *outRow = out->data + (size_t)r * (size_t)cols;
      for (int32_t c = 0; c < cols; ++c) {
        outRow[c] += bias->data[c];
      }
    }
    return;
  }

  if (bias->shape[0] == rows) {
    for (int32_t i = 0; i < out->size; ++i) {
      out->data[i] += bias->data[i];
    }
  }
}

static void tensor_add_residual_inplace(NeuronTensor *out,
                                        const NeuronTensor *residual) {
  if (!tensor_is_valid(out) || residual == NULL ||
      !tensor_same_shape(out, residual)) {
    return;
  }
  for (int32_t i = 0; i < out->size; ++i) {
    out->data[i] += residual->data[i];
  }
}

static float tensor_gelu_approx(float x) {
  const float c = 0.044715f;
  const float scale = 0.7978845608f; // sqrt(2/pi)
  float cubic = x * x * x;
  return 0.5f * x * (1.0f + tanhf(scale * (x + c * cubic)));
}

static void tensor_apply_activation_inplace(NeuronTensor *out,
                                            int32_t activation) {
  if (!tensor_is_valid(out) || activation == NEURON_TENSOR_ACTIVATION_NONE) {
    return;
  }

  if (activation == NEURON_TENSOR_ACTIVATION_RELU) {
    for (int32_t i = 0; i < out->size; ++i) {
      if (out->data[i] < 0.0f) {
        out->data[i] = 0.0f;
      }
    }
    return;
  }

  if (activation == NEURON_TENSOR_ACTIVATION_GELU) {
    for (int32_t i = 0; i < out->size; ++i) {
      out->data[i] = tensor_gelu_approx(out->data[i]);
    }
  }
}

static int tensor_is_nchw_4d(const NeuronTensor *tensor) {
  return tensor_is_valid(tensor) && tensor->dimensions == 4 &&
         tensor->shape[0] > 0 && tensor->shape[1] > 0 &&
         tensor->shape[2] > 0 && tensor->shape[3] > 0;
}

static size_t tensor_nchw_offset(const NeuronTensor *tensor, int32_t n,
                                 int32_t c, int32_t h, int32_t w) {
  return (((size_t)n * (size_t)tensor->shape[1] + (size_t)c) *
              (size_t)tensor->shape[2] +
          (size_t)h) *
             (size_t)tensor->shape[3] +
         (size_t)w;
}

static NeuronTensor *tensor_create_nchw_4d(int32_t n, int32_t c, int32_t h,
                                           int32_t w) {
  int32_t shape[4] = {n, c, h, w};
  return neuron_tensor_create(4, shape);
}

static int tensor_channel_value(const NeuronTensor *tensor, int32_t channels,
                                int32_t channel, float default_value,
                                float *out_value) {
  if (out_value == NULL || channel < 0 || channel >= channels) {
    return 0;
  }
  if (tensor == NULL) {
    *out_value = default_value;
    return 1;
  }
  if (!tensor_is_valid(tensor)) {
    return 0;
  }

  if (tensor->dimensions == 1 && tensor->shape[0] == channels) {
    *out_value = tensor->data[channel];
    return 1;
  }
  if (tensor->dimensions == 2 && tensor->shape[0] == 1 &&
      tensor->shape[1] == channels) {
    *out_value = tensor->data[channel];
    return 1;
  }
  if (tensor->dimensions == 4 && tensor->shape[0] == 1 &&
      tensor->shape[1] == channels && tensor->shape[2] == 1 &&
      tensor->shape[3] == 1) {
    *out_value = tensor->data[channel];
    return 1;
  }
  return 0;
}

NeuronTensor *neuron_tensor_conv2d_batchnorm_relu_ex_hint(
    NeuronTensor *input, NeuronTensor *kernel, NeuronTensor *bias,
    NeuronTensor *gamma, NeuronTensor *beta, NeuronTensor *mean,
    NeuronTensor *variance, float epsilon, int32_t stride_h, int32_t stride_w,
    int32_t padding_h, int32_t padding_w, NeuronTensorExecHint hint) {
  (void)hint;

  if (!tensor_is_nchw_4d(input) || !tensor_is_nchw_4d(kernel) ||
      gamma == NULL || beta == NULL || mean == NULL || variance == NULL ||
      stride_h <= 0 || stride_w <= 0 || padding_h < 0 || padding_w < 0 ||
      epsilon < 0.0f) {
    return NULL;
  }

  const int32_t batch = input->shape[0];
  const int32_t in_channels = input->shape[1];
  const int32_t in_height = input->shape[2];
  const int32_t in_width = input->shape[3];
  const int32_t out_channels = kernel->shape[0];
  const int32_t kernel_channels = kernel->shape[1];
  const int32_t kernel_height = kernel->shape[2];
  const int32_t kernel_width = kernel->shape[3];

  if (kernel_channels != in_channels) {
    return NULL;
  }

  const int32_t output_h_numer =
      in_height + 2 * padding_h - kernel_height;
  const int32_t output_w_numer = in_width + 2 * padding_w - kernel_width;
  if (output_h_numer < 0 || output_w_numer < 0) {
    return NULL;
  }

  const int32_t out_height = output_h_numer / stride_h + 1;
  const int32_t out_width = output_w_numer / stride_w + 1;
  if (out_height <= 0 || out_width <= 0) {
    return NULL;
  }

  tensor_prepare_cpu_operand(input);
  tensor_prepare_cpu_operand(kernel);
  tensor_prepare_cpu_operand(gamma);
  tensor_prepare_cpu_operand(beta);
  tensor_prepare_cpu_operand(mean);
  tensor_prepare_cpu_operand(variance);
  if (bias != NULL) {
    tensor_prepare_cpu_operand(bias);
  }

  for (int32_t oc = 0; oc < out_channels; ++oc) {
    float variance_value = 0.0f;
    if (!tensor_channel_value(variance, out_channels, oc, 0.0f,
                              &variance_value) ||
        variance_value + epsilon <= 0.0f) {
      return NULL;
    }
  }

  NeuronTensor *result =
      tensor_create_nchw_4d(batch, out_channels, out_height, out_width);
  if (result == NULL) {
    return NULL;
  }

  for (int32_t n = 0; n < batch; ++n) {
    for (int32_t oc = 0; oc < out_channels; ++oc) {
      float bias_value = 0.0f;
      float gamma_value = 1.0f;
      float beta_value = 0.0f;
      float mean_value = 0.0f;
      float variance_value = 0.0f;

      if (!tensor_channel_value(bias, out_channels, oc, 0.0f, &bias_value) ||
          !tensor_channel_value(gamma, out_channels, oc, 1.0f, &gamma_value) ||
          !tensor_channel_value(beta, out_channels, oc, 0.0f, &beta_value) ||
          !tensor_channel_value(mean, out_channels, oc, 0.0f, &mean_value) ||
          !tensor_channel_value(variance, out_channels, oc, 0.0f,
                                &variance_value)) {
        neuron_tensor_free(result);
        return NULL;
      }

      const float inv_std = 1.0f / sqrtf(variance_value + epsilon);
      for (int32_t oh = 0; oh < out_height; ++oh) {
        for (int32_t ow = 0; ow < out_width; ++ow) {
          float sum = bias_value;
          for (int32_t ic = 0; ic < in_channels; ++ic) {
            for (int32_t kh = 0; kh < kernel_height; ++kh) {
              const int32_t in_h = oh * stride_h - padding_h + kh;
              if (in_h < 0 || in_h >= in_height) {
                continue;
              }
              for (int32_t kw = 0; kw < kernel_width; ++kw) {
                const int32_t in_w = ow * stride_w - padding_w + kw;
                if (in_w < 0 || in_w >= in_width) {
                  continue;
                }
                const size_t input_offset =
                    tensor_nchw_offset(input, n, ic, in_h, in_w);
                const size_t kernel_offset =
                    tensor_nchw_offset(kernel, oc, ic, kh, kw);
                sum += input->data[input_offset] * kernel->data[kernel_offset];
              }
            }
          }

          float value = (sum - mean_value) * inv_std;
          value = value * gamma_value + beta_value;
          if (value < 0.0f) {
            value = 0.0f;
          }

          const size_t output_offset =
              tensor_nchw_offset(result, n, oc, oh, ow);
          result->data[output_offset] = value;
        }
      }
    }
  }

  return result;
}

NeuronTensor *neuron_tensor_conv2d_batchnorm_relu(
    NeuronTensor *input, NeuronTensor *kernel, NeuronTensor *bias,
    NeuronTensor *gamma, NeuronTensor *beta, NeuronTensor *mean,
    NeuronTensor *variance, float epsilon, int32_t stride_h, int32_t stride_w,
    int32_t padding_h, int32_t padding_w) {
  return neuron_tensor_conv2d_batchnorm_relu_ex_hint(
      input, kernel, bias, gamma, beta, mean, variance, epsilon, stride_h,
      stride_w, padding_h, padding_w, NEURON_TENSOR_EXEC_AUTO);
}

NeuronTensor *neuron_tensor_linear_fused_ex_hint(
    NeuronTensor *a, NeuronTensor *b, const NeuronPackedMatrix *packed_b,
    NeuronTensor *bias, NeuronTensor *residual, int32_t activation,
    NeuronTensor *out, uint32_t flags, NeuronTensorExecHint hint) {
  if (!tensor_is_valid(a) || a->dimensions != 2) {
    return NULL;
  }

  const int32_t m = a->shape[0];
  const int32_t k = a->shape[1];
  int32_t n = 0;
  const float *bData = NULL;

  if (packed_b != NULL) {
    if (packed_b->rows != k || packed_b->cols <= 0) {
      return NULL;
    }
    n = packed_b->cols;
    if (tensor_is_valid(b) && b->dimensions == 2 && b->shape[0] == k &&
        b->shape[1] == n) {
      bData = b->data;
    }
  } else {
    if (!tensor_is_valid(b) || b->dimensions != 2 || b->shape[0] != k) {
      return NULL;
    }
    n = b->shape[1];
    bData = b->data;
  }

  const int accumulate = (flags & NEURON_TENSOR_MATMUL_FLAG_ACCUMULATE) != 0;
  const int activationGpuSupported =
      activation == NEURON_TENSOR_ACTIVATION_NONE ||
      activation == NEURON_TENSOR_ACTIVATION_RELU ||
      activation == NEURON_TENSOR_ACTIVATION_GELU;

  if (activationGpuSupported &&
      tensor_should_try_gpu(hint, NEURON_GPU_OP_TENSOR_MATMUL)) {
    NeuronTensor *gpuResult =
        tensor_prepare_output_matrix(out, m, n, accumulate);
    if (gpuResult == NULL) {
      return NULL;
    }
    if (tensor_try_gpu_matmul_dispatch(a->data, bData, packed_b,
                                       gpuResult->data, m, n, k, accumulate,
                                       bias, residual, activation, hint)) {
      return gpuResult;
    }
    out = gpuResult;
  }

  tensor_prepare_cpu_operand(a);
  if (tensor_is_valid(b)) {
    tensor_prepare_cpu_operand(b);
  }
  if (bias != NULL) {
    tensor_prepare_cpu_operand(bias);
  }
  if (residual != NULL) {
    tensor_prepare_cpu_operand(residual);
  }
  if (accumulate && out != NULL) {
    tensor_prepare_cpu_operand(out);
  }

  // Fast path: keep bias/residual/relu epilogue inside matmul tiling.
  if (activation == NEURON_TENSOR_ACTIVATION_NONE ||
      activation == NEURON_TENSOR_ACTIVATION_RELU) {
    NeuronTensor *result = tensor_prepare_output_matrix(out, m, n, accumulate);
    if (result == NULL) {
      return NULL;
    }

    const int structuredBiasSupported =
        bias != NULL && tensor_is_valid(bias) && bias->dimensions == 2 &&
        bias->shape[1] == n && (bias->shape[0] == 1 || bias->shape[0] == m);
    const int structuredResidualSupported =
        residual != NULL && tensor_is_valid(residual) &&
        residual->dimensions == 2 && residual->shape[0] == m &&
        residual->shape[1] == n;
    const int structuredEpilogueEnabled =
        structuredBiasSupported || structuredResidualSupported ||
        activation != NEURON_TENSOR_ACTIVATION_NONE;
    TensorEpilogueConfig structuredEpilogue = tensor_build_epilogue_config(
        structuredEpilogueEnabled, structuredBiasSupported ? bias->data : NULL,
        structuredBiasSupported ? bias->shape[0] : 0,
        structuredBiasSupported ? bias->shape[1] : 0,
        structuredResidualSupported ? residual->data : NULL,
        structuredResidualSupported ? residual->shape[1] : 0, activation);

    const TensorPackedBCache *cachedBCache = NULL;
    if (!accumulate && bData != NULL && tensor_is_valid(b)) {
      cachedBCache = tensor_get_or_create_cached_b_cache(b);
      if (cachedBCache != NULL && tensor_matmul_try_structured_from_cache(
                                      cachedBCache, a->data, result->data, m, n,
                                      k, 0, &structuredEpilogue)) {
        return result;
      }
    }

    const NeuronPackedMatrix *matmulPackedB = packed_b;
    if (matmulPackedB == NULL && cachedBCache != NULL) {
      matmulPackedB = cachedBCache->packed;
    } else if (matmulPackedB == NULL && !accumulate && tensor_is_valid(b)) {
      const TensorPackedBCache *fallbackCache =
          tensor_get_or_create_cached_b_cache(b);
      matmulPackedB = fallbackCache != NULL ? fallbackCache->packed : NULL;
    }
    const float *matmulBData = (matmulPackedB != NULL) ? NULL : bData;

    TensorMatMulKernelOptions options;
    options.packedB = matmulPackedB;
    options.accumulate = accumulate;
    options.bias = bias;
    options.residual = residual;
    options.activation = activation;
    tensor_matmul_blocked_packed(a->data, matmulBData, result->data, m, n, k,
                                 &options);
    return result;
  }

  // Fallback for non-ReLU activations (e.g. GELU).
  NeuronTensor *result =
      packed_b != NULL ? neuron_tensor_matmul_packed(a, packed_b, out, flags)
                       : neuron_tensor_matmul_ex_hint(
                             a, b, out, flags, NEURON_TENSOR_EXEC_CPU_ONLY);
  if (result == NULL) {
    return NULL;
  }

  tensor_add_bias_inplace(result, bias);
  tensor_add_residual_inplace(result, residual);
  tensor_apply_activation_inplace(result, activation);
  return result;
}

NeuronTensor *neuron_tensor_linear_fused(NeuronTensor *a, NeuronTensor *b,
                                         const NeuronPackedMatrix *packed_b,
                                         NeuronTensor *bias,
                                         NeuronTensor *residual,
                                         int32_t activation, NeuronTensor *out,
                                         uint32_t flags) {
  return neuron_tensor_linear_fused_ex_hint(a, b, packed_b, bias, residual,
                                            activation, out, flags,
                                            NEURON_TENSOR_EXEC_AUTO);
}

NeuronTensor *neuron_tensor_matmul_add_ex_hint(NeuronTensor *a, NeuronTensor *b,
                                               NeuronTensor *bias,
                                               NeuronTensorExecHint hint) {
  return neuron_tensor_linear_fused_ex_hint(
      a, b, NULL, bias, NULL, NEURON_TENSOR_ACTIVATION_NONE, NULL, 0, hint);
}

NeuronTensor *neuron_tensor_matmul_add(NeuronTensor *a, NeuronTensor *b,
                                       NeuronTensor *bias) {
  return neuron_tensor_matmul_add_ex_hint(a, b, bias, NEURON_TENSOR_EXEC_AUTO);
}

NeuronTensor *neuron_tensor_matmul(NeuronTensor *a, NeuronTensor *b) {
  return neuron_tensor_matmul_ex(a, b, NULL, 0);
}

int neuron_tensor_analyze_structure(const NeuronTensor *matrix,
                                    NeuronTensorStructureInfo *out_info) {
  if (out_info == NULL || !tensor_is_valid(matrix) || matrix->dimensions != 2) {
    return -1;
  }

  const TensorStructureAnalysis analysis = tensor_analyze_structure_raw(
      matrix->data, matrix->shape[0], matrix->shape[1]);
  out_info->rows = matrix->shape[0];
  out_info->cols = matrix->shape[1];
  out_info->circulant_score = analysis.circulantScore;
  out_info->toeplitz_score = analysis.toeplitzScore;
  out_info->sparsity = analysis.sparsity;
  out_info->selected_score = analysis.selectedScore;
  switch (analysis.selectedKind) {
  case kTensorStructuredCirculant:
    out_info->selected_kind = NEURON_TENSOR_STRUCTURE_CIRCULANT;
    break;
  case kTensorStructuredToeplitz:
    out_info->selected_kind = NEURON_TENSOR_STRUCTURE_TOEPLITZ;
    break;
  default:
    out_info->selected_kind = NEURON_TENSOR_STRUCTURE_NONE;
    break;
  }

  return 0;
}

void neuron_tensor_print(NeuronTensor *tensor) {
  if (!tensor_is_valid(tensor)) {
    printf("null\n");
    return;
  }

  printf("Tensor(shape=[");
  for (int32_t d = 0; d < tensor->dimensions; d++) {
    printf("%d%s", tensor->shape[d], (d + 1 == tensor->dimensions) ? "" : ", ");
  }
  printf("], data=[");
  for (int32_t i = 0; i < tensor->size; i++) {
    printf("%f%s", tensor->data[i], (i + 1 == tensor->size) ? "" : ", ");
  }
  printf("])\n");
}

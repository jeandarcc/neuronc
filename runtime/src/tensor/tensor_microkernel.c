#include "tensor_core_internal.h"
#include "tensor_math_internal.h"
#include "tensor_microkernel_internal.h"
#include <string.h>

#if defined(__AVX2__) || defined(__SSE__)
#include <immintrin.h>
#endif

void tensor_pack_a_panel(const float *src, int32_t lda, float *dst, int32_t mc,
                         int32_t kc) {
  for (int32_t i = 0; i < mc; ++i) {
    const float *srcRow = src + (size_t)i * (size_t)lda;
    float *dstRow = dst + (size_t)i * (size_t)kc;
    memcpy(dstRow, srcRow, (size_t)kc * sizeof(float));
  }
}

void tensor_pack_b_panel(const float *src, int32_t ldb, float *dst, int32_t kc,
                         int32_t nc) {
  for (int32_t p = 0; p < kc; ++p) {
    const float *srcRow = src + (size_t)p * (size_t)ldb;
    float *dstRow = dst + (size_t)p * (size_t)nc;
    memcpy(dstRow, srcRow, (size_t)nc * sizeof(float));
  }
}

void tensor_microkernel_scalar(const float *aPack, const float *bPack, float *c,
                               int32_t ldc, int32_t kc, int32_t nc, int32_t mr,
                               int32_t nr, int accumulate) {
  for (int32_t i = 0; i < mr; ++i) {
    float *cRow = c + (size_t)i * (size_t)ldc;
    const float *aRow = aPack + (size_t)i * (size_t)kc;
    for (int32_t j = 0; j < nr; ++j) {
      float sum = accumulate ? cRow[j] : 0.0f;
      for (int32_t p = 0; p < kc; ++p) {
        sum += aRow[p] * bPack[(size_t)p * (size_t)nc + (size_t)j];
      }
      cRow[j] = sum;
    }
  }
}

int tensor_cpu_supports_avx2_fma(void) {
#if defined(__AVX2__) && defined(__FMA__) &&                                   \
    (defined(__x86_64__) || defined(__i386__)) &&                              \
    (defined(__GNUC__) || defined(__clang__))
  return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#else
  return 0;
#endif
}

#if defined(__AVX2__) && defined(__FMA__)
static void tensor_microkernel_avx2_8x8(const float *aPack, const float *bPack,
                                        float *c, int32_t ldc, int32_t kc,
                                        int32_t nc, int accumulate) {
  __m256 c0 = accumulate ? _mm256_loadu_ps(c + 0 * ldc) : _mm256_setzero_ps();
  __m256 c1 = accumulate ? _mm256_loadu_ps(c + 1 * ldc) : _mm256_setzero_ps();
  __m256 c2 = accumulate ? _mm256_loadu_ps(c + 2 * ldc) : _mm256_setzero_ps();
  __m256 c3 = accumulate ? _mm256_loadu_ps(c + 3 * ldc) : _mm256_setzero_ps();
  __m256 c4 = accumulate ? _mm256_loadu_ps(c + 4 * ldc) : _mm256_setzero_ps();
  __m256 c5 = accumulate ? _mm256_loadu_ps(c + 5 * ldc) : _mm256_setzero_ps();
  __m256 c6 = accumulate ? _mm256_loadu_ps(c + 6 * ldc) : _mm256_setzero_ps();
  __m256 c7 = accumulate ? _mm256_loadu_ps(c + 7 * ldc) : _mm256_setzero_ps();

  for (int32_t p = 0; p < kc; ++p) {
    const float *bRow = bPack + (size_t)p * (size_t)nc;
    __m256 vb = _mm256_loadu_ps(bRow);
    __m256 va0 = _mm256_set1_ps(aPack[(size_t)0 * (size_t)kc + (size_t)p]);
    __m256 va1 = _mm256_set1_ps(aPack[(size_t)1 * (size_t)kc + (size_t)p]);
    __m256 va2 = _mm256_set1_ps(aPack[(size_t)2 * (size_t)kc + (size_t)p]);
    __m256 va3 = _mm256_set1_ps(aPack[(size_t)3 * (size_t)kc + (size_t)p]);
    __m256 va4 = _mm256_set1_ps(aPack[(size_t)4 * (size_t)kc + (size_t)p]);
    __m256 va5 = _mm256_set1_ps(aPack[(size_t)5 * (size_t)kc + (size_t)p]);
    __m256 va6 = _mm256_set1_ps(aPack[(size_t)6 * (size_t)kc + (size_t)p]);
    __m256 va7 = _mm256_set1_ps(aPack[(size_t)7 * (size_t)kc + (size_t)p]);

    c0 = _mm256_fmadd_ps(va0, vb, c0);
    c1 = _mm256_fmadd_ps(va1, vb, c1);
    c2 = _mm256_fmadd_ps(va2, vb, c2);
    c3 = _mm256_fmadd_ps(va3, vb, c3);
    c4 = _mm256_fmadd_ps(va4, vb, c4);
    c5 = _mm256_fmadd_ps(va5, vb, c5);
    c6 = _mm256_fmadd_ps(va6, vb, c6);
    c7 = _mm256_fmadd_ps(va7, vb, c7);
  }

  _mm256_storeu_ps(c + 0 * ldc, c0);
  _mm256_storeu_ps(c + 1 * ldc, c1);
  _mm256_storeu_ps(c + 2 * ldc, c2);
  _mm256_storeu_ps(c + 3 * ldc, c3);
  _mm256_storeu_ps(c + 4 * ldc, c4);
  _mm256_storeu_ps(c + 5 * ldc, c5);
  _mm256_storeu_ps(c + 6 * ldc, c6);
  _mm256_storeu_ps(c + 7 * ldc, c7);
}

static void tensor_microkernel_avx2_4x16(const float *aPack, const float *bPack,
                                         float *c, int32_t ldc, int32_t kc,
                                         int32_t nc, int accumulate,
                                         const TensorEpilogueConfig *epilogue,
                                         int32_t rowBase, int32_t colBase) {
  __m256 c00 =
      accumulate ? _mm256_loadu_ps(c + 0 * ldc + 0) : _mm256_setzero_ps();
  __m256 c01 =
      accumulate ? _mm256_loadu_ps(c + 0 * ldc + 8) : _mm256_setzero_ps();
  __m256 c10 =
      accumulate ? _mm256_loadu_ps(c + 1 * ldc + 0) : _mm256_setzero_ps();
  __m256 c11 =
      accumulate ? _mm256_loadu_ps(c + 1 * ldc + 8) : _mm256_setzero_ps();
  __m256 c20 =
      accumulate ? _mm256_loadu_ps(c + 2 * ldc + 0) : _mm256_setzero_ps();
  __m256 c21 =
      accumulate ? _mm256_loadu_ps(c + 2 * ldc + 8) : _mm256_setzero_ps();
  __m256 c30 =
      accumulate ? _mm256_loadu_ps(c + 3 * ldc + 0) : _mm256_setzero_ps();
  __m256 c31 =
      accumulate ? _mm256_loadu_ps(c + 3 * ldc + 8) : _mm256_setzero_ps();

  for (int32_t p = 0; p < kc; ++p) {
    const float *bRow = bPack + (size_t)p * (size_t)nc;
    const __m256 vb0 = _mm256_loadu_ps(bRow + 0);
    const __m256 vb1 = _mm256_loadu_ps(bRow + 8);
    const __m256 va0 =
        _mm256_set1_ps(aPack[(size_t)0 * (size_t)kc + (size_t)p]);
    const __m256 va1 =
        _mm256_set1_ps(aPack[(size_t)1 * (size_t)kc + (size_t)p]);
    const __m256 va2 =
        _mm256_set1_ps(aPack[(size_t)2 * (size_t)kc + (size_t)p]);
    const __m256 va3 =
        _mm256_set1_ps(aPack[(size_t)3 * (size_t)kc + (size_t)p]);

    c00 = _mm256_fmadd_ps(va0, vb0, c00);
    c01 = _mm256_fmadd_ps(va0, vb1, c01);
    c10 = _mm256_fmadd_ps(va1, vb0, c10);
    c11 = _mm256_fmadd_ps(va1, vb1, c11);
    c20 = _mm256_fmadd_ps(va2, vb0, c20);
    c21 = _mm256_fmadd_ps(va2, vb1, c21);
    c30 = _mm256_fmadd_ps(va3, vb0, c30);
    c31 = _mm256_fmadd_ps(va3, vb1, c31);
  }

  if (epilogue != NULL && epilogue->enabled) {
    if (epilogue->biasData != NULL) {
      if (epilogue->biasRows == 1) {
        const float *biasRow = epilogue->biasData + (size_t)colBase;
        const __m256 vb0 = _mm256_loadu_ps(biasRow + 0);
        const __m256 vb1 = _mm256_loadu_ps(biasRow + 8);
        c00 = _mm256_add_ps(c00, vb0);
        c01 = _mm256_add_ps(c01, vb1);
        c10 = _mm256_add_ps(c10, vb0);
        c11 = _mm256_add_ps(c11, vb1);
        c20 = _mm256_add_ps(c20, vb0);
        c21 = _mm256_add_ps(c21, vb1);
        c30 = _mm256_add_ps(c30, vb0);
        c31 = _mm256_add_ps(c31, vb1);
      } else {
        const float *b0 = epilogue->biasData +
                          (size_t)(rowBase + 0) * (size_t)epilogue->biasCols +
                          (size_t)colBase;
        const float *b1 = epilogue->biasData +
                          (size_t)(rowBase + 1) * (size_t)epilogue->biasCols +
                          (size_t)colBase;
        const float *b2 = epilogue->biasData +
                          (size_t)(rowBase + 2) * (size_t)epilogue->biasCols +
                          (size_t)colBase;
        const float *b3 = epilogue->biasData +
                          (size_t)(rowBase + 3) * (size_t)epilogue->biasCols +
                          (size_t)colBase;

        c00 = _mm256_add_ps(c00, _mm256_loadu_ps(b0 + 0));
        c01 = _mm256_add_ps(c01, _mm256_loadu_ps(b0 + 8));
        c10 = _mm256_add_ps(c10, _mm256_loadu_ps(b1 + 0));
        c11 = _mm256_add_ps(c11, _mm256_loadu_ps(b1 + 8));
        c20 = _mm256_add_ps(c20, _mm256_loadu_ps(b2 + 0));
        c21 = _mm256_add_ps(c21, _mm256_loadu_ps(b2 + 8));
        c30 = _mm256_add_ps(c30, _mm256_loadu_ps(b3 + 0));
        c31 = _mm256_add_ps(c31, _mm256_loadu_ps(b3 + 8));
      }
    }

    if (epilogue->residualData != NULL) {
      const float *r0 = epilogue->residualData +
                        (size_t)(rowBase + 0) * (size_t)epilogue->residualCols +
                        (size_t)colBase;
      const float *r1 = epilogue->residualData +
                        (size_t)(rowBase + 1) * (size_t)epilogue->residualCols +
                        (size_t)colBase;
      const float *r2 = epilogue->residualData +
                        (size_t)(rowBase + 2) * (size_t)epilogue->residualCols +
                        (size_t)colBase;
      const float *r3 = epilogue->residualData +
                        (size_t)(rowBase + 3) * (size_t)epilogue->residualCols +
                        (size_t)colBase;

      c00 = _mm256_add_ps(c00, _mm256_loadu_ps(r0 + 0));
      c01 = _mm256_add_ps(c01, _mm256_loadu_ps(r0 + 8));
      c10 = _mm256_add_ps(c10, _mm256_loadu_ps(r1 + 0));
      c11 = _mm256_add_ps(c11, _mm256_loadu_ps(r1 + 8));
      c20 = _mm256_add_ps(c20, _mm256_loadu_ps(r2 + 0));
      c21 = _mm256_add_ps(c21, _mm256_loadu_ps(r2 + 8));
      c30 = _mm256_add_ps(c30, _mm256_loadu_ps(r3 + 0));
      c31 = _mm256_add_ps(c31, _mm256_loadu_ps(r3 + 8));
    }

    if (epilogue->activation == NEURON_TENSOR_ACTIVATION_RELU) {
      const __m256 zero = _mm256_setzero_ps();
      c00 = _mm256_max_ps(c00, zero);
      c01 = _mm256_max_ps(c01, zero);
      c10 = _mm256_max_ps(c10, zero);
      c11 = _mm256_max_ps(c11, zero);
      c20 = _mm256_max_ps(c20, zero);
      c21 = _mm256_max_ps(c21, zero);
      c30 = _mm256_max_ps(c30, zero);
      c31 = _mm256_max_ps(c31, zero);
    }
  }

  _mm256_storeu_ps(c + 0 * ldc + 0, c00);
  _mm256_storeu_ps(c + 0 * ldc + 8, c01);
  _mm256_storeu_ps(c + 1 * ldc + 0, c10);
  _mm256_storeu_ps(c + 1 * ldc + 8, c11);
  _mm256_storeu_ps(c + 2 * ldc + 0, c20);
  _mm256_storeu_ps(c + 2 * ldc + 8, c21);
  _mm256_storeu_ps(c + 3 * ldc + 0, c30);
  _mm256_storeu_ps(c + 3 * ldc + 8, c31);
}
#endif

int tensor_compute_mc_block(const float *aPack, const float *bPack,
                            float *cBlock, int32_t n, int32_t kc, int32_t nc,
                            int32_t mc, int useAVX2, int accumulate,
                            TensorKernelVariant kernelVariant,
                            const TensorEpilogueConfig *epilogue,
                            int32_t blockRow0, int32_t blockCol0) {
  const int epilogueRequested = epilogue != NULL && epilogue->enabled;

#if defined(__AVX2__) && defined(__FMA__)
  if (useAVX2 && kernelVariant == kTensorKernel4x16) {
    const int canFuseEpilogue =
        !epilogueRequested || ((mc % 4) == 0 && (nc % 16) == 0);
    if (canFuseEpilogue) {
      int32_t i = 0;
      for (; i + 4 <= mc; i += 4) {
        const float *aMicro = aPack + (size_t)i * (size_t)kc;
        int32_t j = 0;
        for (; j + 16 <= nc; j += 16) {
          float *cMicro = cBlock + (size_t)i * (size_t)n + (size_t)j;
          const float *bMicro = bPack + (size_t)j;
          tensor_microkernel_avx2_4x16(aMicro, bMicro, cMicro, n, kc, nc,
                                       accumulate,
                                       epilogueRequested ? epilogue : NULL,
                                       blockRow0 + i, blockCol0 + j);
        }
        for (; j < nc; j += TENSOR_NR) {
          const int32_t nr = tensor_min_i32(TENSOR_NR, nc - j);
          float *cMicro = cBlock + (size_t)i * (size_t)n + (size_t)j;
          const float *bMicro = bPack + (size_t)j;
          tensor_microkernel_scalar(aMicro, bMicro, cMicro, n, kc, nc, 4, nr,
                                    accumulate);
        }
      }

      for (; i < mc; i += TENSOR_MR) {
        const int32_t mr = tensor_min_i32(TENSOR_MR, mc - i);
        const float *aMicro = aPack + (size_t)i * (size_t)kc;
        for (int32_t j = 0; j < nc; j += TENSOR_NR) {
          const int32_t nr = tensor_min_i32(TENSOR_NR, nc - j);
          float *cMicro = cBlock + (size_t)i * (size_t)n + (size_t)j;
          const float *bMicro = bPack + (size_t)j;
          tensor_microkernel_scalar(aMicro, bMicro, cMicro, n, kc, nc, mr, nr,
                                    accumulate);
        }
      }
      return epilogueRequested ? 1 : 0;
    }
  }
#endif

  for (int32_t i = 0; i < mc; i += TENSOR_MR) {
    const int32_t mr = tensor_min_i32(TENSOR_MR, mc - i);
    const float *aMicro = aPack + (size_t)i * (size_t)kc;

    for (int32_t j = 0; j < nc; j += TENSOR_NR) {
      const int32_t nr = tensor_min_i32(TENSOR_NR, nc - j);
      float *cMicro = cBlock + (size_t)i * (size_t)n + (size_t)j;
      const float *bMicro = bPack + (size_t)j;

#if defined(__AVX2__) && defined(__FMA__)
      if (useAVX2 && kernelVariant == kTensorKernel8x8 && mr == TENSOR_MR &&
          nr == TENSOR_NR) {
        tensor_microkernel_avx2_8x8(aMicro, bMicro, cMicro, n, kc, nc,
                                    accumulate);
        continue;
      }
#endif
      tensor_microkernel_scalar(aMicro, bMicro, cMicro, n, kc, nc, mr, nr,
                                accumulate);
    }
  }

  return 0;
}

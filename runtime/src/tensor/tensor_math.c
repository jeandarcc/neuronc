#include "tensor_core_internal.h"
#include "tensor_math_internal.h"


#include <math.h>
#include <stddef.h>

#if defined(__AVX2__) || defined(__SSE__)
#include <immintrin.h>
#endif

float tensor_dot_product(const float *x, const float *y, int32_t n) {
  float sum = 0.0f;
  int32_t i = 0;
#if defined(__AVX2__)
  __m256 acc8 = _mm256_setzero_ps();
  for (; i + 8 <= n; i += 8) {
    __m256 vx = _mm256_loadu_ps(x + i);
    __m256 vy = _mm256_loadu_ps(y + i);
#if defined(__FMA__)
    acc8 = _mm256_fmadd_ps(vx, vy, acc8);
#else
    acc8 = _mm256_add_ps(acc8, _mm256_mul_ps(vx, vy));
#endif
  }
  float tmp[8];
  _mm256_storeu_ps(tmp, acc8);
  for (int t = 0; t < 8; ++t) {
    sum += tmp[t];
  }
#endif
#if defined(__SSE__)
  __m128 acc4 = _mm_setzero_ps();
  for (; i + 4 <= n; i += 4) {
    __m128 vx = _mm_loadu_ps(x + i);
    __m128 vy = _mm_loadu_ps(y + i);
    acc4 = _mm_add_ps(acc4, _mm_mul_ps(vx, vy));
  }
  float tmp4[4];
  _mm_storeu_ps(tmp4, acc4);
  for (int t = 0; t < 4; ++t) {
    sum += tmp4[t];
  }
#endif
  for (; i < n; ++i) {
    sum += x[i] * y[i];
  }
  return sum;
}

int tensor_is_power_of_two_i32(int32_t value) {
  return value > 0 && ((value & (value - 1)) == 0);
}

uint32_t tensor_bit_reverse_u32(uint32_t value, uint32_t bitCount) {
  uint32_t reversed = 0u;
  for (uint32_t i = 0; i < bitCount; ++i) {
    reversed = (reversed << 1u) | (value & 1u);
    value >>= 1u;
  }
  return reversed;
}

const uint32_t *tensor_fft_get_bitrev_table(int32_t n, uint32_t levels) {
  if (n <= 0) {
    return NULL;
  }
  if (g_tensorWorkspace.fftBitRev != NULL &&
      g_tensorWorkspace.fftBitRevN == n) {
    return g_tensorWorkspace.fftBitRev;
  }

  uint32_t *table = tensor_workspace_reserve_u32(
      &g_tensorWorkspace.fftBitRev, &g_tensorWorkspace.fftBitRevCapacity,
      (size_t)n);
  if (table == NULL) {
    return NULL;
  }

  for (uint32_t i = 0; i < (uint32_t)n; ++i) {
    table[i] = tensor_bit_reverse_u32(i, levels);
  }
  g_tensorWorkspace.fftBitRevN = n;
  return table;
}

TensorComplex tensor_complex_mul(TensorComplex a, TensorComplex b) {
  TensorComplex out;
  out.re = a.re * b.re - a.im * b.im;
  out.im = a.re * b.im + a.im * b.re;
  return out;
}

#if defined(__AVX2__)
static inline __m256 tensor_complex_mul_vec4(__m256 lhs, __m256 rhs) {
  const __m256 rhsRe = _mm256_moveldup_ps(rhs);
  const __m256 rhsIm = _mm256_movehdup_ps(rhs);
  const __m256 lhsSwap = _mm256_shuffle_ps(lhs, lhs, 0xB1);
  const __m256 prodRe = _mm256_mul_ps(lhs, rhsRe);
  const __m256 prodIm = _mm256_mul_ps(lhsSwap, rhsIm);
  return _mm256_addsub_ps(prodRe, prodIm);
}
#endif

void tensor_complex_pointwise_mul_inplace(TensorComplex *dst,
                                          const TensorComplex *rhs,
                                          int32_t count) {
  if (dst == NULL || rhs == NULL || count <= 0) {
    return;
  }

  int32_t i = 0;
#if defined(__AVX2__)
  for (; i + 4 <= count; i += 4) {
    const __m256 a = _mm256_loadu_ps((const float *)(dst + i));
    const __m256 b = _mm256_loadu_ps((const float *)(rhs + i));
    const __m256 out = tensor_complex_mul_vec4(a, b);
    _mm256_storeu_ps((float *)(dst + i), out);
  }
#endif
  for (; i < count; ++i) {
    dst[i] = tensor_complex_mul(dst[i], rhs[i]);
  }
}

int32_t tensor_log2_floor_i32(int32_t value) {
  int32_t bits = 0;
  while (value > 1) {
    value >>= 1;
    ++bits;
  }
  return bits;
}

const TensorComplex *tensor_fft_get_twiddle_table(int32_t n, int inverse) {
  if (n <= 1 || !tensor_is_power_of_two_i32(n)) {
    return NULL;
  }
  TensorComplex **slot = inverse ? &g_tensorWorkspace.fftTwiddleInverse
                                 : &g_tensorWorkspace.fftTwiddleForward;
  size_t *capacity = inverse ? &g_tensorWorkspace.fftTwiddleInverseCapacity
                             : &g_tensorWorkspace.fftTwiddleForwardCapacity;
  int32_t *cachedN = inverse ? &g_tensorWorkspace.fftTwiddleInverseN
                             : &g_tensorWorkspace.fftTwiddleForwardN;
  if (*slot != NULL && *cachedN == n) {
    return *slot;
  }

  // Sum_{k=1..log2(n)} n/2^k = n - 1 twiddles.
  const size_t twiddleCount = (size_t)n - 1u;
  TensorComplex *table =
      tensor_workspace_reserve_complex(slot, capacity, twiddleCount);
  if (table == NULL) {
    return NULL;
  }

  size_t offset = 0;
  for (int32_t mh = 1; mh < n; mh *= 2) {
    const int32_t m = mh * 2;
    const double thetaBase = (inverse ? -2.0 : 2.0) * M_PI / (double)m;
    for (int32_t j = 0; j < mh; ++j) {
      const double theta = thetaBase * (double)j;
      table[offset].re = (float)cos(theta);
      table[offset].im = (float)sin(theta);
      offset++;
    }
  }

  *cachedN = n;
  return table;
}

int tensor_fft_inplace(TensorComplex *data, int32_t n, int inverse) {
  if (data == NULL || n <= 0 || !tensor_is_power_of_two_i32(n)) {
    return 0;
  }
  if (n == 1) {
    return 1;
  }

  const int32_t levels = tensor_log2_floor_i32(n);
  const uint32_t *bitRev = tensor_fft_get_bitrev_table(n, (uint32_t)levels);
  if (bitRev == NULL) {
    return 0;
  }

  for (int32_t i = 0; i < n; ++i) {
    const int32_t rev = (int32_t)bitRev[i];
    if (i < rev) {
      const TensorComplex tmp = data[i];
      data[i] = data[rev];
      data[rev] = tmp;
    }
  }

  const TensorComplex *twiddles = tensor_fft_get_twiddle_table(n, inverse);
  if (twiddles == NULL) {
    return 0;
  }

  size_t offsetOffset = 0;
  for (int32_t mh = 1; mh < n; mh *= 2) {
    const int32_t m = mh * 2;
    const TensorComplex *stageTwiddles = twiddles + offsetOffset;
    offsetOffset += (size_t)mh;

    for (int32_t i = 0; i < n; i += m) {
      for (int32_t j = 0; j < mh; ++j) {
        const TensorComplex w = stageTwiddles[j];
        const TensorComplex u = data[i + j];
        const TensorComplex v = data[i + j + mh];
        const TensorComplex t = tensor_complex_mul(w, v);

        data[i + j].re = u.re + t.re;
        data[i + j].im = u.im + t.im;
        data[i + j + mh].re = u.re - t.re;
        data[i + j + mh].im = u.im - t.im;
      }
    }
  }

  if (inverse) {
    const float scale = 1.0f / (float)n;
    int32_t i = 0;
#if defined(__AVX2__)
    const __m256 vScale = _mm256_set1_ps(scale);
    for (; i + 4 <= n; i += 4) {
      const __m256 v = _mm256_loadu_ps((const float *)(data + i));
      const __m256 out = _mm256_mul_ps(v, vScale);
      _mm256_storeu_ps((float *)(data + i), out);
    }
#endif
    for (; i < n; ++i) {
      data[i].re *= scale;
      data[i].im *= scale;
    }
  }

  return 1;
}

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>

#if defined(__AVX2__) || defined(__SSE__)
#include <immintrin.h>
#endif

namespace {

void fillRandom(std::vector<float> &values, std::mt19937 &rng) {
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  for (float &v : values) {
    v = dist(rng);
  }
}

void fmaElementwise(const float *a, const float *b, const float *c, float *out,
                    int total) {
  int i = 0;
#if defined(__AVX2__)
  for (; i + 8 <= total; i += 8) {
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
  for (; i + 4 <= total; i += 4) {
    __m128 va = _mm_loadu_ps(a + i);
    __m128 vb = _mm_loadu_ps(b + i);
    __m128 vc = _mm_loadu_ps(c + i);
    __m128 vr = _mm_add_ps(_mm_mul_ps(va, vb), vc);
    _mm_storeu_ps(out + i, vr);
  }
#endif
  for (; i < total; ++i) {
    out[i] = a[i] * b[i] + c[i];
  }
}

void matmulBlocked(const float *a, const float *b, float *out, int n) {
  std::memset(out, 0, static_cast<std::size_t>(n) * static_cast<std::size_t>(n) *
                       sizeof(float));

  const int blockM = 32;
  const int blockN = 64;
  const int blockK = 64;

  for (int ii = 0; ii < n; ii += blockM) {
    const int iEnd = (ii + blockM < n) ? (ii + blockM) : n;
    for (int kk0 = 0; kk0 < n; kk0 += blockK) {
      const int kEnd = (kk0 + blockK < n) ? (kk0 + blockK) : n;
      for (int jj = 0; jj < n; jj += blockN) {
        const int jEnd = (jj + blockN < n) ? (jj + blockN) : n;
        for (int i = ii; i < iEnd; ++i) {
          float *outRow = out + static_cast<std::size_t>(i) * n;
          const float *aRow = a + static_cast<std::size_t>(i) * n;
          for (int kk = kk0; kk < kEnd; ++kk) {
            const float aVal = aRow[kk];
            const float *bRow = b + static_cast<std::size_t>(kk) * n;
            int j = jj;
#if defined(__AVX2__)
            const __m256 vA = _mm256_set1_ps(aVal);
            for (; j + 8 <= jEnd; j += 8) {
              __m256 vOut = _mm256_loadu_ps(outRow + j);
              __m256 vB = _mm256_loadu_ps(bRow + j);
#if defined(__FMA__)
              vOut = _mm256_fmadd_ps(vA, vB, vOut);
#else
              vOut = _mm256_add_ps(vOut, _mm256_mul_ps(vA, vB));
#endif
              _mm256_storeu_ps(outRow + j, vOut);
            }
#endif
#if defined(__SSE__)
            const __m128 vA4 = _mm_set1_ps(aVal);
            for (; j + 4 <= jEnd; j += 4) {
              __m128 vOut = _mm_loadu_ps(outRow + j);
              __m128 vB = _mm_loadu_ps(bRow + j);
              vOut = _mm_add_ps(vOut, _mm_mul_ps(vA4, vB));
              _mm_storeu_ps(outRow + j, vOut);
            }
#endif
            for (; j < jEnd; ++j) {
              outRow[j] += aVal * bRow[j];
            }
          }
        }
      }
    }
  }
}

int64_t runFmaBench(int size) {
  const std::size_t total = static_cast<std::size_t>(size) *
                            static_cast<std::size_t>(size);
  std::mt19937 rng(1337);
  std::vector<float> a(total);
  std::vector<float> b(total);
  std::vector<float> c(total);
  fillRandom(a, rng);
  fillRandom(b, rng);
  fillRandom(c, rng);

  std::vector<float> mix(total);
  std::vector<float> out(total, 0.0f);

  const auto start = std::chrono::steady_clock::now();
  fmaElementwise(a.data(), b.data(), c.data(), mix.data(), static_cast<int>(total));
  matmulBlocked(mix.data(), b.data(), out.data(), size);
  const auto end = std::chrono::steady_clock::now();

  volatile float sink = out[0];
  (void)sink;
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
      .count();
}

int64_t runMatMulBench(int size) {
  const std::size_t total = static_cast<std::size_t>(size) *
                            static_cast<std::size_t>(size);
  std::mt19937 rng(4242);
  std::vector<float> a(total);
  std::vector<float> b(total);
  fillRandom(a, rng);
  fillRandom(b, rng);

  std::vector<float> out(total, 0.0f);
  const auto start = std::chrono::steady_clock::now();
  matmulBlocked(a.data(), b.data(), out.data(), size);
  const auto end = std::chrono::steady_clock::now();

  volatile float sink = out[static_cast<std::size_t>(size)];
  (void)sink;
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
      .count();
}

} // namespace

int main() {
  const int64_t fmaMs = runFmaBench(1024);
  const int64_t matmulMs = runMatMulBench(1024);

  std::cout << "OPT_FMA_MS\n" << fmaMs << "\n";
  std::cout << "OPT_MATMUL_MS\n" << matmulMs << "\n";
  return 0;
}

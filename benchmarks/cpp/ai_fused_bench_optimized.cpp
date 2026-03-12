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

int64_t runFusedBench(int size) {
  const std::size_t total = static_cast<std::size_t>(size) *
                            static_cast<std::size_t>(size);
  std::mt19937 rng(1337);
  std::vector<float> a(total);
  std::vector<float> b(total);
  std::vector<float> residual(total);
  std::vector<float> bias(static_cast<std::size_t>(size));
  fillRandom(a, rng);
  fillRandom(b, rng);
  fillRandom(residual, rng);
  fillRandom(bias, rng);

  std::vector<float> out(total, 0.0f);

  const auto start = std::chrono::steady_clock::now();
  matmulBlocked(a.data(), b.data(), out.data(), size);
  for (int r = 0; r < size; ++r) {
    float *row = out.data() + static_cast<std::size_t>(r) * size;
    const float *resRow = residual.data() + static_cast<std::size_t>(r) * size;
    for (int c = 0; c < size; ++c) {
      row[c] += bias[static_cast<std::size_t>(c)] + resRow[c];
    }
  }
  const auto end = std::chrono::steady_clock::now();

  volatile float sink = out[0];
  (void)sink;
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
      .count();
}

} // namespace

int main() {
  const int64_t fusedMs = runFusedBench(1024);
  std::cout << "OPT_FUSED_MS\n" << fusedMs << "\n";
  return 0;
}

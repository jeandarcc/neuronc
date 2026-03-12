#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>

#if __has_include(<cblas.h>)
#include <cblas.h>
#elif __has_include(<openblas/cblas.h>)
#include <openblas/cblas.h>
#else
#error "CBLAS header not found (expected cblas.h or openblas/cblas.h)"
#endif

namespace {

constexpr int kSize = 1024;
constexpr int kRepeats = 6;

void fillRandom(std::vector<float> &values, std::mt19937 &rng, float lo,
                float hi) {
  std::uniform_real_distribution<float> dist(lo, hi);
  for (float &v : values) {
    v = dist(rng);
  }
}

std::vector<float> makeCirculant(const std::vector<float> &firstRow, int n) {
  std::vector<float> out(static_cast<std::size_t>(n) *
                         static_cast<std::size_t>(n));
  for (int r = 0; r < n; ++r) {
    for (int c = 0; c < n; ++c) {
      const int idx = (c - r + n) % n;
      out[static_cast<std::size_t>(r) * n + c] =
          firstRow[static_cast<std::size_t>(idx)];
    }
  }
  return out;
}

std::vector<float> makeToeplitz(const std::vector<float> &firstRow,
                                const std::vector<float> &firstCol, int n) {
  std::vector<float> out(static_cast<std::size_t>(n) *
                         static_cast<std::size_t>(n));
  for (int r = 0; r < n; ++r) {
    for (int c = 0; c < n; ++c) {
      out[static_cast<std::size_t>(r) * n + c] =
          (c >= r) ? firstRow[static_cast<std::size_t>(c - r)]
                   : firstCol[static_cast<std::size_t>(r - c)];
    }
  }
  return out;
}

std::vector<float> makeHybrid(const std::vector<float> &base, int n,
                              float density, float amplitude,
                              std::uint32_t seed) {
  std::vector<float> out = base;
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> noise(-amplitude, amplitude);
  std::uniform_int_distribution<int> indexDist(0, n - 1);
  const int total = n * n;
  const int perturbCount = static_cast<int>(density * static_cast<float>(total));
  for (int i = 0; i < perturbCount; ++i) {
    const int r = indexDist(rng);
    const int c = indexDist(rng);
    out[static_cast<std::size_t>(r) * n + c] += noise(rng);
  }
  return out;
}

template <typename Fn> double benchmarkMs(Fn &&fn, int repeats) {
  fn();
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < repeats; ++i) {
    fn();
  }
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(end - start).count() /
         static_cast<double>(repeats);
}

double benchMatMul(const std::vector<float> &a, const std::vector<float> &b,
                   std::vector<float> &out, int n) {
  return benchmarkMs(
      [&]() {
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, n, n, n, 1.0f,
                    a.data(), n, b.data(), n, 0.0f, out.data(), n);
      },
      kRepeats);
}

double benchFused(const std::vector<float> &a, const std::vector<float> &b,
                  const std::vector<float> &bias,
                  const std::vector<float> &residual, std::vector<float> &out,
                  int n) {
  return benchmarkMs(
      [&]() {
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, n, n, n, 1.0f,
                    a.data(), n, b.data(), n, 0.0f, out.data(), n);
        for (int r = 0; r < n; ++r) {
          float *row = out.data() + static_cast<std::size_t>(r) * n;
          const float *resRow =
              residual.data() + static_cast<std::size_t>(r) * n;
          for (int c = 0; c < n; ++c) {
            float v = row[c] + bias[static_cast<std::size_t>(c)] + resRow[c];
            row[c] = v < 0.0f ? 0.0f : v;
          }
        }
      },
      kRepeats);
}

long long roundMs(double value) {
  return static_cast<long long>(std::llround(value));
}

} // namespace

int main() {
  std::mt19937 rng(1337);
  std::vector<float> aData(static_cast<std::size_t>(kSize) * kSize);
  std::vector<float> firstRow(static_cast<std::size_t>(kSize));
  std::vector<float> firstCol(static_cast<std::size_t>(kSize));
  std::vector<float> biasData(static_cast<std::size_t>(kSize));
  std::vector<float> residualData(static_cast<std::size_t>(kSize) * kSize);
  fillRandom(aData, rng, -1.0f, 1.0f);
  fillRandom(firstRow, rng, -0.5f, 0.5f);
  fillRandom(firstCol, rng, -0.5f, 0.5f);
  firstCol[0] = firstRow[0];
  fillRandom(biasData, rng, -0.1f, 0.1f);
  fillRandom(residualData, rng, -0.2f, 0.2f);

  const std::vector<float> bCirc = makeCirculant(firstRow, kSize);
  const std::vector<float> bToep = makeToeplitz(firstRow, firstCol, kSize);
  const std::vector<float> bHybrid =
      makeHybrid(bCirc, kSize, 0.06f, 0.03f, 2026u);

  std::vector<float> out(static_cast<std::size_t>(kSize) * kSize, 0.0f);
  const auto circMatMulMs = benchMatMul(aData, bCirc, out, kSize);
  const auto toepMatMulMs = benchMatMul(aData, bToep, out, kSize);
  const auto hybridMatMulMs = benchMatMul(aData, bHybrid, out, kSize);

  const auto circFusedMs =
      benchFused(aData, bCirc, biasData, residualData, out, kSize);
  const auto toepFusedMs =
      benchFused(aData, bToep, biasData, residualData, out, kSize);
  const auto hybridFusedMs =
      benchFused(aData, bHybrid, biasData, residualData, out, kSize);

  std::cout << "BLAS_STRUCT_CIRC_MATMUL_MS\n" << roundMs(circMatMulMs) << "\n";
  std::cout << "BLAS_STRUCT_TOEP_MATMUL_MS\n" << roundMs(toepMatMulMs) << "\n";
  std::cout << "BLAS_STRUCT_HYBRID_MATMUL_MS\n" << roundMs(hybridMatMulMs)
            << "\n";
  std::cout << "BLAS_STRUCT_CIRC_FUSED_MS\n" << roundMs(circFusedMs) << "\n";
  std::cout << "BLAS_STRUCT_TOEP_FUSED_MS\n" << roundMs(toepFusedMs) << "\n";
  std::cout << "BLAS_STRUCT_HYBRID_FUSED_MS\n" << roundMs(hybridFusedMs)
            << "\n";
  return 0;
}

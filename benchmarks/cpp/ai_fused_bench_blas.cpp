#include <chrono>
#include <cstdint>
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

void fillRandom(std::vector<float> &values, std::mt19937 &rng) {
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  for (float &v : values) {
    v = dist(rng);
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
  cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, size, size, size, 1.0f,
              a.data(), size, b.data(), size, 0.0f, out.data(), size);
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
  std::cout << "BLAS_FUSED_MS\n" << fusedMs << "\n";
  return 0;
}

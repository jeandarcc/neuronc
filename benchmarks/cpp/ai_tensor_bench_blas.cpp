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
  for (std::size_t i = 0; i < total; ++i) {
    mix[i] = a[i] * b[i] + c[i];
  }
  cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, size, size, size, 1.0f,
              mix.data(), size, b.data(), size, 0.0f, out.data(), size);
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
  cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, size, size, size, 1.0f,
              a.data(), size, b.data(), size, 0.0f, out.data(), size);
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

  std::cout << "BLAS_FMA_MS\n" << fmaMs << "\n";
  std::cout << "BLAS_MATMUL_MS\n" << matmulMs << "\n";
  return 0;
}
